# ArangoDB AQL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the ArangoDB Query Language (AQL), a pipeline-style language for
document and graph queries. This spec focuses on grammar, semantics, and
usage guidance for emulation.

## 2. Core Concepts

- **Collections**: Document collections and edge collections.
- **Pipeline**: AQL is a sequence of clauses executed left to right.
- **Variables**: Created by FOR/LET and passed through the pipeline.
- **Graph traversals**: Built-in traversal clause for graph queries.

## 3. EBNF

### 3.1 Query Structure

```ebnf
query         = { clause } ;

clause        = for_clause
              | let_clause
              | filter_clause
              | sort_clause
              | limit_clause
              | collect_clause
              | return_clause
              | insert_clause
              | update_clause
              | replace_clause
              | remove_clause
              | upsert_clause
              | traversal_clause ;
```

### 3.2 Clauses

```ebnf
for_clause    = "FOR" var "IN" expr [ "OPTIONS" object ] ;

traversal_clause =
    "FOR" [ var [ "," var [ "," var ] ] ]
    "IN" range direction expr graph_target
    [ "OPTIONS" object ] ;

range         = number [ ".." number ] ;
direction     = "OUTBOUND" | "INBOUND" | "ANY" ;
graph_target  = "GRAPH" identifier | collection ;

let_clause    = "LET" var "=" expr ;
filter_clause = "FILTER" expr ;

sort_clause   = "SORT" sort_item { "," sort_item } ;
sort_item     = expr [ "ASC" | "DESC" ] ;

limit_clause  = "LIMIT" expr [ "," expr ] ;

collect_clause = "COLLECT" collect_item { "," collect_item }
                 [ "INTO" var ]
                 [ "KEEP" keep_list ]
                 [ "WITH" "COUNT" "INTO" var ] ;

collect_item  = var "=" expr | var ;
keep_list     = var { "," var } ;

return_clause = "RETURN" expr ;
```

### 3.3 Data Modification

```ebnf
insert_clause = "INSERT" expr "IN" collection
                [ "OPTIONS" object ]
                [ "RETURN" expr ] ;

update_clause = "UPDATE" expr "WITH" expr "IN" collection
                [ "OPTIONS" object ]
                [ "RETURN" expr ] ;

replace_clause = "REPLACE" expr "WITH" expr "IN" collection
                 [ "OPTIONS" object ]
                 [ "RETURN" expr ] ;

remove_clause = "REMOVE" expr "IN" collection
                [ "OPTIONS" object ]
                [ "RETURN" expr ] ;

upsert_clause = "UPSERT" expr
                "INSERT" expr
                "UPDATE" expr
                "IN" collection
                [ "OPTIONS" object ]
                [ "RETURN" expr ] ;
```

### 3.4 Expressions

```ebnf
expr          = literal
              | object
              | array
              | var
              | attribute_access
              | func_call
              | "(" expr ")"
              | expr binary_op expr
              | unary_op expr
              | ternary_expr ;

attribute_access = expr "." identifier
                 | expr "[" expr "]" ;

ternary_expr  = expr "?" expr ":" expr ;

binary_op     = "+" | "-" | "*" | "/" | "%" | "==" | "!=" | "<" | "<=" | ">" | ">="
              | "AND" | "OR" | "IN" | "LIKE" ;
unary_op      = "-" | "NOT" ;
```

## 4. Semantics

- **FOR** iterates over a collection or array expression, emitting rows.
- **LET** binds computed values for later clauses.
- **FILTER** reduces rows by predicate.
- **COLLECT** groups rows; aggregates can be derived via functions or
  `WITH COUNT INTO`.
- **Traversal** yields vertices, edges, and path variables for graph queries.
- **RETURN** is mandatory for read queries.

## 5. Usage Guidance

**When to use AQL**
- Mixed document and graph workloads.
- Queries that naturally map to a pipeline model.

**Avoid when**
- Strict SQL requirements or heavy relational joins are required.

## 6. Examples

**Simple filter**
```aql
FOR u IN users
  FILTER u.status == "active"
  RETURN u
```

**Group and count**
```aql
FOR o IN orders
  COLLECT status = o.status WITH COUNT INTO cnt
  RETURN { status, cnt }
```

**Traversal**
```aql
FOR v, e, p IN 1..2 OUTBOUND \"users/123\" GRAPH \"social\"
  FILTER v.active == true
  RETURN v
```

**Upsert**
```aql
UPSERT { email: \"a@example.com\" }
  INSERT { email: \"a@example.com\", status: \"new\" }
  UPDATE { status: \"active\" }
IN users
RETURN NEW
```

