## Syntax

```jsquery``` expresion usually consists of *path* and *value_expression*:

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

```
user.name = 'diego'
```


```ebnf
  expr ::= path value_expr
   | path HINT value_expr
   | NOT expr
   | NOT HINT value_expr
   | NOT value_expr
   | path '(' expr ')'
   | '(' expr ')'
   | expr AND expr
   | expr OR expr

  value_expr ::= '=' scalar_value
   | IN '(' value_list ')'
   | '=' array
   | '=' '*'
   | '<' NUMERIC
   | '<' '=' NUMERIC
   | '>' NUMERIC
   | '>' '=' NUMERIC
   | '@' '>' array
   | '<' '@' array
   | '&' '&' array
   | IS ARRAY
   | IS NUMERIC
   | IS OBJECT
   | IS STRING
   | IS BOOLEAN

  path ::= key
   | path '.' key_any
   | NOT '.' key_any

  key ::= '*'
   | '#'
   | '%'
   | '$'
   | STRING

  key_any ::= key
   | NOT

  value_list ::= scalar_value
   | value_list ',' scalar_value

  array ::= '[' value_list ']'

  scalar_value ::= null
   | STRING
   | true
   | false
   | NUMERIC
   | OBJECT
```

