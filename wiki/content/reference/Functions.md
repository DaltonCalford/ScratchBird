# Functions Reference

**Last Updated:** 2026-01-28

---

## Overview

ScratchBird provides a comprehensive set of built-in functions. Functions are case-insensitive at call sites.

---

## Aggregate Functions

Functions that operate on multiple rows to return a single value.

| Function | Description | Example |
|----------|-------------|---------|
| `COUNT(expr\|*)` | Count rows | `SELECT COUNT(*) FROM users` |
| `SUM(expr)` | Sum numeric values | `SELECT SUM(amount) FROM orders` |
| `AVG(expr)` | Average of values | `SELECT AVG(price) FROM products` |
| `MIN(expr)` | Minimum value | `SELECT MIN(created_at) FROM posts` |
| `MAX(expr)` | Maximum value | `SELECT MAX(score) FROM results` |
| `STDDEV(expr)` | Standard deviation (sample) | `SELECT STDDEV(price) FROM products` |
| `STDDEV_SAMP(expr)` | Standard deviation (sample) | Alias for STDDEV |
| `STDDEV_POP(expr)` | Standard deviation (population) | `SELECT STDDEV_POP(price) FROM products` |
| `VARIANCE(expr)` | Variance (sample) | `SELECT VARIANCE(score) FROM results` |
| `VAR_SAMP(expr)` | Variance (sample) | Alias for VARIANCE |
| `VAR_POP(expr)` | Variance (population) | `SELECT VAR_POP(score) FROM results` |
| `CORR(x, y)` | Correlation coefficient | `SELECT CORR(x, y) FROM points` |
| `COVAR_POP(x, y)` | Population covariance | `SELECT COVAR_POP(x, y) FROM points` |
| `ARRAY_AGG(expr)` | Aggregate into array | `SELECT ARRAY_AGG(name) FROM users` |

---

## String Functions

| Function | Description | Example |
|----------|-------------|---------|
| `LENGTH(text)` | String length in characters | `SELECT LENGTH('hello')` → 5 |
| `CHAR_LENGTH(text)` | Alias for LENGTH | `SELECT CHAR_LENGTH('hello')` → 5 |
| `OCTET_LENGTH(text)` | String length in bytes | `SELECT OCTET_LENGTH('hello')` → 5 |
| `UPPER(text)` | Convert to uppercase | `SELECT UPPER('hello')` → 'HELLO' |
| `LOWER(text)` | Convert to lowercase | `SELECT LOWER('HELLO')` → 'hello' |
| `TRIM(text)` | Remove leading/trailing whitespace | `SELECT TRIM('  hi  ')` → 'hi' |
| `LTRIM(text)` | Remove leading whitespace | `SELECT LTRIM('  hi')` → 'hi' |
| `RTRIM(text)` | Remove trailing whitespace | `SELECT RTRIM('hi  ')` → 'hi' |
| `SUBSTRING(text, start, length)` | Extract substring | `SELECT SUBSTRING('hello', 2, 3)` → 'ell' |
| `CONCAT(a, b, ...)` | Concatenate strings | `SELECT CONCAT('a', 'b', 'c')` → 'abc' |
| `CONCAT_WS(sep, a, b, ...)` | Concatenate with separator | `SELECT CONCAT_WS('-', 'a', 'b')` → 'a-b' |

### Catalog Description Functions

| Function | Description | Example |
|----------|-------------|---------|
| `FORMAT_TYPE(oid, modifier)` | Format type name | `SELECT FORMAT_TYPE(25, -1)` → 'text' |
| `OBJ_DESCRIPTION(oid, catalog)` | Get object description | `SELECT OBJ_DESCRIPTION(123, 'pg_class')` |
| `COL_DESCRIPTION(table_oid, col)` | Get column description | `SELECT COL_DESCRIPTION(123, 1)` |
| `SHOBJ_DESCRIPTION(oid, catalog)` | Shared object description | `SELECT SHOBJ_DESCRIPTION(123, 'pg_database')` |

---

## Date/Time Functions

