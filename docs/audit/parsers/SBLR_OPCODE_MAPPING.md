# SBLR Opcode Mapping Summary

**Document:** SBLR (ScratchBird Bytecode Language Runtime) Opcode Reference
**Source:** `/include/scratchbird/sblr/opcodes.h`
**Total Opcodes:** 300+ opcodes across core (1-byte) and extended (2-byte with 0xFF prefix)
**Audit Date:** 2026-01-07

---

## Purpose

This document maps SQL functionality to SBLR bytecode opcodes to:
1. Identify which SQL features are supported by the executor
2. Show which parsers emit which opcodes
3. Identify missing implementations (features with no opcode or opcodes with no parser support)

---

## Core Opcodes (Single Byte: 0x00-0xFF)

### DDL Statements (0x10-0x1F range)

| Opcode | Value | SQL Statement | V2 | Firebird | PostgreSQL | MySQL |
|--------|-------|---------------|----|----|----|----|
| CREATE_TABLE | 0x10 | CREATE TABLE | ✅ | ✅ | ✅ | ✅ |
| INSERT | 0x11 | INSERT INTO | ✅ | ✅ | ✅ | ✅ |
| SELECT | 0x12 | SELECT | ✅ | ✅ | ✅ | ✅ |
| START_TRANSACTION | 0x13 | BEGIN / START TRANSACTION | ✅ | ✅ | ✅ | ✅ |
| COMMIT | 0x14 | COMMIT | ✅ | ✅ | ✅ | ✅ |
| ROLLBACK | 0x15 | ROLLBACK | ✅ | ✅ | ✅ | ✅ |
| CREATE_INDEX | 0x1B | CREATE INDEX | ❌ | ✅ | ✅ | ❌ |
| DROP_TABLE | 0x1F | DROP TABLE | ✅ | ✅ | ✅ | ❌ |

### Data Types (0x20-0x2F range)

| Opcode | Value | Data Type | Notes |
|--------|-------|-----------|-------|
| TYPE_INTEGER | 0x20 | INTEGER/INT | 32-bit signed |
| TYPE_BIGINT | 0x21 | BIGINT | 64-bit signed |
| TYPE_DOUBLE | 0x22 | DOUBLE PRECISION | 64-bit float |
| TYPE_VARCHAR | 0x23 | VARCHAR | Variable length string |
| TYPE_BOOLEAN | 0x24 | BOOLEAN | True/false |
| TYPE_DATE | 0x25 | DATE | Calendar date |
| TYPE_TIME | 0x26 | TIME | Time of day |
| TYPE_TIMESTAMP | 0x27 | TIMESTAMP | Date + time |
| TYPE_BLOB | 0x28 | BLOB | Binary large object |
| TYPE_ARRAY | 0x29 | ARRAY | Array type |
| TYPE_JSON | 0x2A | JSON/JSONB | JSON document |
| TYPE_UUID | 0x2B | UUID | Universally unique ID |
| TYPE_NUMERIC | 0x2C | NUMERIC/DECIMAL | Arbitrary precision |

### Expression Operators (0x50-0x7F range)

| Opcode | Value | Operator | Description |
|--------|-------|----------|-------------|
| EXPR_ADD | 0x50 | + | Addition |
| EXPR_SUB | 0x51 | - | Subtraction |
| EXPR_MUL | 0x52 | * | Multiplication |
| EXPR_DIV | 0x53 | / | Division |
| EXPR_MOD | 0x54 | % | Modulo |
| EXPR_NEG | 0x55 | - (unary) | Negation |
| EXPR_EQ | 0x60 | = | Equality |
| EXPR_NE | 0x61 | <> / != | Not equal |
| EXPR_LT | 0x62 | < | Less than |
| EXPR_LE | 0x63 | <= | Less than or equal |
| EXPR_GT | 0x64 | > | Greater than |
| EXPR_GE | 0x65 | >= | Greater than or equal |
| EXPR_AND | 0x70 | AND | Logical AND |
| EXPR_OR | 0x71 | OR | Logical OR |
| EXPR_NOT | 0x72 | NOT | Logical NOT |

