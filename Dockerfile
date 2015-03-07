FROM ubuntu:14.04
MAINTAINER Nikolay Ryzhikov <niquola@gmail.com>

RUN apt-get -qq update
RUN apt-get -qqy install git \
  build-essential \
  gettext \
  libreadline6 \
  libreadline6-dev \
  zlib1g-dev \
  flex \
  bison \
  libxml2-dev \
  libxslt-dev

RUN echo "en_US.UTF-8 UTF-8" > /etc/locale.gen && locale-gen

RUN useradd -m -s /bin/bash db && echo "db:db"|chpasswd && adduser db sudo
RUN echo 'db ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

USER db
ENV HOME /home/db
ENV PG_BRANCH REL9_4_STABLE
ENV PG_REPO git://git.postgresql.org/git/postgresql.git

RUN git clone -b $PG_BRANCH --depth=1 $PG_REPO $HOME/src
RUN cd $HOME/src && ./configure --prefix=$HOME/bin && make && make install

ENV SOURCE_DIR $HOME/src

ENV PATH $HOME/bin/bin:$PATH
ENV PGDATA $HOME/data
ENV PGPORT 5432
ENV PGHOST localhost
RUN mkdir -p $PGDATA
RUN initdb -D $PGDATA -E utf8

RUN echo "host all  all    0.0.0.0/0  md5" >> $PGDATA/pg_hba.conf
RUN echo "listen_addresses='*'" >> $PGDATA/postgresql.conf
RUN echo "port=$PGPORT" >> $PGDATA/postgresql.conf

RUN pg_ctl -D $HOME/data -w start && psql postgres -c "alter user db with password 'db';"

RUN pg_ctl -D $HOME/data -w start && \
    cd $SOURCE_DIR/contrib/pgcrypto && \
    make && make install && make installcheck && \
    pg_ctl -w stop

RUN pg_ctl -D $HOME/data -w start && \
    cd $SOURCE_DIR/contrib && \
    git clone https://github.com/akorotkov/jsquery.git && \
    cd jsquery && make && make install && make installcheck && \
    pg_ctl -w stop

EXPOSE 5432
CMD pg_ctl -D $HOME/data -w start && psql postgres
