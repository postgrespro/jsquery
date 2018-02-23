#!/bin/bash

set -eux

sudo apt-get update

# bug: http://www.postgresql.org/message-id/20130508192711.GA9243@msgid.df7cb.de
sudo update-alternatives --remove-all postmaster.1.gz

# stop all existing instances (because of https://github.com/travis-ci/travis-cookbooks/pull/221)
sudo service postgresql stop
# ... and make sure they don't come back
echo 'exit 0' | sudo tee /etc/init.d/postgresql
sudo chmod a+x /etc/init.d/postgresql

# install PostgreSQL
if [ $CHECK_TYPE = "valgrind" ]; then
	# install required packages
	apt_packages="build-essential libgd-dev valgrind lcov"
	sudo apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" -y install -qq $apt_packages
	# grab sources from github
	tag=`curl -s 'https://api.github.com/repos/postgres/postgres/git/refs/tags' | jq -r '.[] | .ref' | sed 's/^refs\/tags\///' | grep "REL_*${PG_VER/./_}_" | tail -n 1`
	prefix="$HOME/pgsql-$tag"
	curl "https://codeload.github.com/postgres/postgres/tar.gz/$tag" -o ~/$tag.tar.gz
	# build them with valgrind support
	pushd ~
	tar -xzf "$tag.tar.gz"
	cd "postgres-$tag"
	./configure --enable-debug --enable-cassert --enable-coverage --prefix=$prefix
	sed -i.bak "s/\/* #define USE_VALGRIND *\//#define USE_VALGRIND/g" src/include/pg_config_manual.h
	make -sj4
	make -sj4 install
	popd
	export PATH="$prefix/bin:$PATH"
else
	apt_packages="postgresql-$PG_VER postgresql-server-dev-$PG_VER postgresql-common build-essential libgd-dev"
	sudo apt-get -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" -y install -qq $apt_packages
	prefix=/usr/lib/postgresql/$PG_VER
fi

# config path
pg_ctl_path=$prefix/bin/pg_ctl
initdb_path=$prefix/bin/initdb
config_path=$prefix/bin/pg_config

# exit code
status=0

# perform code analysis if necessary
if [ $CHECK_TYPE = "static" ]; then

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

# enable core dumps and specify their path
ulimit -c unlimited -S
echo '/tmp/%e-%s-%p.core' | sudo tee /proc/sys/kernel/core_pattern

# build extension (using CFLAGS_SL for gcov)
if [ $CHECK_TYPE == "valgrind" ] && [ $CC = "clang" ]; then
	make USE_PGXS=1 USE_ASSERT_CHECKING=1 PG_CONFIG=$config_path
	make install USE_PGXS=1 PG_CONFIG=$config_path
else
	make USE_PGXS=1 USE_ASSERT_CHECKING=1 PG_CONFIG=$config_path CFLAGS_SL="$($config_path --cflags_sl) -coverage"
	sudo make install USE_PGXS=1 PG_CONFIG=$config_path
fi

# check build
status=$?
if [ $status -ne 0 ]; then exit $status; fi

# set permission to write postgres locks
sudo chown $USER /var/run/postgresql/

# start cluster 'test'
echo "port = 55435" >> $CLUSTER_PATH/postgresql.conf
if [ $CHECK_TYPE = "valgrind" ]; then
	PGCTLTIMEOUT=600 \
	valgrind --leak-check=no --gen-suppressions=all \
	--suppressions=$HOME/postgres-$tag/src/tools/valgrind.supp --time-stamp=yes \
	--log-file=/tmp/pid-%p.log --trace-children=yes \
	$pg_ctl_path -D $CLUSTER_PATH start -l postgres.log -w
else
	$pg_ctl_path -D $CLUSTER_PATH start -l postgres.log -w
fi

# run regression tests
PGPORT=55435 PGUSER=$USER PG_CONFIG=$config_path make installcheck USE_PGXS=1 || status=$?

# stop cluster
$pg_ctl_path -D $CLUSTER_PATH stop -l postgres.log -w

# show diff if it exists
if test -f regression.diffs; then cat regression.diffs; fi

# show valgrind logs if needed
if [ $CHECK_TYPE = "valgrind" ]; then
	for f in ` find /tmp -name pid-*.log ` ; do
		if grep -q 'Command: [^ ]*/postgres' $f && grep -q 'ERROR SUMMARY: [1-9]' $f; then
			echo "========= Contents of $f"
			cat $f
			status=1
		fi
	done
fi

# check core dumps if any
for corefile in $(find /tmp/ -name '*.core' 2>/dev/null) ; do
	binary=$(gdb -quiet -core $corefile -batch -ex 'info auxv' | grep AT_EXECFN | perl -pe "s/^.*\"(.*)\"\$/\$1/g")
	echo dumping $corefile for $binary
	gdb --batch --quiet -ex "thread apply all bt full" -ex "quit" $binary $corefile
done

#generate *.gcov files
if [ $CHECK_TYPE == "valgrind" ] && [ $CC = "clang" ]; then
	bash <(curl -s https://codecov.io/bash) -x "llvm-cov gcov"
else
	bash <(curl -s https://codecov.io/bash)
fi

exit $status