### Core Functions (0x73-0x7F range)

| Opcode | Value | Function | Description |
|--------|-------|----------|-------------|
| FUNC_LENGTH | 0x73 | LENGTH() | String length |
| FUNC_SUBSTRING | 0x74 | SUBSTRING() | Extract substring |
| FUNC_UPPER | 0x75 | UPPER() | Convert to uppercase |
| FUNC_LOWER | 0x76 | LOWER() | Convert to lowercase |
| FUNC_TRIM | 0x77 | TRIM() | Remove whitespace |
| FUNC_CONCAT | 0x78 | CONCAT() | Concatenate strings |
| FUNC_COALESCE | 0x79 | COALESCE() | First non-null value |
| AGG_SUM | 0x7A | SUM() | Aggregate sum |
| AGG_AVG | 0x7B | AVG() | Aggregate average |
| AGG_COUNT | 0x7C | COUNT() | Aggregate count |
| AGG_MIN | 0x7D | MIN() | Aggregate minimum |
| AGG_MAX | 0x7E | MAX() | Aggregate maximum |

### Control Flow (0x80-0x9F range)

| Opcode | Value | Statement | Description |
|--------|-------|-----------|-------------|
| IF | 0x80 | IF | Conditional branch |
| WHILE | 0x81 | WHILE | Loop |
| FOR | 0x82 | FOR | Iteration loop |
| RETURN | 0x83 | RETURN | Return from function |
| CALL | 0x84 | CALL | Call procedure/function |

### DML Operations (0xC0-0xCF range)

| Opcode | Value | Statement | V2 | Firebird | PostgreSQL | MySQL |
|--------|-------|-----------|----|----|----|----|
| UPDATE | 0xC3 | UPDATE | ✅ | ✅ | ✅ | ✅ |
| DELETE | 0xC4 | DELETE | ✅ | ✅ | ✅ | ✅ |

---

## Extended Opcodes (0xFF prefix + extended byte)

Extended opcodes use 2-byte format: `[0xFF] [extended_opcode]`

### Array Functions (0x01-0x27)

| Opcode | Value | Function | PostgreSQL | MySQL | V2 | Firebird |
|--------|-------|----------|----|----|----|----|
| EXT_ARRAY_APPEND | 0x01 | ARRAY_APPEND(array, elem) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_PREPEND | 0x02 | ARRAY_PREPEND(elem, array) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_CAT | 0x03 | ARRAY_CAT(arr1, arr2) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_REMOVE | 0x04 | ARRAY_REMOVE(array, elem) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_REPLACE | 0x05 | ARRAY_REPLACE(arr, from, to) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_OVERLAP | 0x10 | && (array overlap) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_CONTAINS | 0x11 | @> (array contains) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_CONTAINED_BY | 0x12 | <@ (contained by) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_LENGTH | 0x20 | ARRAY_LENGTH(arr, dim) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_DIMS | 0x21 | ARRAY_DIMS(array) | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_CONSTRUCT | 0x24 | ARRAY[...] constructor | ✅ | ❌ | ✅ | ❌ |
| EXT_ARRAY_SUBSCRIPT | 0x25 | array[index] | ✅ | ❌ | ✅ | ❌ |

### String/Regex Functions (0x30-0x44)

| Opcode | Value | Function | PostgreSQL | MySQL | V2 | Firebird |
|--------|-------|----------|----|----|----|----|
| EXT_REGEX_MATCH | 0x30 | ~ (regex match) | ✅ | ❌ | ⚠️ | ❌ |
| EXT_REGEX_MATCH_CI | 0x31 | ~* (case-insensitive) | ✅ | ❌ | ⚠️ | ❌ |
| EXT_REGEXP_REPLACE | 0x35 | REGEXP_REPLACE() | ✅ | ✅ | ⚠️ | ❌ |
| EXT_SPLIT_PART | 0x38 | SPLIT_PART(str, delim, n) | ✅ | ❌ | ⚠️ | ❌ |
| EXT_INITCAP | 0x40 | INITCAP(str) | ✅ | ❌ | ⚠️ | ❌ |
| EXT_REPEAT | 0x43 | REPEAT(str, count) | ✅ | ✅ | ✅ | ❌ |
| EXT_REVERSE | 0x44 | REVERSE(str) | ✅ | ✅ | ✅ | ❌ |

