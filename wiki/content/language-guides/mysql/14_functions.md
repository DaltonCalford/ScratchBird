[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Functions

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document lists the function surface supported by the MySQL emulation
parser. Only functions explicitly mapped to SBLR opcodes are executable.

**Parser Pipeline:** MySQL Parser -> SBLR bytecode -> Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/mysql/mysql_parser.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## Implemented Function Catalog

### Aggregate Functions

- `COUNT(expr|*)`
- `SUM(expr)`
- `AVG(expr)`
- `MIN(expr)`
- `MAX(expr)`

### String Functions

- `LENGTH(text)`
- `CHAR_LENGTH(text)`, `CHARACTER_LENGTH(text)`
- `UPPER(text)`, `UCASE(text)`
- `LOWER(text)`, `LCASE(text)`
- `SUBSTRING(text, start, length)`, `SUBSTR`, `MID`
- `TRIM(text)`

### Date/Time Functions

- `NOW()`
- `CURRENT_TIMESTAMP()`
- `CURDATE()`, `CURRENT_DATE()`

### Math Functions

- `ABS(x)`
- `ROUND(x[, scale])`
- `FLOOR(x)`
- `CEIL(x)`, `CEILING(x)`
- `SQRT(x)`
- `POWER(a, b)`, `POW(a, b)`

### JSON Functions

- `JSON_EXTRACT(json, path)`
- `JSON_OBJECT(key, value, ...)`

### Conditional Functions

- `COALESCE(a, b, ...)`
- `IFNULL(a, b)`, `NVL(a, b)`
- `NULLIF(a, b)`
- `IF(cond, then, else)`

### Special Context Function

- `VALUES(column)` (only inside `ON DUPLICATE KEY UPDATE`)

---

## Status Notes

- Function names outside this list are emitted as a generic `EXT_CALL`, but the
  executor does not support expression-level `EXT_CALL`. Treat them as missing.
- `CURRENT_TIME()`, `LOCALTIME()`, `LOCALTIMESTAMP()`, and MySQL date/time
  helpers like `DATE_ADD()` are not implemented in the parser.

---

## Strict Parity Whitelist (MySQL 8.x)

Dialect source: `ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`

| Function (as parsed) | Dialect allows? | Parity action | Notes |
| --- | --- | --- | --- |
| COUNT, SUM, AVG, MIN, MAX | Yes | Allow | Aggregate functions |
| LENGTH, CHAR_LENGTH, CHARACTER_LENGTH | Yes | Allow | String length |
| UPPER/UCASE, LOWER/LCASE | Yes | Allow | Case conversion |
| SUBSTRING/SUBSTR/MID | Yes | Allow | String slicing |
| TRIM | Yes | Allow | String trim |
| NOW, CURRENT_TIMESTAMP | Yes | Allow | Date/time |
| CURDATE, CURRENT_DATE | Yes | Allow | Date |
| ABS, ROUND, FLOOR, CEIL/CEILING, SQRT | Yes | Allow | Math |
| POWER/POW | Yes | Allow | Math |
| JSON_EXTRACT, JSON_OBJECT | Yes | Allow | JSON |
| COALESCE, IFNULL, NULLIF, IF | Yes | Allow | Conditional |
| NVL | No | Block | Oracle alias, not MySQL |
| VALUES(column) | Yes | Allow (restricted) | Only inside `ON DUPLICATE KEY UPDATE` |
