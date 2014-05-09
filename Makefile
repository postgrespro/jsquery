# contrib/jsquery/Makefile

MODULE_big = jsquery
OBJS = jsquery_io.o jsquery_gram.o jsquery_op.o \
	   jsquery_constr.o jsquery_extract.o jsonb_gin_ops.o

EXTENSION = jsquery
DATA = jsquery--1.0.sql

REGRESS = jsquery

EXTRA_CLEAN = y.tab.c y.tab.h \
				jsquery_gram.c jsquery_scan.c jsquery_gram.h

ifdef USE_PGXS
PG_CONFIG = pg_config
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

