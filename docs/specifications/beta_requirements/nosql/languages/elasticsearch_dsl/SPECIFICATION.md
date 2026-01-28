# Elasticsearch / OpenSearch Query DSL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the JSON-based query DSL used by Elasticsearch and OpenSearch. This
spec includes query structures, filters, aggregations, and search request
shape, intended for emulation and parser design.

## 2. Core Concepts

- **Query context** vs **filter context** (scoring vs no scoring)
- **Full-text queries** (match, multi_match, query_string)
- **Structured queries** (term, range, exists)
- **Aggregations** (bucket and metric aggregations)
- **Sort and pagination** (`from`, `size`, `search_after`)

## 3. EBNF (Search Request)

```ebnf
search_request = "{" search_item { "," search_item } "}" ;

search_item   = "query" ":" query_obj
              | "sort" ":" sort_list
              | "from" ":" number
              | "size" ":" number
              | "track_total_hits" ":" bool
              | "_source" ":" source_spec
              | "aggs" ":" aggs_obj
              | "post_filter" ":" query_obj
              | "highlight" ":" highlight_obj
              | "collapse" ":" collapse_obj ;
```

## 4. Query DSL Grammar (Core)

```ebnf
query_obj     = "{" query_type ":" query_body "}" ;

query_type    = "match" | "match_phrase" | "multi_match" | "query_string"
              | "term" | "terms" | "range" | "exists" | "prefix" | "wildcard"
              | "bool" | "constant_score" | "function_score" ;

query_body    = object ;
```

### 4.1 Bool Query

```ebnf
bool_query    = "{" "bool" ":" "{" bool_item { "," bool_item } "}" "}" ;

bool_item     = "must" ":" query_list
              | "should" ":" query_list
              | "filter" ":" query_list
              | "must_not" ":" query_list
              | "minimum_should_match" ":" number ;

query_list    = "[" query_obj { "," query_obj } "]" ;
```

### 4.2 Term/Range

```ebnf
term_query    = "{" "term" ":" "{" field ":" value "}" "}" ;

range_query   = "{" "range" ":" "{" field ":" range_body "}" "}" ;
range_body    = "{" range_item { "," range_item } "}" ;
range_item    = "gte" ":" value | "gt" ":" value
              | "lte" ":" value | "lt" ":" value ;
```

### 4.3 Match

```ebnf
match_query   = "{" "match" ":" "{" field ":" match_body "}" "}" ;
match_body    = value | object ;
```

## 5. Aggregations

```ebnf
aggs_obj      = "{" agg_item { "," agg_item } "}" ;
agg_item      = agg_name ":" agg_body ;
agg_body      = "{" agg_type ":" object [ "," "aggs" ":" aggs_obj ] "}" ;

agg_type      = "terms" | "date_histogram" | "range" | "histogram"
              | "avg" | "sum" | "min" | "max" | "count" | "cardinality" ;
```

## 6. Semantics

- **Bool**: `must` and `filter` clauses are ANDed; `should` is OR with
  `minimum_should_match`.
- **Range**: inclusive/exclusive bounds via `gte`/`gt`/`lte`/`lt`.
- **Filter context**: no scoring, cached when possible.
- **Aggregations**: executed after query filtering.

## 7. Usage Guidance

**When to use DSL**
- Full-text search workloads with scoring and relevance.
- Faceted search using aggregations.

**Avoid when**
- Strong relational joins or transactional constraints are needed.

## 8. Examples

**Bool query with filter**
```json
{
  "query": {
    "bool": {
      "must": [{ "match": { "title": "database" } }],
      "filter": [{ "range": { "year": { "gte": 2020 } } }]
    }
  }
}
```

**Aggregations**
```json
{
  "query": { "term": { "status": "active" } },
  "aggs": {
    "by_type": { "terms": { "field": "type" } },
    "avg_size": { "avg": { "field": "size" } }
  }
}
```