| Function | Description | Example |
|----------|-------------|---------|
| `NOW()` | Current timestamp | `SELECT NOW()` |
| `CURRENT_TIMESTAMP()` | Alias for NOW() | `SELECT CURRENT_TIMESTAMP` |
| `CURRENT_DATE()` | Current date | `SELECT CURRENT_DATE` |
| `CURRENT_TIME()` | Current time | `SELECT CURRENT_TIME` |
| `DATE_ADD(date, interval)` | Add interval to date | `SELECT DATE_ADD(NOW(), INTERVAL '1 day')` |
| `DATE_SUB(date, interval)` | Subtract interval from date | `SELECT DATE_SUB(NOW(), INTERVAL '1 hour')` |
| `DATE_DIFF(a, b)` | Difference between dates | `SELECT DATE_DIFF(date1, date2)` |
| `DATEDIFF(a, b)` | Alias for DATE_DIFF | `SELECT DATEDIFF(date1, date2)` |

---

## Math Functions

### Basic Math

| Function | Description | Example |
|----------|-------------|---------|
| `ABS(x)` | Absolute value | `SELECT ABS(-5)` → 5 |
| `SIGN(x)` | Sign of number (-1, 0, 1) | `SELECT SIGN(-5)` → -1 |
| `ROUND(x[, scale])` | Round to precision | `SELECT ROUND(3.14159, 2)` → 3.14 |
| `CEIL(x)` | Round up to integer | `SELECT CEIL(3.2)` → 4 |
| `FLOOR(x)` | Round down to integer | `SELECT FLOOR(3.8)` → 3 |
| `TRUNC(x)` | Truncate toward zero | `SELECT TRUNC(3.8)` → 3 |
| `MOD(a, b)` | Modulo (remainder) | `SELECT MOD(10, 3)` → 1 |

### Powers and Roots

| Function | Description | Example |
|----------|-------------|---------|
| `SQRT(x)` | Square root | `SELECT SQRT(16)` → 4 |
| `CBRT(x)` | Cube root | `SELECT CBRT(27)` → 3 |
| `POWER(a, b)` | a raised to power b | `SELECT POWER(2, 10)` → 1024 |
| `POW(a, b)` | Alias for POWER | `SELECT POW(2, 10)` → 1024 |

### Logarithms and Exponentials

| Function | Description | Example |
|----------|-------------|---------|
| `EXP(x)` | e raised to power x | `SELECT EXP(1)` → 2.718... |
| `LN(x)` | Natural logarithm | `SELECT LN(2.718)` → ~1 |
| `LOG(x)` | Natural logarithm | `SELECT LOG(10)` → 2.302... |
| `LOG10(x)` | Base-10 logarithm | `SELECT LOG10(100)` → 2 |
| `LOG2(x)` | Base-2 logarithm | `SELECT LOG2(8)` → 3 |

### Trigonometric Functions

| Function | Description | Example |
|----------|-------------|---------|
| `SIN(x)` | Sine (radians) | `SELECT SIN(0)` → 0 |
| `COS(x)` | Cosine (radians) | `SELECT COS(0)` → 1 |
| `TAN(x)` | Tangent (radians) | `SELECT TAN(0)` → 0 |
| `ASIN(x)` | Arcsine | `SELECT ASIN(0)` → 0 |
| `ACOS(x)` | Arccosine | `SELECT ACOS(1)` → 0 |
| `ATAN(x)` | Arctangent | `SELECT ATAN(0)` → 0 |
| `ATAN2(y, x)` | Two-argument arctangent | `SELECT ATAN2(1, 1)` → π/4 |
| `COT(x)` | Cotangent | `SELECT COT(1)` |

### Hyperbolic Functions

| Function | Description | Example |
|----------|-------------|---------|
| `SINH(x)` | Hyperbolic sine | `SELECT SINH(0)` → 0 |
| `COSH(x)` | Hyperbolic cosine | `SELECT COSH(0)` → 1 |
| `TANH(x)` | Hyperbolic tangent | `SELECT TANH(0)` → 0 |
| `ASINH(x)` | Inverse hyperbolic sine | `SELECT ASINH(0)` → 0 |
| `ACOSH(x)` | Inverse hyperbolic cosine | `SELECT ACOSH(1)` → 0 |
| `ATANH(x)` | Inverse hyperbolic tangent | `SELECT ATANH(0)` → 0 |

### Angle Conversion