### Grouping/Aggregation (0x45-0x48)

| Opcode | Value | Feature | PostgreSQL | MySQL | V2 | Firebird |
|--------|-------|---------|----|----|----|----|
| EXT_GROUP_ROLLUP | 0x45 | ROLLUP(...) | ✅ | ✅ | ❌ | ❌ |
| EXT_GROUP_CUBE | 0x46 | CUBE(...) | ✅ | ❌ | ❌ | ❌ |
| EXT_GROUP_GROUPING_SETS | 0x47 | GROUPING SETS(...) | ✅ | ❌ | ❌ | ❌ |
| EXT_GROUPING_FUNC | 0x48 | GROUPING(col) | ✅ | ✅ | ❌ | ❌ |

### XML Functions (0x45-0x4E)

| Opcode | Value | Function | PostgreSQL | V2 | Firebird | MySQL |
|--------|-------|----------|----|----|----|----|
| EXT_XMLPARSE | 0x45 | XMLPARSE() | ✅ | ❌ | ❌ | ❌ |
| EXT_XMLELEMENT | 0x47 | XMLELEMENT() | ✅ | ❌ | ❌ | ❌ |
| EXT_XMLCONCAT | 0x48 | XMLCONCAT() | ✅ | ❌ | ❌ | ❌ |
| EXT_XPATH | 0x4C | XPATH(expr, xml) | ✅ | ❌ | ❌ | ❌ |
| EXT_XMLAGG | 0x4E | XMLAGG(xml) | ✅ | ❌ | ❌ | ❌ |

### MERGE Statement (0x4F-0x55)

| Opcode | Value | Clause | PostgreSQL | Firebird | V2 | MySQL |
|--------|-------|--------|----|----|----|----|
| EXT_MERGE_START | 0x4F | MERGE INTO | ✅ | ✅ | ❌ | ❌ |
| EXT_MERGE_SOURCE | 0x50 | USING source | ✅ | ✅ | ❌ | ❌ |
| EXT_MERGE_ON | 0x51 | ON condition | ✅ | ✅ | ❌ | ❌ |
| EXT_MERGE_WHEN_MATCHED | 0x52 | WHEN MATCHED UPDATE | ✅ | ✅ | ❌ | ❌ |
| EXT_MERGE_WHEN_NOT_MATCHED | 0x53 | WHEN NOT MATCHED INSERT | ✅ | ✅ | ❌ | ❌ |
| EXT_MERGE_END | 0x55 | End MERGE | ✅ | ✅ | ❌ | ❌ |

### Transaction Control (0x56-0x5A)

| Opcode | Value | Statement | All Parsers |
|--------|-------|-----------|-------------|
| EXT_RETURNING | 0x56 | RETURNING clause | ✅ (varies) |
| EXT_ANALYZE | 0x57 | ANALYZE table | ⚠️ (partial) |
| EXT_SAVEPOINT | 0x58 | SAVEPOINT name | ✅ |
| EXT_RELEASE_SAVEPOINT | 0x59 | RELEASE SAVEPOINT | ✅ |
| EXT_ROLLBACK_TO_SAVEPOINT | 0x5A | ROLLBACK TO SAVEPOINT | ✅ |

### User-Defined Types (0x5B-0x5C)

| Opcode | Value | Statement | PostgreSQL | Firebird | V2 | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_CREATE_TYPE | 0x5B | CREATE TYPE (ENUM/RANGE) | ✅ | ❌ | ⚠️ | ❌ |
| EXT_CREATE_DOMAIN | 0x5C | CREATE DOMAIN | ✅ | ✅ | ✅ | ❌ |

### Procedure/Function Calls (0x5D)

| Opcode | Value | Statement | Firebird | PostgreSQL | V2 | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_CALL | 0x5D | CALL procedure() | ✅ | ✅ | ❌ | ✅ |

