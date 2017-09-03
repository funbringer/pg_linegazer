MODULE_big = pg_linegazer

OBJS = src/pg_linegazer.o $(WIN32RES)

EXTENSION = pg_linegazer

EXTVERSION = 1.0

PGFILEDESC = "pg_linegazer - transparent code coverage for PL/pgSQL"

DATA = pg_linegazer--1.0.sql

REGRESS = linegazer_main

EXTRA_REGRESS_OPTS=--temp-config=$(top_srcdir)/$(subdir)/conf.add

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_linegazer
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
