[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - Functions

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document lists the function surface supported by the PostgreSQL emulation
parser. Only functions explicitly mapped to SBLR opcodes are executable.

**Parser Pipeline:** PostgreSQL Parser -> SBLR bytecode -> Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/postgresql/pg_parser_expr.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## Implemented Function Catalog

### Aggregate Functions

- `COUNT(expr|*)`
- `SUM(expr)`
- `AVG(expr)`
- `MIN(expr)`
- `MAX(expr)`

### Window Functions

- `ROW_NUMBER()`
- `RANK()`
- `DENSE_RANK()`
- `LAG(expr[, offset[, default]])`
- `LEAD(expr[, offset[, default]])`

### String Functions

- `UPPER(text)`
- `LOWER(text)`
- `LENGTH(text)`
- `CHAR_LENGTH(text)`, `CHARACTER_LENGTH(text)`
- `SUBSTRING(text FROM start FOR length)` (parser accepts `SUBSTRING(a, b, c)`)
- `TRIM(text)`

### Date/Time Functions

- `NOW()`
- `CURRENT_TIMESTAMP()`
- `CURRENT_DATE()`

### Math Functions

- `ABS(x)`
- `SQRT(x)`
- `ROUND(x[, scale])`

### JSON Functions

- `JSON_OBJECT(key, value, ...)`
- `JSONB_BUILD_OBJECT(key, value, ...)`

### Conditional Functions

- `COALESCE(a, b, ...)`
- `NULLIF(a, b)`

---

## Status Notes

- Keyword-style context variables (`CURRENT_DATE`, `CURRENT_USER`, etc.) are not
  parsed as expressions; only the function-call forms listed above work.
- Function names outside this list are emitted as a generic `EXT_CALL`, but the
  executor does not support expression-level `EXT_CALL`. Treat them as missing.

---

## Strict Parity Whitelist (PostgreSQL 16)

Dialect source: `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`

| Function (as parsed) | Dialect allows? | Parity action | Notes |
| --- | --- | --- | --- |
| COUNT, SUM, AVG, MIN, MAX | Yes | Allow | Aggregate functions |
| ROW_NUMBER, RANK, DENSE_RANK | Yes | Allow | Window functions |
| LAG, LEAD | Yes | Allow | Window functions |
| UPPER, LOWER | Yes | Allow | String case |
| LENGTH, CHAR_LENGTH, CHARACTER_LENGTH | Yes | Allow | String length |
| SUBSTRING, TRIM | Yes | Allow | String slicing/trim |
| NOW | Yes | Allow | Date/time |
| CURRENT_TIMESTAMP | Yes (keyword) | Allow (syntax fix) | Keyword form only; parser accepts call form |
| CURRENT_DATE | Yes (keyword) | Allow (syntax fix) | Keyword form only; parser accepts call form |
| ABS, SQRT, ROUND | Yes | Allow | Math |
| JSONB_BUILD_OBJECT | Yes | Allow | JSONB |
| JSON_OBJECT | No | Block | PostgreSQL uses JSON_BUILD_OBJECT instead |
| COALESCE, NULLIF | Yes | Allow | Conditional |
