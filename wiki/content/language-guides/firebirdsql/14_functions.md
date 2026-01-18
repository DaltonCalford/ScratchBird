[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Functions

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document lists the function surface supported by the Firebird emulation
parser. The Firebird parser feeds the native semantic analyzer, so the built-in
function set mirrors native V2 where noted.

**Parser Pipeline:** Firebird Parser -> AST v2 -> SemanticAnalyzerV2 -> BytecodeGeneratorV2 -> Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/firebird/firebird_parser.cpp`
- Semantic: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`
- Bytecode: `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## Implemented Function Catalog (Firebird Emulation)

### Aggregate Functions

- `COUNT(expr|*)`
- `SUM(expr)`
- `AVG(expr)`
- `MIN(expr)`
- `MAX(expr)`
- `STDDEV(expr)`, `STDDEV_SAMP(expr)`, `STDDEV_POP(expr)`
- `VARIANCE(expr)`, `VAR_SAMP(expr)`, `VAR_POP(expr)`
- `CORR(x, y)`, `COVAR_POP(x, y)`
- `ARRAY_AGG(expr)`

### String Functions

- `LENGTH(text)`
- `CHAR_LENGTH(text)`
- `OCTET_LENGTH(text)`
- `UPPER(text)`, `LOWER(text)`
- `TRIM(text)`, `LTRIM(text)`, `RTRIM(text)`
- `SUBSTRING(text, start, length)`
- `CONCAT(a, b, ...)`
- `CONCAT_WS(sep, a, b, ...)`
- `FORMAT_TYPE(oid, modifier)`
- `OBJ_DESCRIPTION(oid, catalog)`
- `COL_DESCRIPTION(table_oid, column_num)`
- `SHOBJ_DESCRIPTION(oid, catalog)`

### Date/Time Functions

- `NOW()`, `CURRENT_TIMESTAMP()`
- `CURRENT_DATE()`
- `CURRENT_TIME()`
- `DATE_ADD(date, interval)`
- `DATE_SUB(date, interval)`
- `DATE_DIFF(a, b)`, `DATEDIFF(a, b)`

### JSON / JSONB Functions

- `JSON_EXTRACT(json, path)`
- `JSON_SET(json, path, value)`
- `JSON_INSERT(json, path, value)`
- `JSON_REMOVE(json, path)`
- `JSON_OBJECT(key, value, ...)`
- `JSON_ARRAY(value, ...)`
- `JSONB_EXTRACT_PATH(jsonb, path, ...)`
- `JSONB_BUILD_OBJECT(key, value, ...)`
- `JSONB_BUILD_ARRAY(value, ...)`
- `JSONB_SET(jsonb, path, value)`

### Spatial Functions

- `ST_POINT(x, y[, srid])`
- `ST_MAKELINE(...)`
- `ST_MAKEPOLYGON(...)`
- `ST_ASTEXT(geom)`
- `ST_ASBINARY(geom)`
- `ST_GEOMETRYTYPE(geom)`
- `ST_ISVALID(geom)`

### Math Functions

- `ABS(x)`
- `SIGN(x)`
- `ROUND(x[, scale])`
- `CEIL(x)`, `FLOOR(x)`, `TRUNC(x)`
- `MOD(a, b)`
- `SQRT(x)`, `CBRT(x)`
- `POWER(a, b)`, `POW(a, b)`
- `EXP(x)`, `LN(x)`, `LOG(x)`, `LOG10(x)`, `LOG2(x)`
- `SIN(x)`, `COS(x)`, `TAN(x)`
- `ASIN(x)`, `ACOS(x)`, `ATAN(x)`, `ATAN2(y, x)`
- `DEGREES(x)`, `RADIANS(x)`, `PI()`
- `SINH(x)`, `COSH(x)`, `TANH(x)`
- `ASINH(x)`, `ACOSH(x)`, `ATANH(x)`
- `COT(x)`

### Conditional Functions

- `COALESCE(a, b, ...)`
- `NULLIF(a, b)`

---

## Firebird Context Variables

Firebird exposes keyword-style context variables (no parentheses):

- Implemented: `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`
- Parsed but unresolved: `CURRENT_USER`, `CURRENT_ROLE`,
  `CURRENT_CONNECTION`, `CURRENT_TRANSACTION`
- Missing: `LOCALTIME`, `LOCALTIMESTAMP`, `TODAY`, `YESTERDAY`, `TOMORROW`

---

## Status Notes

- Functions not listed above are not supported in the Firebird emulation parser.
- The Firebird parser currently does not enforce a dialect-specific whitelist,
  so any built-in listed above is callable even if not part of Firebird SQL.
  For strict 1:1 parity, this list must be filtered to the Firebird function
  set and all others should be rejected.
- Firebird-specific functions such as `GEN_ID`, `RDB$SET_CONTEXT`,
  `RDB$GET_CONTEXT`, and `DATEADD` are not implemented in the current parser
  pipeline.

---

## Strict Parity Whitelist (Firebird 5.0)

Dialect source: `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`

| Function (as parsed) | Dialect allows? | Parity action | Notes |
| --- | --- | --- | --- |
| COUNT, SUM, AVG, MIN, MAX | Yes | Allow | Aggregate functions |
| STDDEV, STDDEV_SAMP, STDDEV_POP | Yes | Allow | Statistical aggregates |
| VAR_SAMP, VAR_POP | Yes | Allow | Statistical aggregates |
| VARIANCE | No | Block | Firebird uses VAR_SAMP/VAR_POP |
| CORR, COVAR_POP | Yes | Allow | Statistical aggregates |
| ARRAY_AGG | No | Block | Firebird uses LIST() |
| CHAR_LENGTH, CHARACTER_LENGTH | Yes | Allow | String length |
| OCTET_LENGTH | Yes | Allow | String length |
| LENGTH | No | Block | Not a Firebird scalar function |
| UPPER, LOWER | Yes | Allow | String case |
| TRIM | Yes | Allow | String trim |
| LTRIM, RTRIM | No | Block | Not in Firebird scalar functions |
| SUBSTRING | Yes | Allow | String slicing |
| CONCAT, CONCAT_WS | No | Block | Use `||` concatenation |
| FORMAT_TYPE, OBJ_DESCRIPTION, COL_DESCRIPTION, SHOBJ_DESCRIPTION | No | Block | PostgreSQL-only |
| CURRENT_DATE | Yes (context) | Allow (syntax fix) | Keyword/context variable, not function call |
| CURRENT_TIME | Yes (context) | Allow (syntax fix) | Keyword/context variable, not function call |
| CURRENT_TIMESTAMP | Yes (context) | Allow (syntax fix) | Keyword/context variable, not function call |
| NOW | No | Block | Use CURRENT_TIMESTAMP |
| DATE_ADD, DATE_SUB | No | Block | Firebird uses DATEADD |
| DATE_DIFF | No | Block | Firebird uses DATEDIFF |
| DATEDIFF | Yes | Allow | Date/time function |
| JSON_EXTRACT, JSON_SET, JSON_INSERT, JSON_REMOVE | No | Block | No JSON scalar functions |
| JSON_OBJECT, JSON_ARRAY | No | Block | No JSON scalar functions |
| JSONB_EXTRACT_PATH, JSONB_BUILD_OBJECT, JSONB_BUILD_ARRAY, JSONB_SET | No | Block | PostgreSQL-only |
| ST_POINT, ST_MAKELINE, ST_MAKEPOLYGON | No | Block | No spatial scalar functions |
| ST_ASTEXT, ST_ASBINARY, ST_GEOMETRYTYPE, ST_ISVALID | No | Block | No spatial scalar functions |
| ABS, SIGN, ROUND, CEIL/CEILING, FLOOR, TRUNC | Yes | Allow | Math |
| MOD, SQRT, POWER, EXP, LN, LOG, LOG10 | Yes | Allow | Math |
| POW | No | Block | Use POWER |
| LOG2, CBRT | No | Block | Not in Firebird math functions |
| SIN, COS, TAN, ASIN, ACOS, ATAN, ATAN2 | Yes | Allow | Math |
| DEGREES, RADIANS | No | Block | Not in Firebird math functions |
| SINH, COSH, TANH, ASINH, ACOSH, ATANH, COT | Yes | Allow | Math |
| COALESCE, NULLIF | Yes | Allow | Conditional |