### SHOW Commands (0x05-0x09, 0x5E-0x7D)

| Opcode | Value | Command | MySQL | PostgreSQL | V2 | Firebird |
|--------|-------|---------|----|----|----|----|
| EXT_SHOW_TABLES | 0x05 | SHOW TABLES | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_DATABASES | 0x06 | SHOW DATABASES | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_COLUMNS | 0x07 | SHOW COLUMNS | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_INDEXES | 0x08 | SHOW INDEXES | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_CREATE_TABLE | 0x09 | SHOW CREATE TABLE | ✅ | ✅ | ✅ | ✅ |
| EXT_DESCRIBE_TABLE | 0x15 | DESCRIBE table | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_TRIGGER | 0x60 | SHOW TRIGGER | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_PROCEDURE | 0x61 | SHOW PROCEDURE | ✅ | ✅ | ⚠️ | ✅ |
| EXT_SHOW_FUNCTION | 0x62 | SHOW FUNCTION | ✅ | ✅ | ⚠️ | ✅ |
| EXT_SHOW_VIEW | 0x63 | SHOW VIEW | ✅ | ✅ | ✅ | ✅ |
| EXT_SHOW_DOMAIN | 0x64 | SHOW DOMAIN | ❌ | ✅ | ✅ | ✅ |
| EXT_SHOW_VARIABLE | 0x7B | SHOW variable_name | ✅ | ✅ | ✅ | ❌ |
| EXT_SHOW_ALL | 0x7C | SHOW ALL | ✅ | ✅ | ✅ | ❌ |

### SET Commands (0x72-0x74, 0x81)

| Opcode | Value | Command | Firebird | PostgreSQL | V2 | MySQL |
|--------|-------|---------|----|----|----|----|
| EXT_SET_SQL_DIALECT | 0x72 | SET SQL DIALECT | ✅ | ❌ | ✅ | ❌ |
| EXT_SET_NAMES | 0x73 | SET NAMES | ✅ | ✅ | ✅ | ✅ |
| EXT_SET_LOCAL_TIMEOUT | 0x74 | SET LOCAL_TIMEOUT | ✅ | ✅ | ✅ | ❌ |
| EXT_SET_VARIABLE | 0x81 | SET variable = value | ✅ | ✅ | ✅ | ✅ |

### INSERT ... ON CONFLICT (PostgreSQL upsert) (0x82-0x87)

| Opcode | Value | Clause | PostgreSQL | V2 | MySQL | Firebird |
|--------|-------|--------|----|----|----|----|
| EXT_ON_CONFLICT | 0x82 | ON CONFLICT marker | ✅ | ✅ | ❌ | ❌ |
| EXT_ON_CONFLICT_COLUMN | 0x83 | ON CONFLICT (cols) | ✅ | ✅ | ❌ | ❌ |
| EXT_ON_CONFLICT_DO_NOTHING | 0x85 | DO NOTHING | ✅ | ✅ | ❌ | ❌ |
| EXT_ON_CONFLICT_DO_UPDATE | 0x86 | DO UPDATE SET | ✅ | ✅ | ❌ | ❌ |

### Security/Permissions (0x88-0x8A, 0xCA-0xD9)

| Opcode | Value | Statement | PostgreSQL | Firebird | V2 | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_GRANT | 0x88 | GRANT privileges | ✅ | ✅ | ⚠️ | ✅ |
| EXT_REVOKE | 0x89 | REVOKE privileges | ✅ | ✅ | ⚠️ | ✅ |
| EXT_CREATE_USER | 0xCA | CREATE USER | ✅ | ✅ | ⚠️ | ✅ |
| EXT_ALTER_USER | 0xCB | ALTER USER | ✅ | ✅ | ⚠️ | ✅ |
| EXT_DROP_USER | 0xCC | DROP USER | ✅ | ✅ | ⚠️ | ✅ |
| EXT_CREATE_ROLE | 0xCD | CREATE ROLE | ✅ | ✅ | ⚠️ | ✅ |
| EXT_CREATE_POLICY | 0xD7 | CREATE POLICY (RLS) | ✅ | ❌ | ⚠️ | ❌ |
| EXT_ALTER_TABLE_RLS | 0xD9 | ENABLE/DISABLE RLS | ✅ | ❌ | ⚠️ | ❌ |

