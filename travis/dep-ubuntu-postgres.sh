#!/bin/sh

curl -fsSL https://www.postgresql.org/media/keys/ACCC4CF8.asc|sudo gpg --dearmor -o /etc/apt/trusted.gpg.d/postgresql.gpg
echo "deb http://apt-archive.postgresql.org/pub/repos/apt/ $(lsb_release -cs)-pgdg main $PG_VER"
