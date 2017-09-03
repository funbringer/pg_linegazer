## [WIP] pg_linegazer
Transparent code coverage for PL/pgSQL

This project aims to provide a code coverage toolkit for PL/pgSQL, a procedural programming language supported by PostgreSQL. At the moment, pg_linegazer is in the alpha stage of development. It might be buggy, so bear with it :)

## Features

* No need for code instrumentation;
* Nested calls are taken into account;
* Compatible with PostgreSQL 9.5+;
* Well, that's it for now :|

## Setup

Execute the following command in your favorite shell: 

```bash
make USE_PGXS=1 install
```

Don't forget to set `PG_CONFIG` if you'd like to install pg_linegazer into a custom build of PostgreSQL:

```bash
make USE_PGXS=1 PG_CONFIG=/path/to/pg_config install
```

## Usage

Just execute your function a few times, then request a report:

```plpgsql
/* Step #1: define a function */
CREATE OR REPLACE FUNCTION test_func()
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

/* Step #2: execute it 2 times */
select test_func();
select test_func();

/* Step #3: examine the report */
select * from linegazer_simple_report('test_func'::regproc);
 line | hits |                  code
------+------+-----------------------------------------
    1 |      |
    2 |      | declare
    3 |      |         a int4 := 0;
    4 |      |         i int4;
    5 |      |
    6 |      | begin
    7 |    2 |         for i in 1..100 loop
    8 |  200 |                 a = a + i;
    9 |      |         end loop;
   10 |      |
   11 |    2 |         if i < 10 then
   12 |    0 |                 raise exception 'wtf!';
   13 |      |         end if;
   14 |      | end;
(14 rows)
```

To reset coverage stats, execute the following function:

```plpgsql
/* Clear cache */
select linegazer_clear();
 linegazer_clear
-----------------

(1 row)

/* All stats is lost! */
select * from linegazer_simple_report('test_func'::regproc);
 line | hits |                  code
------+------+-----------------------------------------
    1 |    0 |
    2 |    0 | declare
    3 |    0 |         a int4 := 0;
    4 |    0 |         i int4;
    5 |    0 |
    6 |    0 | begin
    7 |    0 |         for i in 1..100 loop
    8 |    0 |                 a = a + i;
    9 |    0 |         end loop;
   10 |    0 |
   11 |    0 |         if i < 10 then
   12 |    0 |                 raise exception 'wtf!';
   13 |    0 |         end if;
   14 |    0 | end;
(14 rows)
```