| Function | Description | Example |
|----------|-------------|---------|
| `DEGREES(x)` | Radians to degrees | `SELECT DEGREES(PI())` → 180 |
| `RADIANS(x)` | Degrees to radians | `SELECT RADIANS(180)` → π |
| `PI()` | Value of π | `SELECT PI()` → 3.14159... |

---

## JSON Functions

| Function | Description | Example |
|----------|-------------|---------|
| `JSON_EXTRACT(json, path)` | Extract value by path | `SELECT JSON_EXTRACT('{"a":1}', '$.a')` |
| `JSON_SET(json, path, value)` | Set value at path | `SELECT JSON_SET('{}', '$.a', 1)` |
| `JSON_INSERT(json, path, value)` | Insert if not exists | `SELECT JSON_INSERT('{}', '$.a', 1)` |
| `JSON_REMOVE(json, path)` | Remove value at path | `SELECT JSON_REMOVE('{"a":1}', '$.a')` |
| `JSON_OBJECT(key, value, ...)` | Build JSON object | `SELECT JSON_OBJECT('a', 1, 'b', 2)` |
| `JSON_ARRAY(value, ...)` | Build JSON array | `SELECT JSON_ARRAY(1, 2, 3)` |

### JSONB Functions

| Function | Description | Example |
|----------|-------------|---------|
| `JSONB_EXTRACT_PATH(jsonb, ...)` | Extract by path components | `SELECT JSONB_EXTRACT_PATH(data, 'a', 'b')` |
| `JSONB_BUILD_OBJECT(key, val, ...)` | Build JSONB object | `SELECT JSONB_BUILD_OBJECT('a', 1)` |
| `JSONB_BUILD_ARRAY(val, ...)` | Build JSONB array | `SELECT JSONB_BUILD_ARRAY(1, 2)` |
| `JSONB_SET(jsonb, path, value)` | Set value in JSONB | `SELECT JSONB_SET(data, '{a}', '1')` |

---

## Spatial Functions

| Function | Description | Example |
|----------|-------------|---------|
| `ST_POINT(x, y[, srid])` | Create point geometry | `SELECT ST_POINT(0, 0)` |
| `ST_MAKELINE(...)` | Create line from points | `SELECT ST_MAKELINE(p1, p2)` |
| `ST_MAKEPOLYGON(...)` | Create polygon | `SELECT ST_MAKEPOLYGON(ring)` |
| `ST_ASTEXT(geom)` | Convert to WKT | `SELECT ST_ASTEXT(geom)` |
| `ST_ASBINARY(geom)` | Convert to WKB | `SELECT ST_ASBINARY(geom)` |
| `ST_GEOMETRYTYPE(geom)` | Get geometry type | `SELECT ST_GEOMETRYTYPE(geom)` |
| `ST_ISVALID(geom)` | Check validity | `SELECT ST_ISVALID(geom)` |

---

## Conditional Functions

| Function | Description | Example |
|----------|-------------|---------|
| `COALESCE(a, b, ...)` | Return first non-NULL | `SELECT COALESCE(NULL, 'default')` → 'default' |
| `NULLIF(a, b)` | Return NULL if equal | `SELECT NULLIF(1, 1)` → NULL |

---

## Dialect-Specific Functions

### PostgreSQL Emulation

When connecting via the PostgreSQL port (5432), additional pg_catalog functions are available:

- `pg_catalog.version()` - Returns ScratchBird version in PostgreSQL format
- `pg_typeof(expr)` - Returns type name
- `current_database()` - Current database name
- `current_schema()` - Current schema name

### MySQL Emulation

When connecting via the MySQL port (3306):

- `DATABASE()` - Current database name
- `VERSION()` - Server version
- `USER()` - Current user

### Firebird Emulation

When connecting via the Firebird port (3050):

- `GEN_ID(generator, increment)` - Generate sequence value
- `CAST(expr AS type)` - Type casting

---

## Status Notes

- Functions listed above are implemented in the native V2 parser
- Functions not listed require UDF/UDR registration
- `CONVERT()` and `COLLATE()` are not available as built-ins
- Function availability may vary by dialect/port

---

## Related Documents

- [SQL Syntax Reference](SQL-Syntax.md)
- [Operators Reference](Operators.md)
- [Data Types Reference](Data-Types.md)
- [Language Guides](../language-guides/README.md)
