# PromQL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define PromQL, the query language for Prometheus metrics. This spec covers
instant queries, range queries, vector matching, and aggregation.

## 2. Core Concepts

- **Instant vector**: Set of time series at a single timestamp.
- **Range vector**: Set of time series over a time range.
- **Scalar**: Single numeric value.
- **Label matching**: `{label="value"}` filters.

## 3. EBNF

```ebnf
expr          = aggregate_expr | binary_expr | unary_expr | vector_selector
              | range_selector | function_call | number ;

vector_selector = metric_name [ label_matchers ] ;
label_matchers  = "{" matcher { "," matcher } "}" ;
matcher       = label_name match_op label_value ;
match_op      = "=" | "!=" | "=~" | "!~" ;

range_selector = vector_selector "[" duration "]" ;

aggregate_expr = agg_op [ "(" grouping ")" ] "(" expr ")" ;
agg_op        = "sum" | "avg" | "min" | "max" | "count" | "stddev" | "stdvar"
              | "topk" | "bottomk" ;

binary_expr   = expr bin_op expr [ vector_matching ] ;
vector_matching = ("on" | "ignoring") "(" label_list ")"
                  [ "group_left" | "group_right" ] ;

bin_op        = "+" | "-" | "*" | "/" | "%" | "==" | "!=" | "<" | "<=" | ">" | ">="
              | "and" | "or" | "unless" ;

unary_expr    = ("+" | "-") expr ;

function_call = identifier "(" [ arg_list ] ")" ;
arg_list      = expr { "," expr } ;
```

## 4. Semantics

- `metric{label="x"}` selects time series by label set.
- Range selectors (`[5m]`) produce range vectors for functions like
  `rate()` or `avg_over_time()`.
- Vector matching defines label alignment for binary ops.

## 5. Usage Guidance

**When to use PromQL**
- Metrics analytics, alerting queries, time-series counters/gauges.

**Avoid when**
- Full text search or arbitrary document queries are needed.

## 6. Examples

**CPU usage rate**
```
rate(cpu_usage_seconds_total{job="api"}[5m])
```

**Error ratio**
```
(sum(rate(http_requests_total{status=~"5.."}[5m]))
 / sum(rate(http_requests_total[5m])))
```

