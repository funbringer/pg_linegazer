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


CREATE OR REPLACE FUNCTION test_6()
RETURNS VOID AS
$$
declare
	a int4;
	i int4;

begin
	for i in (select generate_series(1, 4)) loop
		a = a + i;

		if a > 100 then
			raise exception 'wtf!';
		end if;
	end loop;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_7()
RETURNS VOID AS
$$
declare
	a int4;
	i int4;

begin
	foreach i in array '{1,2,3,4}'::int4[] loop
		a = a + i;

		if a > 100 then
			raise exception 'wtf!';
		end if;
	end loop;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_8()
RETURNS VOID AS
$$
declare
	a int4;
	i int4;

begin
	for i in select * from generate_series(1, 2) loop
		a = a + i;

		if a > 100 then
			raise exception 'wtf!';
		end if;
	end loop;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_9()
RETURNS VOID AS
$$
declare
	a int4 := 0;

begin
	loop
		if a > 10 then
			exit;
		end if;

		if a > 100 then
			raise exception 'wtf!';
		end if;

		a = a + 1;
	end loop;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_10()
RETURNS VOID AS
$$
declare
	a int4 := 0;

begin
	while a < 10 loop

		if a > 100 then
			raise exception 'wtf!';
		elseif a > 1000 then
			raise exception 'wtf!';
		else
			a = a;
		end if;

		a = a + 1;
	end loop;
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_11()
RETURNS VOID AS
$$
declare
	a int4 := 0;

begin
	a = 100;

	if a = 200 then
		raise exception 'wtf!';
	end if;

exception
	when division_by_zero then
		raise exception 'wtf!';
end;
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION test_12()
RETURNS VOID AS
$$
declare
	a int4;
	i int4;

begin
	for i in 1..10 loop
		case i
			when 1, 2, 3 then
				a = 1;
			when 4 then
				a = 2;
			when 100 then
				raise exception 'wtf!';
			else
				a = 3;
		end case;
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


/* Test function #6 */
select test_6();
select * from linegazer_simple_report('test_6'::regproc);
select test_6();
select * from linegazer_simple_report('test_6'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_6'::regproc);


/* Test function #7 */
select test_7();
select * from linegazer_simple_report('test_7'::regproc);
select test_7();
select * from linegazer_simple_report('test_7'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_7'::regproc);


/* Test function #8 */
select test_8();
select * from linegazer_simple_report('test_8'::regproc);
select test_8();
select * from linegazer_simple_report('test_8'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_8'::regproc);


/* Test function #9 */
select test_9();
select * from linegazer_simple_report('test_9'::regproc);
select test_9();
select * from linegazer_simple_report('test_9'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_9'::regproc);


/* Test function #10 */
select test_10();
select * from linegazer_simple_report('test_10'::regproc);
select test_10();
select * from linegazer_simple_report('test_10'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_10'::regproc);


/* Test function #11 */
select test_11();
select * from linegazer_simple_report('test_11'::regproc);
select test_11();
select * from linegazer_simple_report('test_11'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_11'::regproc);


/* Test function #12 */
select test_12();
select * from linegazer_simple_report('test_12'::regproc);
select test_12();
select * from linegazer_simple_report('test_12'::regproc);
select linegazer_clear();
select * from linegazer_simple_report('test_12'::regproc);


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
