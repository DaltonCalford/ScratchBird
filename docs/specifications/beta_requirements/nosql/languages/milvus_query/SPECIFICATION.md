# Milvus Query Language Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the SQL-like query language used by Milvus for vector search and
attribute filtering. This spec focuses on grammar, semantics, and usage
patterns for emulation.

## 2. Core Concepts

- **Collection**: Vector collection with schema (vector field + scalar fields).
- **Vector search**: Similarity search using ANN indexes.
- **Filtering**: Boolean expressions on scalar fields.
- **Output fields**: Selected attributes returned with search results.

## 3. EBNF

```ebnf
search_stmt   = "SEARCH" "FROM" collection
                "WHERE" filter_expr
                "USING" search_params
                "TOPK" number
                "OUTPUT" output_fields ;

query_stmt    = "QUERY" "FROM" collection
                "WHERE" filter_expr
                [ "OUTPUT" output_fields ]
                [ "LIMIT" number ] ;

collection    = identifier ;

search_params = "{" param_item { "," param_item } "}" ;
param_item    = identifier ":" value ;

output_fields = "[" field { "," field } "]" ;
field         = identifier ;

filter_expr   = expr ;
expr          = term { ("AND" | "OR") term } ;
term          = factor | "(" expr ")" ;
factor        = field op value ;

op            = "==" | "!=" | "<" | "<=" | ">" | ">=" | "IN" ;
```

## 4. Semantics

- `SEARCH` returns nearest neighbors based on vector similarity.
- `filter_expr` applies scalar filtering before or after ANN search.
- `OUTPUT` defines which scalar fields are returned.
- Search parameters include `metric_type` and index-specific parameters.

## 5. Usage Guidance

**When to use Milvus queries**
- Vector similarity search with metadata filtering.
- Large-scale embedding retrieval.

**Avoid when**
- Pure relational queries without vector similarity.

## 6. Examples

**Vector search**
```sql
SEARCH FROM images
WHERE category == "cat"
USING {"metric_type": "L2", "params": {"nprobe": 16}}
TOPK 10
OUTPUT [id, category]
```

**Scalar query**
```sql
QUERY FROM images
WHERE category == "cat" AND score > 0.9
OUTPUT [id, score]
LIMIT 100
```

