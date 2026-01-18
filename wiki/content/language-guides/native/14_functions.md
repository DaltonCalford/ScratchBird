[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native V2 SQL - Functions

## Overview

This document lists the built-in functions supported by the native V2 parser
and executable by the core engine. Functions not listed here are either
missing or require explicit user-defined registration (UDR/UDF).

**Parser Pipeline:** V2 Parser -> AST v2 -> SemanticAnalyzerV2 -> BytecodeGeneratorV2 -> Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- Semantic: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`
- Bytecode: `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## Implemented Function Catalog

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

## Status Notes

- Functions are case-insensitive at call sites.
- Functions not listed above are not available in the native V2 parser unless
  provided as catalog-defined functions (UDF/UDR).
- `CONVERT()` and `COLLATE()` are not resolved as built-ins in the semantic
  analyzer (even though opcodes exist), so they are currently unavailable.