### PSQL/Procedural (0x90-0xA8)

| Opcode | Value | Statement | Firebird | PostgreSQL | V2 | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_FUNCTION | 0x90 | Function definition | ✅ | ✅ | ❌ | ❌ |
| EXT_PROCEDURE | 0x91 | Procedure definition | ✅ | ✅ | ❌ | ❌ |
| EXT_BLOCK | 0x92 | BEGIN...END block | ✅ | ✅ | ❌ | ❌ |
| EXT_DECLARE | 0x93 | Variable declaration | ✅ | ✅ | ❌ | ❌ |
| EXT_ASSIGN | 0x94 | Variable assignment | ✅ | ✅ | ❌ | ❌ |
| EXT_IF | 0x95 | IF statement | ✅ | ✅ | ❌ | ❌ |
| EXT_LOOP | 0x98 | LOOP statement | ✅ | ✅ | ❌ | ❌ |
| EXT_WHILE | 0x99 | WHILE loop | ✅ | ✅ | ❌ | ❌ |
| EXT_EXIT | 0x9A | EXIT statement | ✅ | ✅ | ❌ | ❌ |
| EXT_RETURN | 0x9B | RETURN statement | ✅ | ✅ | ❌ | ❌ |
| EXT_RAISE | 0x9C | RAISE exception | ✅ | ✅ | ❌ | ❌ |
| EXT_PARAM_IN | 0xA6 | IN parameter | ✅ | ✅ | ❌ | ❌ |
| EXT_PARAM_OUT | 0xA7 | OUT parameter | ✅ | ✅ | ❌ | ❌ |

### Text Search (PostgreSQL) (0xA9-0xB0)

| Opcode | Value | Function | PostgreSQL | V2 | Others |
|--------|-------|----------|----|----|--------|
| EXT_TSMATCH | 0xA9 | @@ (text search match) | ✅ | ❌ | ❌ |
| EXT_TS_RANK | 0xAA | TS_RANK() | ✅ | ❌ | ❌ |
| EXT_TO_TSVECTOR | 0xAD | TO_TSVECTOR() | ✅ | ❌ | ❌ |
| EXT_TO_TSQUERY | 0xAE | TO_TSQUERY() | ✅ | ❌ | ❌ |

### Range Types (PostgreSQL) (0xB1-0xC9)

| Opcode | Value | Type/Function | PostgreSQL | V2 | Others |
|--------|-------|---------------|----|----|--------|
| EXT_TYPE_INT4RANGE | 0xB1 | INT4RANGE type | ✅ | ❌ | ❌ |
| EXT_TYPE_DATERANGE | 0xB4 | DATERANGE type | ✅ | ❌ | ❌ |
| EXT_RANGE_CONSTRUCT | 0xB7 | Range constructor | ✅ | ❌ | ❌ |
| EXT_RANGE_OVERLAPS | 0xB8 | && (range overlap) | ✅ | ❌ | ❌ |
| EXT_RANGE_CONTAINS_RANGE | 0xB9 | @> (contains range) | ✅ | ❌ | ❌ |

### CTE/Set Operations (0x60-0x69)

| Opcode | Value | Feature | PostgreSQL | Firebird | V2 | MySQL |
|--------|-------|---------|----|----|----|----|
| EXT_CTE_DEF | 0x60 | CTE definition | ✅ | ❌ | ❌ | ❌ |
| EXT_WITH_CLAUSE | 0x62 | WITH clause | ✅ | ❌ | ❌ | ❌ |
| EXT_UNION | 0x64 | UNION | ✅ | ✅ | ✅ | ✅ |
| EXT_UNION_ALL | 0x65 | UNION ALL | ✅ | ✅ | ✅ | ✅ |
| EXT_INTERSECT | 0x66 | INTERSECT | ✅ | ✅ | ✅ | ❌ |
| EXT_EXCEPT | 0x68 | EXCEPT | ✅ | ✅ | ✅ | ❌ |

