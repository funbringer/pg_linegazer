CREATE OR REPLACE FUNCTION linegazer_clear()
RETURNS VOID
AS 'pg_linegazer', 'linegazer_clear_pl'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION linegazer_simple_report(procid REGPROCEDURE)
RETURNS TABLE (line INT4, hits INT4, code TEXT)
AS 'pg_linegazer', 'linegazer_simple_report_pl'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION linegazer_simple_coverage(procid REGPROCEDURE)
RETURNS FLOAT8
AS $$
	WITH stats AS (SELECT * FROM linegazer_simple_report(procid))
	SELECT (SELECT count(hits) FROM stats WHERE hits > 0)::float8 /
		   (SELECT count(hits) FROM stats)::float8;
$$ LANGUAGE SQL STRICT;
