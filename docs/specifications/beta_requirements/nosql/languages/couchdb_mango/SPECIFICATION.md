# CouchDB Mango Query Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the Mango JSON query language used by CouchDB, including selectors,
index definitions, sorting, pagination, and field selection.

## 2. Core Concepts

- **Selector**: JSON predicate tree matching documents.
- **Index**: JSON index on fields (or design document index).
- **Sort**: Ordered result set on indexed fields.
- **Pagination**: `limit` and `skip`.

## 3. EBNF (Selector)

```ebnf
selector      = "{" sel_item { "," sel_item } "}" ;
sel_item      = field_path ":" criterion
              | logical_op ":" array ;

field_path    = string ;
logical_op    = "$and" | "$or" | "$nor" | "$not" ;

criterion     = value
              | "{" op_item { "," op_item } "}" ;

op_item       = comparison_op ":" value
              | array_op ":" value
              | element_op ":" value
              | eval_op ":" value ;

comparison_op = "$eq" | "$ne" | "$gt" | "$gte" | "$lt" | "$lte"
              | "$in" | "$nin" ;

array_op      = "$all" | "$elemMatch" | "$size" ;

element_op    = "$exists" | "$type" ;

eval_op       = "$regex" | "$mod" ;
```

## 4. Query Object Grammar

```ebnf
find_query    = "{" find_item { "," find_item } "}" ;
find_item     = "selector" ":" selector
              | "fields" ":" array
              | "sort" ":" array
              | "limit" ":" number
              | "skip" ":" number
              | "use_index" ":" use_index
              | "bookmark" ":" string ;

use_index     = string | array ;
```

## 5. Index Definition Grammar

```ebnf
index_def     = "{" "index" ":" index_body
                  [ "," "name" ":" string ]
                  [ "," "type" ":" string ]
                "}" ;

index_body    = "{" "fields" ":" array "}" ;
```

## 6. Semantics

- `selector` filters documents by field predicates.
- `fields` projects only listed fields.
- `sort` requires an index on the sort fields.
- `use_index` targets a specific index or design doc index.
- `bookmark` is used for pagination in large result sets.

## 7. Usage Guidance

**When to use Mango**
- Simple to moderately complex document queries.
- Index-based filtering on common fields.

**Avoid when**
- Workloads need full-text search or multi-document joins.

## 8. Examples

**Find documents**
```json
{
  "selector": { "type": "order", "total": { "$gte": 100 } },
  "fields": ["_id", "total", "status"],
  "sort": [{ "total": "desc" }],
  "limit": 50
}
```

**Create index**
```json
{
  "index": { "fields": ["type", "total"] },
  "name": "type_total_idx",
  "type": "json"
}
```