### Window Functions (0x6A-0x6B)

| Opcode | Value | Function | PostgreSQL | Firebird | V2 | MySQL |
|--------|-------|----------|----|----|----|----|
| EXT_WIN_CUME_DIST | 0x6A | CUME_DIST() | ✅ | ✅ | ⚠️ | ✅ |
| EXT_WIN_PERCENT_RANK | 0x6B | PERCENT_RANK() | ✅ | ✅ | ⚠️ | ✅ |

### Triggers (0x6D-0x72)

| Opcode | Value | Statement | Firebird | PostgreSQL | V2 | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_CREATE_DB_TRIGGER | 0x6D | CREATE TRIGGER (DB-level) | ✅ | ✅ | ❌ | ❌ |
| EXT_CREATE_TRIGGER | 0x70 | CREATE TRIGGER | ✅ | ✅ | ❌ | ✅ |
| EXT_DROP_TRIGGER | 0x71 | DROP TRIGGER | ✅ | ✅ | ❌ | ✅ |

### Subqueries (0x73-0x77)

| Opcode | Value | Type | All Parsers |
|--------|-------|------|-------------|
| EXT_SUBQUERY_SCALAR | 0x73 | Scalar subquery | ✅ |
| EXT_SUBQUERY_EXISTS | 0x74 | EXISTS subquery | ✅ |
| EXT_SUBQUERY_IN | 0x75 | IN subquery | ✅ |
| EXT_SUBQUERY_NOT_IN | 0x76 | NOT IN subquery | ✅ |

### Spatial/Geometry Functions (PostGIS-style) (0x50-0x8E)

| Opcode | Value | Function | PostgreSQL | Others |
|--------|-------|----------|------------|--------|
| EXT_ST_POINT | 0x53 | ST_Point(x, y) | ✅ | ❌ |
| EXT_ST_ASTEXT | 0x56 | ST_AsText(geom) | ✅ | ❌ |
| EXT_ST_INTERSECTS | 0x5D | ST_Intersects() | ✅ | ❌ |
| EXT_ST_CONTAINS | 0x5E | ST_Contains() | ✅ | ❌ |
| EXT_ST_DISTANCE | 0x81 | ST_Distance() | ✅ | ❌ |

### Math Functions (0xDA-0xF2)

| Opcode | Value | Function | All Parsers |
|--------|-------|----------|-------------|
| EXT_FUNC_SIN | 0xDA | SIN(x) | ✅ |
| EXT_FUNC_COS | 0xDB | COS(x) | ✅ |
| EXT_FUNC_TAN | 0xDC | TAN(x) | ✅ |
| EXT_FUNC_SQRT | 0xEB | SQRT(x) | ✅ |
| EXT_FUNC_POWER | 0xED | POWER(x, y) | ✅ |
| EXT_FUNC_EXP | 0xEE | EXP(x) | ✅ |
| EXT_FUNC_LN | 0xEF | LN(x) | ✅ |
| EXT_FUNC_LOG | 0xF0 | LOG(x) | ✅ |

### Statistical Functions (0xF3-0xF8)

| Opcode | Value | Function | PostgreSQL | Firebird | V2 | MySQL |
|--------|-------|----------|----|----|----|----|
| EXT_STDDEV_SAMP | 0xF3 | STDDEV_SAMP() | ✅ | ✅ | ⚠️ | ✅ |
| EXT_VAR_SAMP | 0xF5 | VAR_SAMP() | ✅ | ✅ | ⚠️ | ✅ |
| EXT_CORR | 0xF7 | CORR(y, x) | ✅ | ✅ | ❌ | ❌ |

### Cryptographic Functions (0xF9-0xFC)

| Opcode | Value | Function | PostgreSQL | MySQL | V2 | Firebird |
|--------|-------|----------|----|----|----|----|
| EXT_MD5 | 0xF9 | MD5(data) | ✅ | ✅ | ⚠️ | ❌ |
| EXT_SHA256 | 0xFB | SHA256(data) | ✅ | ✅ | ⚠️ | ❌ |

