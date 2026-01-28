# Couchbase N1QL (SQL++) Specification

**Status:** Draft (Beta)

## 1. Purpose

Define a complete specification for Couchbase N1QL (SQL++), including the
SELECT query model, joins, UNNEST/NEST, and DML. This document is intended
for dialect emulation and parser design.

## 2. Core Concepts

- **Keyspace**: Bucket, scope, and collection. Full path is
  `bucket.scope.collection`.
- **Document**: JSON object with a primary key.
- **Missing vs Null**: Missing means field is absent; null is an explicit
  value. Predicates distinguish them.
- **META()**: Metadata access (document key, CAS, expiration).
- **UNNEST**: Flattens array elements into result rows.
- **NEST**: Produces arrays of joined documents.
- **USE KEYS**: Key lookup hint using primary keys.

## 3. Lexical Rules

- Identifiers are case-sensitive when quoted with backticks.
- Unquoted identifiers are case-insensitive and folded to lower-case.
- String literals use double quotes.

## 4. EBNF

### 4.1 Query

```ebnf
query         = select_stmt | insert_stmt | upsert_stmt | update_stmt
              | delete_stmt | merge_stmt ;

select_stmt   = "SELECT" [ "DISTINCT" ] select_list
                from_clause
                [ let_clause ]
                [ where_clause ]
                [ group_by_clause ]
                [ having_clause ]
                [ order_by_clause ]
                [ limit_clause ] ;

select_list   = select_item { "," select_item } ;
select_item   = "*" | expr [ "AS" alias ] ;

from_clause   = "FROM" keyspace_ref
                { join_clause | nest_clause | unnest_clause } ;

keyspace_ref  = keyspace [ "AS" alias ] [ use_keys ] [ use_index ] ;
keyspace      = identifier
              | identifier "." identifier
              | identifier "." identifier "." identifier ;

use_keys      = "USE" "KEYS" expr ;
use_index     = "USE" "INDEX" "(" index_list ")" [ "USING" index_type ] ;
index_list    = identifier { "," identifier } ;
index_type    = "GSI" | "FTS" ;

join_clause   = join_type "JOIN" keyspace_ref "ON" expr ;
nest_clause   = join_type "NEST" keyspace_ref "ON" expr ;
join_type     = "INNER" | "LEFT" | "RIGHT" | "FULL" | "CROSS" ;

unnest_clause = "UNNEST" expr [ "AS" alias ] ;

let_clause    = "LET" let_binding { "," let_binding } ;
let_binding   = alias "=" expr ;

where_clause  = "WHERE" expr ;

group_by_clause = "GROUP" "BY" expr_list
                  [ "LETTING" let_binding { "," let_binding } ] ;

having_clause = "HAVING" expr ;

order_by_clause = "ORDER" "BY" order_item { "," order_item } ;
order_item    = expr [ "ASC" | "DESC" ]
                [ "NULLS" "FIRST" | "NULLS" "LAST" ] ;

limit_clause  = "LIMIT" expr [ "OFFSET" expr ] ;

expr_list     = expr { "," expr } ;
```

### 4.2 DML

```ebnf
insert_stmt   = "INSERT" "INTO" keyspace_ref insert_values
                [ "RETURNING" select_list ] ;
insert_values = "VALUES" insert_value
              | select_stmt ;
insert_value  = "(" "KEY" "," "VALUE" ")" "VALUES" "(" expr "," expr ")" ;

upsert_stmt   = "UPSERT" "INTO" keyspace_ref insert_values
                [ "RETURNING" select_list ] ;

update_stmt   = "UPDATE" keyspace_ref
                "SET" set_item { "," set_item }
                [ "UNSET" field_path_list ]
                [ where_clause ]
                [ "RETURNING" select_list ] ;

delete_stmt   = "DELETE" "FROM" keyspace_ref
                [ where_clause ]
                [ "RETURNING" select_list ] ;

merge_stmt    = "MERGE" "INTO" keyspace_ref
                "USING" keyspace_ref
                "ON" expr
                merge_actions ;

merge_actions = "WHEN" "MATCHED" "THEN" update_stmt
              | "WHEN" "NOT" "MATCHED" "THEN" insert_stmt ;

set_item      = field_path "=" expr ;
field_path_list = field_path { "," field_path } ;
```

### 4.3 Expressions

```ebnf
expr          = literal
              | field_path
              | func_call
              | case_expr
              | "(" expr ")"
              | expr binary_op expr
              | unary_op expr
              | any_every_expr ;

field_path    = identifier { "." identifier }
              | identifier "[" expr "]" ;

any_every_expr = ("ANY" | "EVERY" | "ANY" "AND" "EVERY")
                 alias "IN" expr "SATISFIES" expr "END" ;

binary_op     = "+" | "-" | "*" | "/" | "=" | "!=" | "<" | "<=" | ">" | ">="
              | "AND" | "OR" | "LIKE" | "IN" | "BETWEEN" | "IS" | "IS NOT" ;
unary_op      = "-" | "NOT" ;
```

## 5. Semantics

- `USE KEYS` performs key-based lookup and bypasses secondary indexes.
- `UNNEST` multiplies rows by array elements; `NEST` aggregates join matches
  into an array.
- `ANY/EVERY` apply array quantification and must bind an array variable.
- `NULL` and `MISSING` are distinct in predicates.

## 6. Usage Guidance

**When to use N1QL**
- JSON document data with relational-style queries and joins.
- Keyspace-aware filtering and schema-flexible workloads.

**Avoid when**
- Workloads require strict relational integrity or heavy SQL analytics with
  complex windowing.

## 7. Examples

**Keyspace select with USE KEYS**
```sql
SELECT d.name, META(d).id
FROM bucket.scope.collection AS d
USE KEYS ["u123", "u456"];
```

**UNNEST array**
```sql
SELECT o.order_id, item.sku
FROM orders AS o
UNNEST o.items AS item
WHERE item.qty > 2;
```

**Join and NEST**
```sql
SELECT u.name, olist
FROM users AS u
LEFT NEST orders AS olist
ON olist.user_id = META(u).id;
```

**Update with SET/UNSET**
```sql
UPDATE users AS u
SET u.status = "inactive"
UNSET u.last_login
WHERE u.status = "active";
```

