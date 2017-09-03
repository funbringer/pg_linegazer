CREATE OR REPLACE FUNCTION linegazer_clear()
RETURNS VOID
AS 'pg_linegazer', 'linegazer_clear_pl'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION linegazer_simple_report(procid REGPROCEDURE)
RETURNS TABLE (line INT4, usage INT4, code TEXT)
AS 'pg_linegazer', 'linegazer_simple_report_pl'
LANGUAGE C STRICT;
