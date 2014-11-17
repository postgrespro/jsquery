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

* [Getting Started](doc/intro.md)
* [Syntax](doc/syntax.md)
* [Operators](doc/operators.md)
* [Indexes](doc/indexes.md)
* [Optimizer](doc/optimiser.md)

## Installation

Requirements:

* PostgreSQL >= 9.4

### Using pgxn.org ?

http://pgxn.org/

### Build from sources

```sh
git clone https://github.com/akorotkov/jsquery.git $SOURCE_DIR/contrib
cd $SOURCE_DIR/contrib/jsquery && make && make install && make installcheck

```

### Using docker

```
docker run --name=myjsquery -p 5432:5555 -i -t jsquery/jsquery

psql -p 5555
```

## Roadmap

* TODO1
* TODO2

## Contribution

You can contribute by:

* stars
* [issues](https://github.com/akorotkov/jsquery/issues)
* documentation
* pull requests

## License

MIT?