### Index Operations (0x0A-0x2D)

All index operations use MGA visibility tracking (xmin/xmax):

| Opcode | Value | Operation | Notes |
|--------|-------|-----------|-------|
| EXT_INDEX_INSERT | 0x0A | Insert index entry | Sets xmin to current XID |
| EXT_INDEX_SEARCH | 0x0B | Search index by key | Returns visible TIDs |
| EXT_INDEX_SCAN | 0x0C | Range scan index | Start/end keys |
| EXT_INDEX_DELETE | 0x0D | Logical delete | Sets xmax (MGA) |
| EXT_INDEX_UPDATE | 0x1C | Update index entry | Old key → new key |
| EXT_GIN_INSERT | 0x28 | GIN index insert | Multi-value extraction |
| EXT_HNSW_INSERT | 0x2A | Vector index insert | HNSW for ANN search |
| EXT_COLUMNSTORE_SCAN | 0x2D | Columnstore scan | Columnar predicate push |

### Cursor Operations (0x1D-0x1F, 0x2E)

| Opcode | Value | Statement | Firebird | PostgreSQL | V2 | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_CURSOR_DECLARE | 0x1D | DECLARE cursor | ✅ | ✅ | ❌ | ❌ |
| EXT_CURSOR_OPEN | 0x1E | OPEN cursor | ✅ | ✅ | ❌ | ❌ |
| EXT_CURSOR_FETCH | 0x1F | FETCH cursor | ✅ | ✅ | ❌ | ❌ |
| EXT_CURSOR_CLOSE | 0x2E | CLOSE cursor | ✅ | ✅ | ❌ | ❌ |

### Domain Enforcement (Plan 03B) (0x0204-0x020D)

| Opcode | Value | Operation | V2 | PostgreSQL | Firebird | MySQL |
|--------|-------|-----------|----|----|----|----|
| EXT_CHECK_DOMAIN_CONSTRAINT | 0x0204 | Validate domain constraints | ✅ | ⚠️ | ⚠️ | ❌ |
| EXT_APPLY_DOMAIN_MASKING | 0x0205 | Apply data masking | ✅ | ❌ | ❌ | ❌ |
| EXT_ENCRYPT_DOMAIN_VALUE | 0x0206 | Encrypt domain value | ✅ | ❌ | ❌ | ❌ |
| EXT_DECRYPT_DOMAIN_VALUE | 0x0207 | Decrypt domain value | ✅ | ❌ | ❌ | ❌ |

---

## Index Type Support

All index types support MGA visibility (xmin/xmax tracking):

| Index Type | Value | PostgreSQL | Firebird | V2 | MySQL |
|------------|-------|----|----|----|----|
| BTREE | 0x00 | ✅ | ✅ | ✅ | ✅ |
| HASH | 0x01 | ✅ | ✅ | ✅ | ✅ |
| GIN | 0x02 | ✅ | ❌ | ⚠️ | ❌ |
| GIST | 0x03 | ✅ | ❌ | ⚠️ | ❌ |
| SPGIST | 0x04 | ✅ | ❌ | ❌ | ❌ |
| BRIN | 0x05 | ✅ | ❌ | ❌ | ❌ |
| RTREE | 0x06 | ✅ | ❌ | ⚠️ | ❌ |
| HNSW | 0x07 | ✅ | ❌ | ⚠️ | ❌ |
| BITMAP | 0x08 | ❌ | ❌ | ⚠️ | ❌ |
| COLUMNSTORE | 0x09 | ❌ | ❌ | ⚠️ | ❌ |
| LSM | 0x0A | ❌ | ❌ | ⚠️ | ❌ |

---

## Critical Gaps Identified

### V2 Parser Missing Implementations

1. **PSQL/Procedural** - AST nodes exist but parsers not implemented:
   - CREATE FUNCTION/PROCEDURE (EXT_FUNCTION=0x90, EXT_PROCEDURE=0x91)
   - EXECUTE BLOCK (EXT_BLOCK=0x92)
   - IF/WHILE/FOR statements (EXT_IF=0x95, EXT_WHILE=0x99)
   - Exception handling (EXT_RAISE=0x9C)

