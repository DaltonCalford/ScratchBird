# InfluxQL Specification

**Status:** Draft (Beta)

## 1. Purpose

Define InfluxQL (InfluxDB v1) for time-series queries, including SELECT,
measurement filters, time ranges, and aggregation functions.

## 2. Core Concepts

- **Measurement**: Time-series table.
- **Tags**: Indexed key/value labels (strings).
- **Fields**: Measured values (numeric, boolean, string).
- **Time**: Primary dimension for range filtering.

## 3. EBNF

```ebnf
query         = select_stmt | show_stmt | drop_stmt ;

select_stmt   = "SELECT" select_list
                "FROM" measurement
                [ "WHERE" where_clause ]
                [ "GROUP" "BY" group_clause ]
                [ "ORDER" "BY" time_order ]
                [ "LIMIT" number ] [ "OFFSET" number ] ;

select_list   = "*" | select_item { "," select_item } ;
select_item   = field_expr [ "AS" identifier ] ;

field_expr    = field
              | agg_func "(" field ")"
              | math_expr ;

where_clause  = condition { ("AND" | "OR") condition } ;
condition     = expr op expr ;

expr          = field | tag | number | string | time_literal | func_call ;

op            = "=" | "!=" | ">" | ">=" | "<" | "<=" ;

group_clause  = group_item { "," group_item } ;
group_item    = "time" "(" duration ")" | tag ;

show_stmt     = "SHOW" ("MEASUREMENTS" | "TAG" "KEYS" | "FIELD" "KEYS") ;

drop_stmt     = "DROP" ("MEASUREMENT" measurement | "SERIES" | "DATABASE" identifier) ;
```

## 4. Semantics

- `GROUP BY time()` buckets points into fixed time windows.
- Tag conditions are indexed; field conditions are not.
- Aggregations are applied per group bucket.

## 5. Usage Guidance

**When to use InfluxQL**
- Time-series analytics with SQL-like syntax.
- Aggregations over time buckets.

**Avoid when**
- Complex joins or non-time-series workloads are required.

## 6. Examples

```sql
SELECT mean(value) FROM cpu
WHERE host = 'server1' AND time > now() - 1h
GROUP BY time(5m)
ORDER BY time DESC
LIMIT 100;
```

