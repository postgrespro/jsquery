#!/bin/sh

cat ./travis/postgresql.gpg.key | sudo apt-key add -
echo "deb http://apt.postgresql.org/pub/repos/apt/ $(lsb_release -cs)-pgdg main $PG_VER" | sudo tee /etc/apt/sources.list.d/pgdg.list
