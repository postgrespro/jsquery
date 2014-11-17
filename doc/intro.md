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

the expresion is:

```
user.name = 'diego'
```

Sevral expressions could be connected using `AND` & `OR` operators:

```
user.name = 'diego' AND site.url = 'diego.com'
```
