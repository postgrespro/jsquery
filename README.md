# jsquery

[![Build Status](https://travis-ci.org/niquola/jsquery.svg)](https://travis-ci.org/niquola/jsquery)


`Jsquery` is PostgreSQL extension,
which provides advanced query language for jsonb documents.

Features:

* recursive structure of query
* search in array
* rich set of comparison operators
* types support
* schema support (constraints on keys, values)
* indexes support
* hinting support


Jsquery implemented as datatype `jsquery` and operator `@@`.

Examples:

`#` - any element array

```SQL
SELECT '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.# = 2';
```

`%` - any key

```SQL
SELECT '{"a": {"b": [1,2,3]}}'::jsonb @@ '%.b.# = 2';
```

`*` - anything

```SQL
SELECT '{"a": {"b": [1,2,3]}}'::jsonb @@ '*.# = 2';
```

`$` - current element

```SQL
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b.# ($ = 2 OR $ < 3)';
```

Use "double quotes" for key !

```SQL
select 'a1."12222" < 111'::jsquery;
```

## Documentation

* [Syntax](doc/intro.md)
* [Indexes](doc/indexes.md)
* [Optimizer](doc/optimiser.md)

## Installation

Requirements:

* PostgreSQL >= 9.4

### Build from sources

```sh
git clone https://github.com/akorotkov/jsquery.git $SOURCE_DIR/contrib
cd $SOURCE_DIR/contrib/jsquery && make && make install && make installcheck

# or not from pg sources

git clone https://github.com/akorotkov/jsquery.git
cd jsquery && make USE_PGXS=1 && make install USE_PGXS=1

```

### Using docker

TODO

## Roadmap

* Implement jsquery as extension of SQL syntax

## Contribution

You can contribute by:

* stars
* [issues](https://github.com/akorotkov/jsquery/issues)
* documentation
* pull requests

## Authors

* Oleg Bartunov
* Teodor Sigaev
* Alexand Korotkov

## Regards

Development is sponsored by [Wargaming.net]

## License

GPL
