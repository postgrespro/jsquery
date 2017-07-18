# contrib/jsquery/Makefile

MODULE_big = jsquery
OBJS = jsonb_gin_ops.o jsquery_constr.o jsquery_extract.o \
	jsquery_gram.o jsquery_io.o jsquery_op.o jsquery_support.o \
	monq_gram.o monq_scan.o monq_get_jsquery.o monq_create_query.o monq_delete_query.o

EXTENSION = jsquery
DATA = jsquery--1.0.1.sql
INCLUDES = jsquery.h monq.h 

REGRESS = jsquery
# We need a UTF8 database
ENCODING = UTF8

EXTRA_CLEAN = y.tab.c y.tab.h \
				jsquery_gram.c jsquery_scan.c jsquery_gram.h \
				monq_gram.c monq_scan.c monq_gram.h

PG_CONFIG ?= pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

jsquery_gram.o: jsquery_scan.c

jsquery_gram.c: BISONFLAGS += -d

monq_gram.o: monq_scan.c

monq_gram.c: BISONFLAGS += -d

distprep: jsquery_gram.c jsquery_scan.c monq_gram.c monq_scan.c

maintainer-clean:
	rm -f jsquery_gram.c jsquery_scan.c jsquery_gram.h monq_gram.c monq_scan.c monq_gram.h

install: installincludes

installincludes:
	$(INSTALL_DATA) $(addprefix $(srcdir)/, $(INCLUDES)) '$(DESTDIR)$(includedir_server)/'