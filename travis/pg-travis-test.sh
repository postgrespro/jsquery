#!/bin/bash

set -eux

sudo apt-get update


# required packages
apt_packages="postgresql-$PG_VER postgresql-server-dev-$PG_VER postgresql-common build-essential flex bison"

# exit code
status=0

# pg_config path
pg_ctl_path=/usr/lib/postgresql/$PG_VER/bin/pg_ctl
initdb_path=/usr/lib/postgresql/$PG_VER/bin/initdb
config_path=/usr/lib/postgresql/$PG_VER/bin/pg_config


# bug: http://www.postgresql.org/message-id/20130508192711.GA9243@msgid.df7cb.de
sudo update-alternatives --remove-all postmaster.1.gz

# stop all existing instances (because of https://github.com/travis-ci/travis-cookbooks/pull/221)
sudo service postgresql stop
# ... and make sure they don't come back
echo 'exit 0' | sudo tee /etc/init.d/postgresql
sudo chmod a+x /etc/init.d/postgresql

# install required packages
sudo apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" -y install -qq $apt_packages


# perform code analysis if necessary
if [ $CHECK_CODE = "true" ]; then

	if [ "$CC" = "clang" ]; then
		sudo apt-get -y install -qq clang-$LLVM_VER

		scan-build-$LLVM_VER --status-bugs \
			-disable-checker deadcode.DeadStores \
			make USE_PGXS=1 USE_ASSERT_CHECKING=1 PG_CONFIG=$config_path || status=$?
		exit $status

	elif [ "$CC" = "gcc" ]; then
		sudo apt-get -y install -qq cppcheck

		cppcheck --template "{file} ({line}): {severity} ({id}): {message}" \
			--enable=warning,portability,performance \
			--suppress=redundantAssignment \
			--suppress=uselessAssignmentPtrArg \
			--suppress=incorrectStringBooleanError \
			--std=c89 *.c *.h 2> cppcheck.log

		if [ -s cppcheck.log ]; then
			cat cppcheck.log
			status=1 # error
		fi

		exit $status
	fi

	# don't forget to "make clean"
	make clean USE_PGXS=1 PG_CONFIG=$config_path
fi


# create cluster 'test'
CLUSTER_PATH=$(pwd)/test_cluster
$initdb_path -D $CLUSTER_PATH -U $USER -A trust

# build jsquery (using CFLAGS_SL for gcov)
make USE_PGXS=1 PG_CONFIG=$config_path CFLAGS_SL="$($config_path --cflags_sl) -coverage"
sudo make install USE_PGXS=1 PG_CONFIG=$config_path

# check build
status=$?
if [ $status -ne 0 ]; then exit $status; fi

# set permission to write postgres locks
sudo chown $USER /var/run/postgresql/

# start cluster 'test'
echo "port = 55435" >> $CLUSTER_PATH/postgresql.conf
$pg_ctl_path -D $CLUSTER_PATH start -l postgres.log -w

# run regression tests
PGPORT=55435 PGUSER=$USER PG_CONFIG=$config_path make installcheck USE_PGXS=1 || status=$?

# show diff if it exists
if test -f regression.diffs; then cat regression.diffs; fi

#generate *.gcov files
gcov *.c *.h

exit $status
