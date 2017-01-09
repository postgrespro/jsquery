# contrib/jsquery/Makefile

MODULE_big = jsquery
OBJS = jsonb_gin_ops.o jsquery_constr.o jsquery_extract.o \
	jsquery_gram.o jsquery_io.o jsquery_op.o jsquery_selfuncs.o \
	jsquery_support.o

EXTENSION = jsquery
DATA = jsquery--1.0.sql

REGRESS = jsquery jsquery_stats
# We need a UTF8 database
ENCODING = UTF8

EXTRA_CLEAN = y.tab.c y.tab.h \
				jsquery_gram.c jsquery_scan.c jsquery_gram.h

ifdef USE_PGXS
PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/jsquery
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

jsquery_gram.o: jsquery_scan.c

jsquery_gram.c: BISONFLAGS += -d

distprep: jsquery_gram.c jsquery_scan.c

maintainer-clean:
	rm -f jsquery_gram.c jsquery_scan.c jsquery_gram.h