2. **CTE (Common Table Expressions)** - AST fields exist but never populated:
   - WITH clause (EXT_WITH_CLAUSE=0x62)
   - CTE definition (EXT_CTE_DEF=0x60)
   - CTE scan (EXT_CTE_SCAN=0x61)

3. **Advanced Grouping** - Opcodes exist but not parsed:
   - ROLLUP (EXT_GROUP_ROLLUP=0x45)
   - CUBE (EXT_GROUP_CUBE=0x46)
   - GROUPING SETS (EXT_GROUP_GROUPING_SETS=0x47)

### MySQL Parser Missing Implementations

1. **DDL Statements**:
   - CREATE INDEX (0x1B) - Stub only
   - CREATE VIEW - Stub only
   - DROP TABLE/INDEX/VIEW - Not implemented

2. **Advanced Features**:
   - MERGE statement (EXT_MERGE_* opcodes)
   - CTEs (EXT_CTE_* opcodes)

### Firebird Parser Missing Implementations

1. **PostgreSQL Extensions** (correctly not implemented):
   - Array operators (@>, <@, &&) - PostgreSQL-specific
   - Range types - PostgreSQL-specific
   - Text search (@@ operator) - PostgreSQL-specific

2. **CTEs** (should implement):
   - WITH/WITH RECURSIVE (EXT_CTE_DEF=0x60)

### PostgreSQL Parser - Executor Bytecode Format Mismatches

Many opcodes emitted but format doesn't match executor expectations:
- CREATE TABLE format mismatch (~30% success rate)
- INSERT format mismatch (ON CONFLICT encoding)
- UPDATE/DELETE format mismatch (alias encoding)
- MERGE not supported by executor

---

## Transaction Isolation Mapping (Intentional Semantic Mapping)

**Important:** MySQL and PostgreSQL transaction isolation levels are mapped to Firebird MGA equivalents. This is **INTENTIONAL ARCHITECTURAL DECISION**, not contamination.

### PostgreSQL MVCC → Firebird MGA Mapping

| PostgreSQL Isolation | Firebird MGA Equivalent | Opcode |
|---------------------|------------------------|--------|
| READ UNCOMMITTED | READ COMMITTED | 0x00 (closest match, MGA doesn't support dirty reads) |
| READ COMMITTED | READ COMMITTED | 0x00 |
| REPEATABLE READ | SNAPSHOT | 0x02 |
| SERIALIZABLE | SNAPSHOT TABLE STABILITY | 0x03 |

### MySQL MVCC → Firebird MGA Mapping

| MySQL Isolation | Firebird MGA Equivalent | Opcode |
|----------------|------------------------|--------|
| READ UNCOMMITTED | READ COMMITTED | 0x00 (closest match) |
| READ COMMITTED | READ COMMITTED | 0x00 |
| REPEATABLE READ | SNAPSHOT | 0x02 |
| SERIALIZABLE | SNAPSHOT TABLE STABILITY | 0x03 |

**Rationale:** ScratchBird uses Firebird MGA architecture. MySQL/PostgreSQL parsers map their MVCC isolation levels to the closest available MGA semantics. This provides reasonable behavior even though the underlying concurrency models differ.

---

## Legend

- ✅ **Fully implemented** - Parser emits correct bytecode
- ⚠️ **Partially implemented** - Parser recognizes but may have format issues
- ❌ **Not implemented** - Parser does not support this feature

---

**Next Steps:**
1. Implement V2 parser PSQL support (CREATE FUNCTION/PROCEDURE/TRIGGER)
2. Implement V2 parser CTE support (WITH clause)
3. Fix PostgreSQL parser bytecode format mismatches
4. Complete MySQL parser DDL statements (CREATE INDEX/VIEW, DROP statements)
5. Document all executor-supported opcodes with format specifications

---

**Related Documents:**
- `/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `/docs/audit/parsers/CRITICAL_FINDINGS.md`
- `/docs/audit/parsers/COMPARISON_MATRIX.md` (to be created)
- `/include/scratchbird/sblr/opcodes.h`
