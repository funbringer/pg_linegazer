#include "postgres.h"
#include "fmgr.h"
#include "plpgsql.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"


#define MAX_RECORDED_CNT	9999
#define FUNC_TABLE_SIZE		128
#define FUNC_ROW_COUNT		4


PG_MODULE_MAGIC;


PG_FUNCTION_INFO_V1(linegazer_clear_pl);
PG_FUNCTION_INFO_V1(linegazer_simple_report_pl);


typedef struct FuncTableEntry
{
	Oid		func_id;
	char   *func_name;

	int		last_stmt,
			rows_alloced;
	int	   *rows;
} FuncTableEntry;


void _PG_init(void);
void linegazer_stmt_begin(PLpgSQL_execstate *estate,
						  PLpgSQL_stmt *stmt);
void linegazer_func_begin(PLpgSQL_execstate *estate,
						  PLpgSQL_function *func);


static PLpgSQL_plugin	linegazer_plpgsql_plugin;
static HTAB			   *linegazer_func_table = NULL;


/* calculate ceil(log base 2) of num */
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

/* calculate first power of 2 >= num, bounded to what will fit in an int */
static int
next_pow2_int(long num)
{
	if (num > INT_MAX / 2)
		num = INT_MAX / 2;
	return 1 << my_log2(num);
}


static inline void
checked_increment(int *val)
{
	*val = Min(*val + 1, MAX_RECORDED_CNT);
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
linegazer_func_begin(PLpgSQL_execstate *estate,
					 PLpgSQL_function *func)
{
	if (!linegazer_func_table)
	{
		HASHCTL hashctl;

		memset(&hashctl, 0, sizeof(HASHCTL));
		hashctl.keysize		= sizeof(Oid);
		hashctl.entrysize	= sizeof(FuncTableEntry);
		hashctl.hcxt		= TopMemoryContext;

		linegazer_func_table = hash_create("pg_linegazer function table",
										   FUNC_TABLE_SIZE, &hashctl,
										   HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
	}

	if (OidIsValid(func->fn_oid) && !estate->plugin_info)
	{
		bool			found;
		FuncTableEntry *fte = hash_search(linegazer_func_table,
										  (void *) &func->fn_oid,
										  HASH_ENTER, &found);

		if (!found)
		{
			/* Initialize stats */
			fte->last_stmt = 0;
			fte->rows_alloced = FUNC_ROW_COUNT;
			fte->rows = MemoryContextAllocZero(TopMemoryContext,
											   sizeof(int) * fte->rows_alloced);

			/* Fetch function's name */
			fte->func_name =
				DatumGetPointer(DirectFunctionCall1(regprocedureout,
													ObjectIdGetDatum(fte->func_id)));
		}

		estate->plugin_info = (void *) fte;
	}
}

void
linegazer_stmt_begin(PLpgSQL_execstate *estate,
					 PLpgSQL_stmt *stmt)
{
	FuncTableEntry	   *fte = (FuncTableEntry *) estate->plugin_info;
	Oid					procid = estate->func->fn_oid;

#ifdef USE_ASSERT_CHECKING
	elog(DEBUG1, "linegazer: %s, line %d", fte->func_name, stmt->lineno);
#endif

	/* Update last known statement */
	fte->last_stmt = Max(fte->last_stmt, stmt->lineno);

	/* Extend array if needed */
	if (fte->last_stmt >= fte->rows_alloced)
	{
		int		old_size = fte->rows_alloced,
				new_size = next_pow2_int(fte->last_stmt + 1),
				new_rows = new_size - fte->rows_alloced;

		fte->rows_alloced = new_size;
		fte->rows = repalloc(fte->rows, sizeof(int) * new_size);

		/* Don't forget to reset new cells! */
		memset(&fte->rows[old_size], 0, sizeof(int) * new_rows);

#ifdef USE_ASSERT_CHECKING
	elog(DEBUG1,
		 "linegazer: function %u, old_size %d, new_size %d",
		 procid, old_size, new_size);
#endif
	}

	/* Increment row usage count */
	checked_increment(&fte->rows[fte->last_stmt]);
}



Datum
linegazer_clear_pl(PG_FUNCTION_ARGS)
{
	elog(WARNING, "%s not implemented", __FUNCTION__);

	PG_RETURN_VOID();
}

Datum
linegazer_simple_report_pl(PG_FUNCTION_ARGS)
{
	elog(WARNING, "%s not implemented", __FUNCTION__);

	PG_RETURN_NULL();
}
