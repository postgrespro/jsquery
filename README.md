JsQuery – json query language with GIN index support
====================================================

Introduction
------------

JsQuery – is a language to query jsonb data type, introduced in PostgreSQL
release 9.4.

It's primary goal is to provide an additional functionality to jsonb
(currently missing in PostgreSQL), such as a simple and effective way
to search in nested objects and arrays, more comparison operators with
indexes support. We hope, that jsquery will be eventually a part of
PostgreSQL.

Jsquery is released as jsquery data type (similar to tsquery) and @@
match operator for jsonb.

Authors
-------

 * Teodor Sigaev <teodor@sigaev.ru>, Postgres Professional, Moscow, Russia
 * Alexander Korotkov <aekorotkov@gmail.com>, Postgres Professional, Moscow, Russia
 * Oleg Bartunov <oleg@sai.msu.su>, Postgres Professional, Moscow, Russia

Availability
------------

JsQuery is realized as an extension and not available in default PostgreSQL
installation. It is available from
[github](https://github.com/postgrespro/jsquery)
under the same license as
[PostgreSQL](http://www.postgresql.org/about/licence/)
and supports PostgreSQL 9.4+.

Regards
-------

Development is sponsored by [Wargaming.net](http://wargaming.net).

Installation
------------

JsQuery is PostgreSQL extension which requires PostgreSQL 9.4 or higher.
Before build and install you should ensure following:
    
 * PostgreSQL version is 9.4 or higher.
 * You have development package of PostgreSQL installed or you built
   PostgreSQL from source.
 * You have flex and bison installed on your system. JsQuery was tested on
   flex 2.5.37-2.5.39, bison 2.7.12.
 * Your PATH variable is configured so that pg\_config command available, or set PG_CONFIG variable.
    
Typical installation procedure may look like this:
    
    $ git clone https://github.com/postgrespro/jsquery.git
    $ cd jsquery
    $ make USE_PGXS=1
    $ sudo make USE_PGXS=1 install
    $ make USE_PGXS=1 installcheck
    $ psql DB -c "CREATE EXTENSION jsquery;"

JSON query language
-------------------

JsQuery extension contains `jsquery` datatype which represents whole JSON query
as a single value (like `tsquery` does for fulltext search). The query is an
expression on JSON-document values.

Simple expression is specified as `path binary_operator value` or
`path unary_operator`. See following examples.

 * `x = "abc"` – value of key "x" is equal to "abc";
 * `$ @> [4, 5, "zzz"]` – the JSON document is an array containing values
    4, 5 and "zzz";
 * `"abc xyz" >= 10` – value of key "abc xyz" is greater than or equal to 10;
 * `volume IS NUMERIC` – type of key "volume" is numeric.
 * `$ = true` – the whole JSON document is just a true.
 * `similar_ids.@# > 5` – similar\_ids is an array or object of length greater
   than 5;
 * `similar_product_ids.# = "0684824396"` – array "similar\_product\_ids"
   contains string "0684824396".
 * `*.color = "red"` – there is an object somewhere in the document with a key
   "color" that has a value of "red".
 * `foo = *` – key "foo" exists in object.

Path selects a set of JSON values to be checked using given operators. In
the simplest case path is just a key name. In general path is key names and
placeholders combined by dot signs. Path can use the following placeholders:

 * `#` – any index of an array;
 * `%` – any key of an object;
 * `*` – any sequence of array indexes or object keys;
 * `@#` – length of array or object, may only be used as the last component of
    a path;
 * `$` – the whole JSON document as single value, may only be the whole path.

Expression is true when operator is true against at least one value selected
by path.

Key names could be given either with or without double quotes. Key names
without double quotes may not contain spaces, start with a number or match
a jsquery keyword.

The supported binary operators are:

 * Equality operator: `=`;
 * Numeric comparison operators: `>`, `>=`, `<`, `<=`;
 * Search in a list of scalar values using `IN` operator;
 * Array comparison operators: `&&` (overlap), `@>` (contains),
   `<@` (contained in).

The supported unary operators are:

 * Check for existence operator: `= *`;
 * Check for JSON type operators: `IS ARRAY`, `IS NUMERIC`, `IS OBJECT`, `IS STRING`
   and `IS BOOLEAN`.

Expressions can be complex. Complex expressions are a set of expressions
combined by logical operators (`AND`, `OR`, `NOT`) and grouped using braces.

Examples of complex expressions:

 * `a = 1 AND (b = 2 OR c = 3) AND NOT d = 1`
 * `x.% = true OR x.# = true`

Prefix expressions are expressions given in the form `path (subexpression)`.
In this case path selects JSON values to be checked using the given subexpression.
Check results are aggregated in the same way as in simple expressions.

 * `#(a = 1 AND b = 2)` – exists element of array which a key is 1 and b key is 2
 * `%($ >= 10 AND $ <= 20)` – exists object key which values is between 10 and 20

Path can also contain the following special placeholders with "every" semantics:

 * `#:` – every index of an array;
 * `%:` – every key of an object;
 * `*:` – every sequence of array indexes and object keys.

Consider following example.

    %.#:($ >= 0 AND $ <= 1)

This example could be read as following: there is at least one key whose value
is an array of numerics between 0 and 1.

We can rewrite this example in the following form with extra braces:

    %(#:($ >= 0 AND $ <= 1))

The first placeholder `%` checks that the expression in braces is true for at least
one value in the object. The second placeholder `#:` checks if the value is an array
and that all its elements satisfy the expressions in braces.

We can rewrite this example without the `#:` placeholder as follows:

    %(NOT #(NOT ($ >= 0 AND $ <= 1)) AND $ IS ARRAY)

In this example we transform the assertion that every element of array satisfy some
condition to an assertion that there are no elements which don't satisfy the same
condition.

Some examples of using paths:

 * `numbers.#: IS NUMERIC` – every element of "numbers" array is numeric.
 * `*:($ IS OBJECT OR $ IS BOOLEAN)` – JSON is a structure of nested objects
   with booleans as leaf values.
 * `#:.%:($ >= 0 AND $ <= 1)` – each element of array is an object containing
   only numeric values between 0 and 1.
 * `documents.#:.% = *` – "documents" is an array of objects containing at least
   one key.
 * `%.#: ($ IS STRING)` – JSON object contains at least one array of strings.
 * `#.% = true` – at least one array element is an object which contains at least
   one "true" value.

The use of path operators and braces need some further explanation. When the same path
operators are used multiple times, they may refer to different values. If you want them
to always refer to the same value, you must use braces and the `$` operator. For example:

 * `# < 10 AND # > 20` – an element less than 10 exists, and an element
   greater than 20 exists.
 * `#($ < 10 AND $ > 20)` – a single element which is both less than 10 and greater
   than 20 exists (impossible).
 * `#($ >= 10 AND $ <= 20)` – there is a single element between 10 and 20.
 * `# >= 10 AND # <= 20` – an element greater or equal to 10 exists, and
   another element less or equal to 20 exists. This would be satisfied by an array with
   no elements between 10 and 20, for instance [0,30]. (Is this correct?? Shouldn't it be `NOT #($ >= 10 AND # <= 20)`?)

Same rules apply when searching inside objects and branch structures.

Type checking operators and "every" placeholders are useful for document
schema validation. JsQuery matchig operator `@@` is immutable and can be used
in CHECK constraint. See following example.

```sql
CREATE TABLE js (
    id serial,
    data jsonb,
    CHECK (data @@ '
        name IS STRING AND
        similar_ids.#: IS NUMERIC AND
        points.#:(x IS NUMERIC AND y IS NUMERIC)'::jsquery));
```

In this example the check constraint validates that in the "data" jsonb column
the value of the "name" key is a string, the value of the "similar_ids" key is an array of numerics,
and the value of the "points" key is an array of objects which contain numeric values in
"x" and "y" keys.

See our
[pgconf.eu presentation](http://www.sai.msu.su/~megera/postgres/talks/pgconfeu-2014-jsquery.pdf)
for more examples.

GIN indexes
-----------

JsQuery extension contains two operator classes (opclasses) for GIN which
provide different kinds of query optimization.

 * jsonb\_path\_value\_ops
 * jsonb\_value\_path\_ops

In each GIN opclass jsonb documents are decomposed into entries. Each
entry is associated with a particular value and its path. The difference between
opclasses is in the entry representation, comparison and usage for search
optimization.

For example, the jsonb document
`{"a": [{"b": "xyz", "c": true}, 10], "d": {"e": [7, false]}}`
would be decomposed into following entries:

 * "a".#."b"."xyz"
 * "a".#."c".true
 * "a".#.10
 * "d"."e".#.7
 * "d"."e".#.false

Since JsQuery doesn't support searching in a particular array index, we consider
all array elements to be equivalent. Thus, each array element is marked with
the same `#` sign in its path.

A problem with this representation is its size. In the given example the
key "a" is presented three times. In large branchy documents with long
keys sizes of naive entries, the representation becomes unreasonably large.
Both opclasses address this issue, but in slightly different ways.

### jsonb\_path\_value\_ops

jsonb\_path\_value\_ops represents an index entry as a pair of a hash of the path
and the value:

    (hash(path_item_1.path_item_2. ... .path_item_n); value)

When comparison index entries, the path hash is the first part of entry and the value is
the lower part. This determines the features of this opclass. Since the path
is hashed and it's the first part of the index entry, we need to know the full path to
a value in order to use the index for searching. However, once the path is specified
we can use both exact and range searches very efficiently.

### jsonb\_value\_path\_ops

jsonb\_value\_path\_ops represents an index entry as pair of the JSON value and a bloom filter
of paths:

    (value; bloom(path_item_1) | bloom(path_item_2) | ... | bloom(path_item_n))

This is the reverse of how index entries are ordered with jsonb\_path\_value\_ops. Since
the value is the first part of an index entry, the index can only perform searches against
exact values. A search over a range of values is possible as well, but we have to
filter all the the different paths where matching values occur. The Bloom filter
over path items allows the index to be used for conditions containing `%` and `*` in
their paths.

### Query optimization

JsQuery opclasses perform complex query optimization. It's valuable for
a developer or administrator to see the how a JsQuery has been optimized.
Unfortunately, opclasses aren't allowed to put any custom output in an
EXPLAIN statement. JsQuery provides these functions to let you see how a
JsQuery has been optimized:

 * gin\_debug\_query\_path\_value(jsquery) – for jsonb\_path\_value\_ops
 * gin\_debug\_query\_value\_path(jsquery) – for jsonb\_value\_path\_ops

The result of these functions is a textual representation of the query tree
where leaves are GIN search entries. For example:

    # SELECT gin_debug_query_path_value('x = 1 AND (*.y = 1 OR y = 2)');
     gin_debug_query_path_value
    ----------------------------
     x = 1 , entry 0           +

    # SELECT gin_debug_query_value_path('x = 1 AND (*.y = 1 OR y = 2)');
     gin_debug_query_value_path
    ----------------------------
     AND                       +
       x = 1 , entry 0         +
       OR                      +
         *.y = 1 , entry 1     +
         y = 2 , entry 2       +

Unfortunately, Postgres does not collect statistics for jsonb yet. Because of
that, the JsQuery optimizer has to make an imperative decision about whether to
use an index while evaluating a JsQuery expression.
This decision is made by assuming that some conditions are less
selective than others. The optimizer divides conditions into following selectivity
classes (listed in descending order of selectivity):

 1. Equality (x = c)
 2. Range (c1 < x < c2)
 3. Inequality (x > c)
 4. Is (x is type)
 5. Any (x = \*)

The optimizer avoids index evaluation of less selective conditions when possible.
For example, in the `x = 1 AND y > 0` query `x = 1` is assumed to be more
selective than `y > 0`. That's why the index isn't used for evaluation of `y > 0`.

    # SELECT gin_debug_query_path_value('x = 1 AND y > 0');
     gin_debug_query_path_value
    ----------------------------
     x = 1 , entry 0           +

With the lack of statistics, decisions made by optimizer can be inaccurate. That's
why JsQuery supports hints. The comments `/*-- index */` or `/*-- noindex */`
placed in the conditions force the optimizer to use (or not use) an index:

    SELECT gin_debug_query_path_value('x = 1 AND y /*-- index */ > 0');
     gin_debug_query_path_value
    ----------------------------
     AND                       +
       x = 1 , entry 0         +
       y > 0 , entry 1         +

    SELECT gin_debug_query_path_value('x /*-- noindex */ = 1 AND y > 0');
     gin_debug_query_path_value
     ----------------------------
      y > 0 , entry 0           +

Contribution
------------

Please note that JsQuery is still under development. While it's
stable and tested, it may contain some bugs. Don't hesitate to create
[issues at github](https://github.com/akorotkov/jsquery/issues) for any
bug reports.

If there's some functionality you'd like to see added to JsQuery and you feel
like you can implement it, then you're welcome to make pull requests.

