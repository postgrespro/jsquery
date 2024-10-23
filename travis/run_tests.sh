#!/usr/bin/env bash

#
# Copyright (c) 2024, Postgres Professional
#
# supported levels:
#		* standard
#

set -ev
status=0

# show pg_config just in case
pg_config

# build and install extension (using PG_CPPFLAGS and SHLIB_LINK for gcov)
make USE_PGXS=1 PG_CPPFLAGS="-coverage" SHLIB_LINK="-coverage" install

# initialize database
initdb -D $PGDATA

# set appropriate port
export PGPORT=55435
echo "port = $PGPORT" >> $PGDATA/postgresql.conf

# restart cluster 'test'
pg_ctl start -l /tmp/postgres.log -w || status=$?

# something's wrong, exit now!
if [ $status -ne 0 ]; then cat /tmp/postgres.log; exit 1; fi

# run regression tests
export PG_REGRESS_DIFF_OPTS="-w -U3" # for alpine's diff (BusyBox)
make USE_PGXS=1 installcheck || status=$?

# show diff if it exists
if test -f regression.diffs; then cat regression.diffs; fi

# something's wrong, exit now!
if [ $status -ne 0 ]; then exit 1; fi

# generate *.gcov files
gcov src/*.c src/*.h


set +ev


# send coverage stats to Codecov
bash <(curl -s https://codecov.io/bash)
