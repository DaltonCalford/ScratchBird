# V3 Parser: SELECT and Query Forms (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative SELECT/query statement forms for the ScratchBird V3
parser and how each form MUST emit SBLR V3 opcodes.

## Parsing Rules (Authoritative)

1. Enter DML parse mode.
2. Parse optional WITH clause (CTEs) in order.
3. Parse query expression (SELECT, VALUES, or set operations).
4. Parse ORDER BY, LIMIT/OFFSET, FETCH, and locking clauses.
5. Emit SBLR query opcodes with child expressions in declaration order.

## Emission Rules (Authoritative)

Emit:
- `SBLR3_QUERY_SELECT` for SELECT blocks.
- `SBLR3_QUERY_SETOP` for UNION/INTERSECT/EXCEPT.
- `SBLR3_QUERY_VALUES` for VALUES-only queries.
- `SBLR3_QUERY_ORDER_BY`, `SBLR3_QUERY_LIMIT`, `SBLR3_QUERY_OFFSET`,
  `SBLR3_QUERY_FETCH`, `SBLR3_QUERY_LOCK` for trailing clauses.
- `SBLR3_QUERY_CTE` for each WITH entry.

## Statement Forms

### Core SELECT

```
[WITH <cte_list>]
SELECT [ALL|DISTINCT] <select_list>
FROM <table_ref_list>
[WHERE <expr>]
[GROUP BY <expr_list>]
[HAVING <expr>]
[WINDOW <window_def_list>]
[ORDER BY <order_list>]
[LIMIT <expr> [OFFSET <expr>]]
[FETCH { FIRST | NEXT } <expr> ROWS { ONLY | WITH TIES }]
[FOR { UPDATE | SHARE } [OF <table_list>]]
```

### VALUES Query

```
[WITH <cte_list>]
VALUES (<expr_list>)[, ...]
[ORDER BY <order_list>]
[LIMIT <expr> [OFFSET <expr>]]
```

### Set Operations

```
<query> { UNION | INTERSECT | EXCEPT } [ALL | DISTINCT] <query>
```

Set operation precedence: parentheses > INTERSECT > UNION/EXCEPT.

## Errors

- Missing select list: `ERR_PARSE_EXPECTED_SELECT_LIST`.
- DISTINCT on unsupported dialect: `ERR_FEATURE_NOT_SUPPORTED`.
- ORDER BY on set operations without column references: `ERR_INVALID_ORDER_BY`.

## Related Specs

- `JOINS.md`
- `WINDOWING.md`
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
