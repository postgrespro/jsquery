#!/bin/bash
set -ev

export PATH=/usr/lib/postgresql/$PG_VER/bin:$PATH
export PGDATA=/var/lib/postgresql/$PG_VER/test
export COPT=-Werror
export USE_PGXS=1

sudo chmod 1777 /var/lib/postgresql/$PG_VER
sudo chmod 1777 /var/run/postgresql

make clean
make

sudo -E env PATH=$PATH make install

/usr/lib/postgresql/$PG_VER/bin/initdb

/usr/lib/postgresql/$PG_VER/bin/pg_ctl -l logfile start
make installcheck
/usr/lib/postgresql/$PG_VER/bin/pg_ctl stop
