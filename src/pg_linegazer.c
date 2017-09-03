#include "postgres.h"
#include "access/htup_details.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "fmgr.h"
#include "funcapi.h"
#include "plpgsql.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/syscache.h"


#define DEAD_STMT_MARK		(-1)
#define MAX_RECORDED_CNT	9999
#define FUNC_TABLE_SIZE		128
#define FUNC_ROW_COUNT		4

#if PG_VERSION_NUM >= 90500 && PG_VERSION_NUM < 90600
#define ALLOCSET_DEFAULT_SIZES \
	ALLOCSET_DEFAULT_MINSIZE, ALLOCSET_DEFAULT_INITSIZE, ALLOCSET_DEFAULT_MAXSIZE

#define ALLOCSET_SMALL_SIZES \
	ALLOCSET_SMALL_MINSIZE, ALLOCSET_SMALL_INITSIZE, ALLOCSET_SMALL_MAXSIZE
#endif


PG_MODULE_MAGIC;


PG_FUNCTION_INFO_V1(linegazer_clear_pl);
PG_FUNCTION_INFO_V1(linegazer_simple_report_pl);


typedef struct SimpleReportCxt
{
	List	   *stats;	/* hits + code */
	ListCell   *lc;		/* iterator */
	int			line;
} SimpleReportCxt;

typedef struct FuncTableEntry
{
	Oid		func_id;
	char   *func_name;

	int		last_line,
			lines_alloced;
	int	   *lines_stat; /* counter */
} FuncTableEntry;


void _PG_init(void);

void init_fte_htab(void);
void fini_fte_htab(void);
void *search_fte_htab(Oid procid, HASHACTION action, bool *found);
MemoryContext get_fte_mcxt(void);

void mark_branch_statements(PLpgSQL_stmt *stmt,
							FuncTableEntry *fte);

void fte_expand_lines_stat(FuncTableEntry *fte, int lineno);

void linegazer_stmt_begin(PLpgSQL_execstate *estate,
						  PLpgSQL_stmt *stmt);
void linegazer_func_begin(PLpgSQL_execstate *estate,
						  PLpgSQL_function *func);


static PLpgSQL_plugin	linegazer_plpgsql_plugin;
static HTAB			   *linegazer_func_table = NULL;
static MemoryContext	linegazer_func_table_mcxt = NULL;


static int
my_log2(long num)
{
	int			i;
	long		limit;

	/* guard against too-large input, which would put us into infinite loop */
	if (num > LONG_MAX / 2)
		num = LONG_MAX / 2;

	for (i = 0, limit = 1; limit < num; i++, limit <<= 1)
		;
	return i;
}

static int
next_pow2_int(long num)
{
	if (num > INT_MAX / 2)
		num = INT_MAX / 2;
	return 1 << my_log2(num);
}

static char *
strtok_single(char *str, char const *delims)
{
	static char	   *src = NULL;
	char		   *p,
				   *ret = 0;

	if (str != NULL)
		src = str;

	if (src == NULL)
		return NULL;

	if ((p = strpbrk(src, delims)) != NULL)
	{
		*p  = 0;
		ret = src;
		src = ++p;
	}
	else if (*src)
	{
		ret = src;
		src = NULL;
	}

	return ret;
}


/* Mark line as "potentially dead" */
static inline void
fte_checked_penalty(int *val)
{
	/* Reached some statement within a branch */
	if (*val == 0)
		*val = DEAD_STMT_MARK; /* mark as potentially dead */
}

/* Increment hit count of a line of code */
static inline void
fte_checked_increment(int *val)
{
	/* Reached some statement within a branch */
	if (*val == DEAD_STMT_MARK)
		*val = 0; /* revive this statement */

	*val = Min(*val + 1, MAX_RECORDED_CNT);
}

/* Get hit count for a line of code */
static inline int
fte_checked_hit_poll(FuncTableEntry *fce, int line)
{
	if (!fce)
		return DEAD_STMT_MARK;

	/* We don't know anything about this statement */
	if (line > fce->last_line)
		return 0;

	return fce->lines_stat[line];
}


