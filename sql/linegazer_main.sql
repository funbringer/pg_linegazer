\set VERBOSITY terse
SET search_path = 'public';

CREATE EXTENSION pg_linegazer;



/* Check htab drop */
select linegazer_clear();
select linegazer_clear();
select linegazer_clear();

/* Check wrong args */
select * from linegazer_simple_report(0);
select * from linegazer_simple_report(1);


CREATE OR REPLACE FUNCTION test_1()
RETURNS VOID AS
$$
declare a int4; begin a = 1; a = a + 2; a = a + 3; end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_2()
RETURNS VOID AS
$$
declare
	a int4;

begin
	a = 1;

	a = 2;

	a = 3;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_3()
RETURNS VOID AS
$$
declare
	a int4 := 0;
	i int4;

begin
	for i in 1..100 loop
		a = a + i;
	end loop;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_4()
RETURNS VOID AS
$$
declare
	a int4 := 0;
	i int4;

begin
	for i in 1..100 loop
		a = a + i;
	end loop;

	if i < 10 then
		raise exception 'wtf!';
	end if;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_5()
RETURNS VOID AS
$$
declare
	i int4;

begin
	for i in 1..4 loop
		perform test_2();

		if i % 2 = 0 then
			perform test_3();
		end if;
	end loop;
end;
$$ LANGUAGE plpgsql;



/* Test function #1 */
select test_1();
select * from linegazer_simple_report('test_1'::regproc);
select test_1();
select * from linegazer_simple_report('test_1'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_1'::regproc);


/* Test function #2 */
select test_2();
select * from linegazer_simple_report('test_2'::regproc);
select test_2();
select * from linegazer_simple_report('test_2'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_2'::regproc);


/* Test function #3 */
select test_3();
select * from linegazer_simple_report('test_3'::regproc);
select test_3();
select * from linegazer_simple_report('test_3'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_3'::regproc);


/* Test function #4 */
select test_4();
select * from linegazer_simple_report('test_4'::regproc);
select test_4();
select * from linegazer_simple_report('test_4'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_4'::regproc);


/* Test function #5 */
select test_5();
select * from linegazer_simple_report('test_5'::regproc);
select * from linegazer_simple_report('test_2'::regproc);
select * from linegazer_simple_report('test_3'::regproc);
select test_5();
select * from linegazer_simple_report('test_5'::regproc);
select * from linegazer_simple_report('test_2'::regproc);
select * from linegazer_simple_report('test_3'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_5'::regproc);
select * from linegazer_simple_report('test_2'::regproc);
select * from linegazer_simple_report('test_3'::regproc);


/* Test native function */
select generate_series(1::int4, 1::int4);
select * from linegazer_simple_report('generate_series(int4,int4)'::regprocedure);
select linegazer_clear();


/* Test isolation */
select test_1();
select * from linegazer_simple_report('test_1'::regproc);
select test_2();
select * from linegazer_simple_report('test_1'::regproc);
select * from linegazer_simple_report('test_2'::regproc);
select test_3();
select * from linegazer_simple_report('test_1'::regproc);
select * from linegazer_simple_report('test_2'::regproc);
select * from linegazer_simple_report('test_3'::regproc);
select linegazer_clear();



DROP EXTENSION pg_linegazer;
