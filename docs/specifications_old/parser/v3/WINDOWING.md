# V3 Parser: Window Functions and Window Frames (Authoritative)

Status: Authoritative (V3)

This document defines the parsing, AST, SBLR emission, and execution semantics
for window functions and `OVER (...)` clauses. It is the only authoritative
source for windowing behavior in V3.

## Grammar

```
<window_call> ::= <window_func> "(" [<expr_list>] ")" <window_spec>
<window_spec> ::= OVER "(" [<partition_clause>] [<order_clause>] [<frame_clause>] ")"
<partition_clause> ::= PARTITION BY <expr_list>
<order_clause> ::= ORDER BY <order_by_list>

<frame_clause> ::= <frame_unit> <frame_extent>
<frame_unit> ::= ROWS | RANGE | GROUPS
<frame_extent> ::= BETWEEN <frame_bound> AND <frame_bound>
                 | <frame_bound>  -- end defaults to CURRENT ROW
<frame_bound> ::= UNBOUNDED PRECEDING
               | UNBOUNDED FOLLOWING
               | CURRENT ROW
               | <expr> PRECEDING
               | <expr> FOLLOWING
```

Unsupported clauses (must error with `0A000`):
- `EXCLUDE` (all variants)
- named windows (`OVER <window_name>`) in the ScratchBird dialect

## Supported Window Functions

| Function | Args | ORDER BY Required |
| --- | --- | --- |
| ROW_NUMBER | 0 | Yes |
| RANK | 0 | Yes |
| DENSE_RANK | 0 | Yes |
| PERCENT_RANK | 0 | Yes |
| CUME_DIST | 0 | Yes |
| LAG | 1..3 | Yes |
| LEAD | 1..3 | Yes |
| FIRST_VALUE | 1 | Yes |
| LAST_VALUE | 1 | Yes |
| NTH_VALUE | 2 | Yes |

## AST Schema

`WINDOW_CALL`
- `function_name`: string (canonical uppercase)
- `args`: list<EXPR>
- `window_spec`: WINDOW_SPEC

`WINDOW_SPEC`
- `partition_by`: list<EXPR>
- `order_by`: ORDER_BY_LIST
- `frame`: WINDOW_FRAME (optional)

`WINDOW_FRAME`
- `unit`: enum { ROWS, RANGE, GROUPS }
- `start_bound`: WINDOW_BOUND
- `end_bound`: WINDOW_BOUND
- `explicit_between`: bool

`WINDOW_BOUND`
- `kind`: enum { UNBOUNDED_PRECEDING, UNBOUNDED_FOLLOWING, CURRENT_ROW, PRECEDING, FOLLOWING }
- `expr`: EXPR (required only for PRECEDING/FOLLOWING)

## SBLR V3 Emission

Window functions emit a window opcode with `SCHEMA_WINDOW_CALL`.

1. Emit the function opcode:
   - `ROW_NUMBER` -> `SBLR3_WIN_ROW_NUMBER`
   - `RANK` -> `SBLR3_WIN_RANK`
   - `DENSE_RANK` -> `SBLR3_WIN_DENSE_RANK`
   - `PERCENT_RANK` -> `SBLR3_WIN_PERCENT_RANK`
   - `CUME_DIST` -> `SBLR3_WIN_CUME_DIST`
   - `LAG` -> `SBLR3_WIN_LAG`
   - `LEAD` -> `SBLR3_WIN_LEAD`
   - `FIRST_VALUE` -> `SBLR3_WIN_FIRST_VALUE`
   - `LAST_VALUE` -> `SBLR3_WIN_LAST_VALUE`
   - `NTH_VALUE` -> `SBLR3_WIN_NTH_VALUE`
2. Emit arguments as `expr_list` payload.
3. Emit `WINDOW_SPEC`:
   - `SBLR3_WINDOW_SPEC` with payload `SCHEMA_WINDOW_SPEC`.
   - If `order_by` present, emit `SBLR3_WINDOW_ORDER_BY`.
   - If frame present, emit a frame opcode using `SBLR3_FRAME_ROWS`,
     `SBLR3_FRAME_RANGE`, or `SBLR3_FRAME_GROUPS` with `SCHEMA_WINDOW_FRAME`.

Payloads and schema are defined in `SBLR_V3_OPCODE_PAYLOADS.md`.

## Execution Semantics

### Partitioning and Ordering

- Rows are partitioned by evaluating `partition_by` expressions.
- Within each partition, rows are ordered using the `ORDER BY` list and
  the global `ORDER BY` semantics (including null ordering and collation).
- If `ORDER BY` is required but missing, execution MUST fail with `42883`.

### Frame Defaults

- If `ORDER BY` is present and no frame clause is supplied:
  `RANGE BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`.
- If `ORDER BY` is absent and no frame clause is supplied:
  `RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING`.

### Frame Bounds

- `CURRENT ROW` in `ROWS` mode refers to the current physical row.
- `CURRENT ROW` in `RANGE`/`GROUPS` mode refers to the peer group
  (rows with equal `ORDER BY` key).
- `<expr> PRECEDING/FOLLOWING`:
  - For `ROWS`, `<expr>` MUST evaluate to a non-negative integer row count.
  - For `RANGE`, `<expr>` MUST be a numeric or interval type consistent
    with the first `ORDER BY` expression.
  - For `GROUPS`, `<expr>` MUST evaluate to a non-negative integer peer count.

### Function Semantics

- `ROW_NUMBER`: 1-based position of the row in the ordered partition.
- `RANK`: 1-based rank with gaps for ties.
- `DENSE_RANK`: 1-based rank without gaps.
- `PERCENT_RANK`: `(rank - 1) / (partition_rows - 1)`; if partition has 1 row, result is 0.
- `CUME_DIST`: `count(rows_with_key <= current_key) / partition_rows`.
- `LAG(expr, offset = 1, default = NULL)`: value of `expr` from `offset` rows before.
- `LEAD(expr, offset = 1, default = NULL)`: value of `expr` from `offset` rows after.
- `FIRST_VALUE(expr)`: first value in the frame.
- `LAST_VALUE(expr)`: last value in the frame.
- `NTH_VALUE(expr, n)`: nth value in the frame; `n` must be >= 1.

### Error Conditions

- Invalid frame bound types -> `42883`.
- Negative offsets or `NTH_VALUE` with `n <= 0` -> `22003`.
- `RANGE` without `ORDER BY` -> `0A000`.
- Unsupported clauses (EXCLUDE, named windows in ScratchBird dialect) -> `0A000`.

## Dialect Emulation Rules

- PostgreSQL 16+ emulation:
  - Named windows are permitted and MUST be resolved to an inline `WINDOW_SPEC`.
  - Unsupported clauses must map to `0A000` unless the PostgreSQL dialect
    explicitly defines a different error in its emulation spec.
- MySQL 8.x emulation:
  - Reject `GROUPS` and `RANGE` bounds that MySQL does not support with `0A000`.
- Firebird 5.x emulation:
  - Firebird lacks window frames; any frame clause MUST be rejected with `0A000`.