void
_PG_init(void)
{
	PLpgSQL_plugin **plpgsql_plugin_ptr;

	/* Initialize plugin structure */
	memset((void *) &linegazer_plpgsql_plugin, 0, sizeof(PLpgSQL_plugin));
	linegazer_plpgsql_plugin.stmt_beg = linegazer_stmt_begin;
	linegazer_plpgsql_plugin.func_beg = linegazer_func_begin;

	/* Let PL/pgSQL know about our custom plugin */
	plpgsql_plugin_ptr = (PLpgSQL_plugin **) find_rendezvous_variable("PLpgSQL_plugin");
	*plpgsql_plugin_ptr = &linegazer_plpgsql_plugin;
}

void
init_fte_htab(void)
{
	HASHCTL hashctl;

	Assert(linegazer_func_table == NULL);

	memset(&hashctl, 0, sizeof(HASHCTL));
	hashctl.keysize		= sizeof(Oid);
	hashctl.entrysize	= sizeof(FuncTableEntry);
	hashctl.hcxt		= get_fte_mcxt();

	linegazer_func_table = hash_create("pg_linegazer function table",
									   FUNC_TABLE_SIZE, &hashctl,
									   HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
}

void
fini_fte_htab(void)
{
	hash_destroy(linegazer_func_table);
	linegazer_func_table = NULL;

	if (linegazer_func_table_mcxt)
		MemoryContextReset(linegazer_func_table_mcxt);
}

/* Find an entry in function hash table */
void *
search_fte_htab(Oid procid, HASHACTION action, bool *found)
{
	if (!linegazer_func_table)
		init_fte_htab();

	return hash_search(linegazer_func_table,
					   (void *) &procid,
					   action, found);
}

/* Get a suitable memory context for function hash table */
MemoryContext
get_fte_mcxt(void)
{
	linegazer_func_table_mcxt = AllocSetContextCreate(TopMemoryContext,
													  "pg_linegazer memory context",
													  ALLOCSET_DEFAULT_SIZES);

	return linegazer_func_table_mcxt;
}


/* Recursively mark all reachable stmts as "potentially dead" */
void
mark_branch_statements(PLpgSQL_stmt *stmt,
					   FuncTableEntry *fte)
{
	ListCell   *lc1,
			   *lc2;

	switch (stmt->cmd_type)
	{
		case PLPGSQL_STMT_BLOCK:
			{
				PLpgSQL_stmt_block *block = (PLpgSQL_stmt_block *) stmt;

				foreach (lc1, block->body)
					mark_branch_statements(lfirst(lc1), fte);

				/* Handle exceptions as well */
				if (block->exceptions)
				{
					foreach (lc1, block->exceptions->exc_list)
					{
						PLpgSQL_exception *exception = lfirst(lc1);

						foreach (lc2, exception->action)
							mark_branch_statements(lfirst(lc2), fte);
					}
				}
			}
			return; /* do not mark blocks (etc BEGIN) */

		/* IF cond THEN cmd ELSE IF cond THEN cmd ELSE cmd END IF */
		case PLPGSQL_STMT_IF:
			{
				PLpgSQL_stmt_if *if_stmt = (PLpgSQL_stmt_if *) stmt;

				foreach (lc1, if_stmt->then_body)
					mark_branch_statements(lfirst(lc1), fte);

				foreach (lc1, if_stmt->elsif_list)
				{
					PLpgSQL_if_elsif *elsif = lfirst(lc1);

					foreach (lc2, elsif->stmts)
						mark_branch_statements(lfirst(lc2), fte);
				}

				foreach (lc1, if_stmt->else_body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* CASE expr WHEN expr THEN cmd ELSE cmd END CASE */
		case PLPGSQL_STMT_CASE:
			{
				PLpgSQL_stmt_case *case_stmt = (PLpgSQL_stmt_case *) stmt;

				foreach (lc1, case_stmt->case_when_list)
				{
					PLpgSQL_case_when *casewhen = lfirst(lc1);

					foreach (lc2, casewhen->stmts)
						mark_branch_statements(lfirst(lc1), fte);
				}

				foreach (lc1, case_stmt->else_stmts)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* LOOP cmd END LOOP */
		case PLPGSQL_STMT_LOOP:
			{
				PLpgSQL_stmt_loop *loop = (PLpgSQL_stmt_loop *) stmt;

				foreach (lc1, loop->body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* WHILE cond LOOP stmt END LOOP */
		case PLPGSQL_STMT_WHILE:
			{
				PLpgSQL_stmt_while *while_stmt = (PLpgSQL_stmt_while *) stmt;

				foreach (lc1, while_stmt->body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* FOR i IN expr .. expr BY expr LOOP cmd END LOOP */
		case PLPGSQL_STMT_FORI:
			{
				PLpgSQL_stmt_fori *fori_stmt = (PLpgSQL_stmt_fori *) stmt;

				foreach (lc1, fori_stmt->body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* FOR i IN (SELECT ...) LOOP cmd END LOOP */
		case PLPGSQL_STMT_FORS:
			{
				PLpgSQL_stmt_fors *fors_stmt = (PLpgSQL_stmt_fors *) stmt;

				foreach (lc1, fors_stmt->body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* FOR i IN query LOOP cmd END LOOP */
		case PLPGSQL_STMT_FORC:
			{
				PLpgSQL_stmt_forc *forc_stmt = (PLpgSQL_stmt_forc *) stmt;

				foreach (lc1, forc_stmt->body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* FOR i IN ARRAY LOOP cmd END LOOP */
		case PLPGSQL_STMT_FOREACH_A:
			{
				PLpgSQL_stmt_foreach_a *foreach_stmt = (PLpgSQL_stmt_foreach_a *) stmt;

				foreach (lc1, foreach_stmt->body)
					mark_branch_statements(lfirst(lc1), fte);
			}
			break;

		/* FOR i IN EXECUTE LOOP cmd END LOOP */
		case PLPGSQL_STMT_DYNFORS:
			{
				PLpgSQL_stmt_dynfors *dynfors = (PLpgSQL_stmt_dynfors *) stmt;

				foreach (lc1, dynfors->body)
					mark_branch_statements(lfirst(lc1), fte);
			}

		default:
			break;
	}

	/* Expand fte->lines_stat if necessary */
	fte_expand_lines_stat(fte, stmt->lineno);

	/* Mark this statement as "probably unreachable" */
	fte_checked_penalty(&fte->lines_stat[stmt->lineno]);
}

/* Allocate more lines in FuncTableEntry */
void
fte_expand_lines_stat(FuncTableEntry *fte, int lineno)
{
	/* Update last known statement */
	fte->last_line = Max(fte->last_line, lineno);

	/* Extend array if needed */
	if (fte->last_line >= fte->lines_alloced)
	{
		int		old_size = fte->lines_alloced,
				new_size = next_pow2_int(fte->last_line + 1),
				new_rows = new_size - fte->lines_alloced;

		fte->lines_alloced = new_size;
		fte->lines_stat = repalloc(fte->lines_stat, sizeof(int) * new_size);

		/* Don't forget to reset new cells! */
		memset(&fte->lines_stat[old_size], 0, sizeof(int) * new_rows);
	}
}


void
linegazer_func_begin(PLpgSQL_execstate *estate,
					 PLpgSQL_function *func)
{
	if (OidIsValid(func->fn_oid) && !estate->plugin_info)
	{
		bool			found;
		FuncTableEntry *fte = search_fte_htab(func->fn_oid, HASH_ENTER, &found);

		if (!found)
		{
			/* Initialize stats */
			fte->last_line = 0;
			fte->lines_alloced = FUNC_ROW_COUNT;
			fte->lines_stat = MemoryContextAllocZero(get_fte_mcxt(),
													 sizeof(int) * fte->lines_alloced);

			/* Fetch function's name */
			fte->func_name =
				DatumGetPointer(DirectFunctionCall1(regprocedureout,
													ObjectIdGetDatum(fte->func_id)));
		}

		estate->plugin_info = (void *) fte;

		/* Find "potentially dead" statements */
		mark_branch_statements((PLpgSQL_stmt *) func->action, fte);
	}
}

void
linegazer_stmt_begin(PLpgSQL_execstate *estate,
					 PLpgSQL_stmt *stmt)
{
	FuncTableEntry *fte = (FuncTableEntry *) estate->plugin_info;

#ifdef USE_ASSERT_CHECKING
	elog(DEBUG1, "linegazer: %s, line %d", fte->func_name, stmt->lineno);
#endif

	/* Expand fte->lines_stat if necessary */
	fte_expand_lines_stat(fte, stmt->lineno);

	/* Increment row hits count */
	fte_checked_increment(&fte->lines_stat[stmt->lineno]);
}


/* Clear function hash table */
Datum
linegazer_clear_pl(PG_FUNCTION_ARGS)
{
	fini_fte_htab();

	PG_RETURN_NULL();
}

/* Build a simple report for a certain function */
Datum
linegazer_simple_report_pl(PG_FUNCTION_ARGS)
{
	FuncCallContext	   *funccxt;
	SimpleReportCxt	   *usercxt;
	Oid					proc_id = PG_GETARG_OID(0);

	/* Initialize tuple descriptor & function call context */
	if (SRF_IS_FIRSTCALL())
	{
		TupleDesc		tupdesc;
		MemoryContext	old_mcxt;
		FuncTableEntry *fte;
		HeapTuple		htup;
		Datum			value;
		bool			isnull;
		char		   *proc_src;
		char		   *ptr;
		int				i;

		funccxt = SRF_FIRSTCALL_INIT();

		old_mcxt = MemoryContextSwitchTo(funccxt->multi_call_memory_ctx);


		fte = search_fte_htab(proc_id, HASH_FIND, NULL);


		htup = SearchSysCache1(PROCOID, ObjectIdGetDatum(proc_id));
		if (!HeapTupleIsValid(htup))
			elog(ERROR, "wrong function %u", proc_id);

		value = SysCacheGetAttr(PROCOID, htup, Anum_pg_proc_prosrc, &isnull);
		if (isnull)
			elog(ERROR, "wrong function %u", proc_id);

		proc_src = TextDatumGetCString(value);

		ReleaseSysCache(htup);


		usercxt = palloc0(sizeof(SimpleReportCxt));
		ptr = strtok_single(proc_src, "\n");

		i = 1;
		while (ptr)
		{
			int hits = fte_checked_hit_poll(fte, i);

			/* Append (hit count, line of code) */
			usercxt->stats = lappend(usercxt->stats,
									 list_make2(makeInteger(hits),
												makeString(ptr)));

			ptr = strtok_single(NULL, "\n");
			i++;
		}


		/* Create tuple descriptor */
		tupdesc = CreateTemplateTupleDesc(3, false);

		TupleDescInitEntry(tupdesc, 1,
						   "line", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, 2,
						   "hits", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, 3,
						   "code", TEXTOID, -1, 0);

		funccxt->tuple_desc = BlessTupleDesc(tupdesc);
		funccxt->user_fctx = (void *) usercxt;


		MemoryContextSwitchTo(old_mcxt);

		usercxt->line = 1;
		usercxt->lc = list_head(usercxt->stats);
	}

	/* Restore per-call context */
	funccxt = SRF_PERCALL_SETUP();
	usercxt = (SimpleReportCxt *) funccxt->user_fctx;

	/* Are there any lines? */
	if (usercxt->lc)
	{
		List	   *pair = (List *) lfirst(usercxt->lc);
		int			hits = intVal(linitial(pair));
		Datum		values[3];
		bool		isnull[3] = {0};
		HeapTuple	htup;

		/* Special rule for dead code */
		if (hits == DEAD_STMT_MARK)
			hits = 0;
		/* Special rule for rubbish lines */
		else if (hits == 0)
			isnull[1] = true;

		values[0] = Int32GetDatum(usercxt->line++);
		values[1] = Int32GetDatum(hits);
		values[2] = CStringGetTextDatum(strVal(lsecond(pair)));

		htup = heap_form_tuple(funccxt->tuple_desc, values, isnull);

		/* Switch to next cell before return */
		usercxt->lc = lnext(usercxt->lc);

		SRF_RETURN_NEXT(funccxt, HeapTupleGetDatum(htup));
	}

	SRF_RETURN_DONE(funccxt);
}
