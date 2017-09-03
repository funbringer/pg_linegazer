CREATE OR REPLACE FUNCTION linegazer_clear()
RETURNS VOID AS 'pg_linegazer', 'linegazer_clear_pl'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION linegazer_simple_report(
	procid		REGPROCEDURE,
	OUT line	INT4,
	OUT usage	INT4,
	OUT code	TEXT)
RETURNS RECORD AS 'pg_linegazer', 'linegazer_simple_report_pl'
LANGUAGE C STRICT;
