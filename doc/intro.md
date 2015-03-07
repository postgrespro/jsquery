# Jsquery intro

```jsquery``` implemented as datatype and `@@` operator.

Search query has a form:

```SQL
SELECT * FROM mytable
 WHERE jsonb_column @@ 'jsquery_expression';
```


```jsquery_expression``` usually consists
of *path* and *value_expression*.

For example if we are looking for:

```json
{
  "user": {
    "name": "Diego"
  },
  "site": {
    "url": "diego.com"
  }
}
```

the expression is:

```
user.name = 'diego'
```

Sevral expressions could be connected using boolean `AND` & `OR` operators:

```
user.name = 'diego' AND site.url = 'diego.com'
```

Expression could be negated with `NOT` operator:

```
NOT user.name = 'diego'
```

JSON value could be one following types:

* array
* numeric
* object
* string
* boolean

You can check type using `is` operator:

```
user.name is array
user.name is numeric
user.name is object
user.name is string
user.name is boolean
```

For all types you `=` (equality) operator is defined:

```
user.roles = ["admin","root"]
user.age = 3
user.active = true
user.address = {city: "SPb"}
user.name = "diego"
```

For numerics there are expected comparison operators:

```
x > 1 AND x < 10
x >= 1 AND x <= 10
```

To check that scalar value belongs to some list:

```sql
select '{a: 2}'::jsonb @@ 'a IN (1,2,5)';
```

For arrays there are convenient operators:

```sql
-- overlap
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b && [1,2,5]';

-- contains
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b @> [1,2]';

-- contained
select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b <@ [1,2,3,4,5]'
```

If you just want to check that some path exists in json document use `=*`:

select '{"a": {"b": [1,2,3]}}'::jsonb @@ 'a.b = *'


Path expression supports wild cards:

`#` - any alement of array
`%` - any key in object
`*` - any path

```sql
select '{a: {b: {c: 1}}}'::jsonb @@ '*.c = 1'
select '{a: {b: {c: 1}}}'::jsonb @@ 'a.%.c = 1'
select '{a: {b: [1,2]}}'::jsonb @@ 'a.b.# = 1'
```

jsquery expression could be expressed recursively using `()`:

```
address(city = "SPB" AND street = "Nevskiy")
```

This means eval `city = "SPB" AND street = "Nevskiy"` expression in context of `address` attribute.
