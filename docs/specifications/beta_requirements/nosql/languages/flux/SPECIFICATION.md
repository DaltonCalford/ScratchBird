# Flux Specification

**Status:** Draft (Beta)

## 1. Purpose

Define Flux, the functional data scripting language for time-series
analytics. This spec covers core pipeline syntax, functions, and usage
patterns.

## 2. Core Concepts

- **Pipes**: Data flows through chained functions using `|>`.
- **Tables**: Streams of tables partitioned by group keys.
- **Functions**: Stateless transforms (filter, map, aggregate).

## 3. EBNF

```ebnf
program       = { statement } ;
statement     = assignment | expression ;
assignment    = identifier "=" expression ;

expression    = pipe_expr | function_call | literal | identifier ;

pipe_expr     = expression "|>" function_call { "|>" function_call } ;

function_call = identifier "(" [ args ] ")" ;
args          = arg { "," arg } ;
arg           = identifier ":" expression | expression ;

literal       = number | string | boolean | time_literal | duration_literal ;
```

## 4. Core Functions

- `from(bucket: "name")`
- `range(start: -1h, stop: now())`
- `filter(fn: (r) => ...)`
- `aggregateWindow(every: 5m, fn: mean)`
- `group(columns: [...])`
- `yield(name: "result")`

## 5. Semantics

- Each function operates on a stream of tables.
- Group keys define table partitioning; `group()` changes them.
- `aggregateWindow` reduces rows into time buckets.

## 6. Usage Guidance

**When to use Flux**
- Complex time-series transformations.
- Multi-step analytics with custom functions.

**Avoid when**
- Simple time-series queries (InfluxQL is simpler).

## 7. Examples

```flux
from(bucket: "metrics")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "cpu")
  |> aggregateWindow(every: 5m, fn: mean)
  |> yield(name: "avg_cpu")
```

