# Parser Comparison Matrix

**Audit Date:** 2026-01-07
**Purpose:** Cross-parser feature comparison to identify dialect bleeding and missing implementations

---

## Legend

- ✅ **Fully Supported** - Feature completely implemented in parser
- ⚠️ **Partial** - Feature recognized but incomplete or has issues
- ❌ **Not Supported** - Feature not implemented
- 🔴 **Dialect Violation** - Feature present that shouldn't be (contamination)
- ✨ **Intentional Extension** - V2 parser extension beyond Firebird base

---

## DDL Statements

### Table Operations

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE TABLE | ✅ | ✅ | ✅ | ✅ | 0x10 |
| CREATE TABLE IF NOT EXISTS | ✅ | ✅ | ✅ | ✅ | 0x10 + flag |
| CREATE TEMPORARY TABLE | ✅ | ✅ | ✅ | ✅ | 0x10 + flag |
| CREATE GLOBAL TEMPORARY TABLE | ❌ | ✅ | ❌ | ❌ | 0x10 + flag |
| DROP TABLE | ✅ | ✅ | ✅ | ❌ | 0x1F |
| DROP TABLE IF EXISTS | ✅ | ✅ | ✅ | ❌ | 0x1F + flag |
| DROP TABLE CASCADE | ✅ | ✅ | ✅ | ❌ | 0x1F + flag |
| ALTER TABLE RENAME | ✅ | ✅ | ✅ | ✅ | EXT_RENAME_OBJECT |
| ALTER TABLE ADD COLUMN | ✅ | ✅ | ✅ | ⚠️ | ALTER_TABLE |
| ALTER TABLE DROP COLUMN | ✅ | ✅ | ✅ | ❌ | ALTER_TABLE |
| ALTER TABLE ADD CONSTRAINT | ✅ | ✅ | ✅ | ❌ | ALTER_TABLE |
| RECREATE TABLE | ❌ | ✅ | ❌ | ❌ | 0x10 + flag |

### Index Operations

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE INDEX | ✅ | ✅ | ✅ | ❌ | 0x1B |
| CREATE UNIQUE INDEX | ✅ | ✅ | ✅ | ❌ | 0x1B + flag |
| CREATE INDEX USING BTREE | ✅ | ✅ | ✅ | ❌ | 0x1B + EXT_INDEX_TYPE |
| CREATE INDEX USING HASH | ✅ | ✅ | ✅ | ❌ | 0x1B + EXT_INDEX_TYPE |
| CREATE INDEX USING GIN | ⚠️ | ❌ | ✅ | ❌ | 0x1B + EXT_INDEX_TYPE |
| CREATE INDEX USING GIST | ⚠️ | ❌ | ✅ | ❌ | 0x1B + EXT_INDEX_TYPE |
| CREATE INDEX USING BRIN | ❌ | ❌ | ✅ | ❌ | 0x1B + EXT_INDEX_TYPE |
| CREATE INDEX USING HNSW | ⚠️ | ❌ | ✅ | ❌ | 0x1B + EXT_INDEX_TYPE |
| DROP INDEX | ✅ | ✅ | ✅ | ❌ | DROP_INDEX |

### View Operations

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE VIEW | ✅ | ✅ | ✅ | ❌ | CREATE_VIEW |
| CREATE OR REPLACE VIEW | ✅ | ✅ | ✅ | ❌ | CREATE_VIEW + flag |
| CREATE MATERIALIZED VIEW | ❌ | ❌ | ✅ | ❌ | CREATE_MAT_VIEW |
| DROP VIEW | ✅ | ✅ | ✅ | ❌ | DROP_VIEW |

### Schema/Database Operations

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE DATABASE | ✅ | ✅ | ✅ | ✅ | EXT_CREATE_DATABASE |
| DROP DATABASE | ✅ | ✅ | ✅ | ✅ | EXT_DROP_DATABASE |
| CREATE SCHEMA | ✅ | ❌ | ✅ | ❌ | EXT_CREATE_SCHEMA |
| DROP SCHEMA | ✅ | ❌ | ✅ | ❌ | EXT_DROP_SCHEMA |
| USE database | ✅ | ✅ | ❌ | ✅ | USE_DATABASE |

### Domain/Type Operations

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE DOMAIN | ✅ | ✅ | ✅ | ❌ | EXT_CREATE_DOMAIN |
| ALTER DOMAIN | ✅ | ✅ | ✅ | ❌ | EXT_ALTER_DOMAIN |
| DROP DOMAIN | ✅ | ✅ | ✅ | ❌ | EXT_DROP_DOMAIN |
| CREATE TYPE (ENUM) | ⚠️ | ❌ | ✅ | ❌ | EXT_CREATE_TYPE |
| CREATE TYPE (COMPOSITE) | ❌ | ❌ | ✅ | ❌ | EXT_CREATE_TYPE |
| CREATE TYPE (RANGE) | ❌ | ❌ | ✅ | ❌ | EXT_CREATE_TYPE |

---

## DML Statements

### SELECT Statement

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| SELECT ... FROM | ✅ | ✅ | ✅ | ✅ | 0x12 |
| SELECT DISTINCT | ✅ | ✅ | ✅ | ✅ | 0x12 + flag |
| SELECT ... WHERE | ✅ | ✅ | ✅ | ✅ | 0x12 + predicate |
| SELECT ... JOIN | ✅ | ✅ | ✅ | ✅ | 0x12 + join |
| SELECT ... LEFT JOIN | ✅ | ✅ | ✅ | ✅ | 0x12 + join |
| SELECT ... RIGHT JOIN | ✅ | ✅ | ✅ | ✅ | 0x12 + join |
| SELECT ... FULL JOIN | ✅ | ✅ | ✅ | ❌ | 0x12 + join |
| SELECT ... CROSS JOIN | ✅ | ✅ | ✅ | ✅ | 0x12 + join |
| SELECT ... LATERAL JOIN | ❌ | ❌ | ✅ | ❌ | 0x12 + flag |
| SELECT ... GROUP BY | ✅ | ✅ | ✅ | ✅ | 0x12 + grouping |
| SELECT ... HAVING | ✅ | ✅ | ✅ | ✅ | 0x12 + having |
| SELECT ... ORDER BY | ✅ | ✅ | ✅ | ✅ | 0x12 + ordering |
| SELECT ... LIMIT | ✅ | ❌ | ✅ | ✅ | 0x12 + limit |
| SELECT ... OFFSET | ✅ | ❌ | ✅ | ✅ | 0x12 + offset |
| SELECT FIRST n SKIP m | ❌ | ✅ | ❌ | ❌ | 0x12 + first/skip |
| SELECT ... FOR UPDATE | ✅ | ✅ | ✅ | ✅ | 0x12 + lock |

### INSERT Statement

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| INSERT INTO | ✅ | ✅ | ✅ | ✅ | 0x11 |
| INSERT ... VALUES (single row) | ✅ | ✅ | ✅ | ✅ | 0x11 |
| INSERT ... VALUES (multi-row) | ✅ | ✅ | ✅ | ✅ | 0x11 |
| INSERT ... SELECT | ✅ | ✅ | ✅ | ✅ | 0x11 + subquery |
| INSERT ... RETURNING | ⚠️ | ✅ | ✅ | ❌ | 0x11 + EXT_RETURNING |
| INSERT ... ON CONFLICT (PostgreSQL) | ✅ 🔴 | ❌ | ✅ | ❌ | EXT_ON_CONFLICT |
| INSERT ... ON DUPLICATE KEY UPDATE (MySQL) | ❌ | ❌ | ❌ | ✅ | Custom MySQL |
| INSERT IGNORE (MySQL) | ❌ | ❌ | ❌ | ✅ | Custom MySQL |
| REPLACE INTO (MySQL) | ❌ | ❌ | ❌ | ✅ | Custom MySQL |

**Note:** V2 parser has `INSERT ... ON CONFLICT` which is PostgreSQL-specific. This may be intentional extension or contamination - clarification needed.

### UPDATE Statement

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| UPDATE ... SET | ✅ | ✅ | ✅ | ✅ | 0xC3 |
| UPDATE ... WHERE | ✅ | ✅ | ✅ | ✅ | 0xC3 + predicate |
| UPDATE ... RETURNING | ⚠️ | ✅ | ✅ | ❌ | 0xC3 + EXT_RETURNING |
| UPDATE ... FROM (PostgreSQL) | ✅ 🔴 | ❌ | ✅ | ❌ | 0xC3 + from_clause |
| UPDATE multi-table (MySQL) | ❌ | ❌ | ❌ | ✅ | Custom MySQL |
| UPDATE ... ORDER BY LIMIT (MySQL) | ❌ | ❌ | ❌ | ✅ | Custom MySQL |

**Note:** V2 parser has `UPDATE ... FROM` which is PostgreSQL-specific extension.

### DELETE Statement

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| DELETE FROM ... WHERE | ✅ | ✅ | ✅ | ✅ | 0xC4 |
| DELETE ... RETURNING | ⚠️ | ✅ | ✅ | ❌ | 0xC4 + EXT_RETURNING |
| DELETE ... USING (PostgreSQL) | ✅ 🔴 | ❌ | ✅ | ❌ | 0xC4 + using_clause |
| DELETE ... ORDER BY LIMIT (MySQL) | ❌ | ❌ | ❌ | ✅ | Custom MySQL |

**Note:** V2 parser has `DELETE ... USING` which is PostgreSQL-specific extension.

### MERGE Statement

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| MERGE INTO | ❌ | ✅ | ✅ | ❌ | EXT_MERGE_START |
| MERGE ... USING | ❌ | ✅ | ✅ | ❌ | EXT_MERGE_SOURCE |
| MERGE ... ON | ❌ | ✅ | ✅ | ❌ | EXT_MERGE_ON |
| WHEN MATCHED UPDATE | ❌ | ✅ | ✅ | ❌ | EXT_MERGE_WHEN_MATCHED |
| WHEN NOT MATCHED INSERT | ❌ | ✅ | ✅ | ❌ | EXT_MERGE_WHEN_NOT_MATCHED |
| UPDATE OR INSERT (Firebird) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |

---

## Transaction Control

### Transaction Statements

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| BEGIN / START TRANSACTION | ✅ | ✅ | ✅ | ✅ | 0x13 |
| COMMIT | ✅ | ✅ | ✅ | ✅ | 0x14 |
| ROLLBACK | ✅ | ✅ | ✅ | ✅ | 0x15 |
| SAVEPOINT | ✅ | ✅ | ✅ | ✅ | EXT_SAVEPOINT |
| RELEASE SAVEPOINT | ✅ | ✅ | ✅ | ✅ | EXT_RELEASE_SAVEPOINT |
| ROLLBACK TO SAVEPOINT | ✅ | ✅ | ✅ | ✅ | EXT_ROLLBACK_TO_SAVEPOINT |
| COMMIT RETAINING (Firebird) | ✅ | ✅ | ❌ | ❌ | EXT_COMMIT_RETAINING |
| ROLLBACK RETAINING (Firebird) | ✅ | ✅ | ❌ | ❌ | EXT_ROLLBACK_RETAINING |
| SET AUTOCOMMIT (MySQL) | ✅ | ❌ | ❌ | ✅ | EXT_SET_AUTOCOMMIT |

### Isolation Levels (Mapped to Firebird MGA)

**Note:** MySQL and PostgreSQL isolation levels are **INTENTIONALLY MAPPED** to Firebird MGA equivalents. This is semantic mapping, not contamination.

| Feature | V2 (MGA) | Firebird (MGA) | PostgreSQL (MVCC→MGA) | MySQL (MVCC→MGA) | SBLR Value |
|---------|----------|----------------|----------------------|-----------------|------------|
| READ UNCOMMITTED | ❌ | ❌ | ✅→READ COMMITTED | ✅→READ COMMITTED | 0x00 |
| READ COMMITTED | ✅ | ✅ | ✅ | ✅ | 0x00 |
| REPEATABLE READ | N/A | N/A | ✅→SNAPSHOT | ✅→SNAPSHOT | 0x02 |
| SNAPSHOT | ✅ | ✅ | ⚠️ Mapped from RR | ⚠️ Mapped from RR | 0x02 |
| SERIALIZABLE | N/A | N/A | ✅→SNAP TABLE STAB | ✅→SNAP TABLE STAB | 0x03 |
| SNAPSHOT TABLE STABILITY | ✅ | ✅ | ⚠️ Mapped from SER | ⚠️ Mapped from SER | 0x03 |
| READ CONSISTENCY (Firebird) | ✅ | ✅ | ❌ | ❌ | Firebird flag |
| RECORD VERSION (Firebird) | ✅ | ✅ | ❌ | ❌ | Firebird flag |
| NO RECORD VERSION (Firebird) | ✅ | ✅ | ❌ | ❌ | Firebird flag |
| WAIT / NO WAIT (Firebird) | ✅ | ✅ | ❌ | ❌ | Firebird flag |
| LOCK TIMEOUT (Firebird) | ✅ | ✅ | ❌ | ❌ | Firebird param |
| RESERVING (Firebird) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |

---

## PSQL / Procedural SQL

### Function/Procedure DDL

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE FUNCTION | ❌ | ✅ | ✅ | ❌ | EXT_CREATE_FUNCTION_STMT |
| CREATE OR REPLACE FUNCTION | ❌ | ✅ | ✅ | ❌ | EXT_CREATE_FUNCTION_STMT + flag |
| CREATE PROCEDURE | ❌ | ✅ | ✅ | ❌ | EXT_CREATE_PROCEDURE_STMT |
| CREATE OR REPLACE PROCEDURE | ❌ | ✅ | ✅ | ❌ | EXT_CREATE_PROCEDURE_STMT + flag |
| DROP FUNCTION | ❌ | ✅ | ✅ | ❌ | EXT_DROP_FUNCTION_STMT |
| DROP PROCEDURE | ❌ | ✅ | ✅ | ❌ | EXT_DROP_PROCEDURE_STMT |
| EXECUTE BLOCK (Firebird) | ❌ | ✅ | ❌ | ❌ | EXT_BLOCK |
| CALL procedure() | ❌ | ✅ | ✅ | ✅ | EXT_CALL |

**CRITICAL GAP:** V2 parser has AST nodes for PSQL but never parses them (lines 299-302 in parser_v2.cpp are TODO comments).

### Control Flow

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| DECLARE variable | ❌ | ✅ | ✅ | ❌ | EXT_DECLARE |
| Variable assignment (:=) | ❌ | ✅ | ✅ | ❌ | EXT_ASSIGN |
| IF...THEN...ELSE | ❌ | ✅ | ✅ | ❌ | EXT_IF |
| WHILE loop | ❌ | ✅ | ✅ | ❌ | EXT_WHILE |
| FOR loop | ❌ | ✅ | ✅ | ❌ | FOR |
| FOR SELECT loop (Firebird) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |
| LOOP...END LOOP | ❌ | ✅ | ✅ | ❌ | EXT_LOOP |
| EXIT / BREAK | ❌ | ✅ | ✅ | ❌ | EXT_EXIT |
| RETURN | ❌ | ✅ | ✅ | ❌ | EXT_RETURN |
| RAISE exception | ❌ | ✅ | ✅ | ❌ | EXT_RAISE |
| TRY...CATCH / WHEN...DO | ❌ | ✅ | ✅ | ❌ | EXT_EXCEPTION_HANDLER |

### Cursors

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| DECLARE cursor | ❌ | ✅ | ✅ | ❌ | EXT_CURSOR_DECLARE |
| OPEN cursor | ❌ | ✅ | ✅ | ❌ | EXT_CURSOR_OPEN |
| FETCH cursor | ❌ | ✅ | ✅ | ❌ | EXT_CURSOR_FETCH |
| CLOSE cursor | ❌ | ✅ | ✅ | ❌ | EXT_CURSOR_CLOSE |

---

## Advanced SQL Features

### CTEs (Common Table Expressions)

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| WITH clause | ❌ | ❌ | ✅ | ❌ | EXT_WITH_CLAUSE |
| WITH RECURSIVE | ❌ | ❌ | ✅ | ❌ | EXT_WITH_CLAUSE + flag |
| Multiple CTEs | ❌ | ❌ | ✅ | ❌ | EXT_CTE_DEF (multiple) |

**CRITICAL GAP:** V2 parser has `WithClause` fields in AST nodes but never populates them.

### Set Operations

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| UNION | ✅ | ✅ | ✅ | ✅ | EXT_UNION |
| UNION ALL | ✅ | ✅ | ✅ | ✅ | EXT_UNION_ALL |
| INTERSECT | ✅ | ✅ | ✅ | ❌ | EXT_INTERSECT |
| INTERSECT ALL | ✅ | ✅ | ✅ | ❌ | EXT_INTERSECT_ALL |
| EXCEPT | ✅ | ✅ | ✅ | ❌ | EXT_EXCEPT |
| EXCEPT ALL | ✅ | ✅ | ✅ | ❌ | EXT_EXCEPT_ALL |
| MINUS (Oracle-style) | ❌ | ❌ | ❌ | ❌ | - |

### Subqueries

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| Scalar subquery | ✅ | ✅ | ✅ | ✅ | EXT_SUBQUERY_SCALAR |
| EXISTS subquery | ✅ | ✅ | ✅ | ✅ | EXT_SUBQUERY_EXISTS |
| IN subquery | ✅ | ✅ | ✅ | ✅ | EXT_SUBQUERY_IN |
| NOT IN subquery | ✅ | ✅ | ✅ | ✅ | EXT_SUBQUERY_NOT_IN |
| ARRAY(SELECT...) (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | EXT_SUBQUERY_ARRAY |

### Window Functions

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| ROW_NUMBER() | ⚠️ | ✅ | ✅ | ✅ | WIN_ROW_NUMBER |
| RANK() | ⚠️ | ✅ | ✅ | ✅ | WIN_RANK |
| DENSE_RANK() | ⚠️ | ✅ | ✅ | ✅ | WIN_DENSE_RANK |
| LAG() / LEAD() | ⚠️ | ✅ | ✅ | ✅ | WIN_LAG / WIN_LEAD |
| FIRST_VALUE() / LAST_VALUE() | ⚠️ | ✅ | ✅ | ✅ | WIN_FIRST_VALUE |
| NTH_VALUE() | ⚠️ | ✅ | ✅ | ✅ | WIN_NTH_VALUE |
| NTILE() | ⚠️ | ✅ | ✅ | ✅ | WIN_NTILE |
| CUME_DIST() | ⚠️ | ✅ | ✅ | ✅ | EXT_WIN_CUME_DIST |
| PERCENT_RANK() | ⚠️ | ✅ | ✅ | ✅ | EXT_WIN_PERCENT_RANK |
| OVER (PARTITION BY) | ⚠️ | ✅ | ✅ | ✅ | Window clause |
| OVER (ORDER BY) | ⚠️ | ✅ | ✅ | ✅ | Window clause |
| ROWS BETWEEN | ⚠️ | ✅ | ✅ | ✅ | Window frame |
| RANGE BETWEEN | ⚠️ | ✅ | ✅ | ✅ | Window frame |

### Grouping Extensions

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| GROUP BY ROLLUP | ❌ | ❌ | ✅ | ✅ | EXT_GROUP_ROLLUP |
| GROUP BY CUBE | ❌ | ❌ | ✅ | ❌ | EXT_GROUP_CUBE |
| GROUP BY GROUPING SETS | ❌ | ❌ | ✅ | ❌ | EXT_GROUP_GROUPING_SETS |
| GROUPING() function | ❌ | ❌ | ✅ | ✅ | EXT_GROUPING_FUNC |

---

## Data Types

### Core Types

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| INTEGER / INT | ✅ | ✅ | ✅ | ✅ | TYPE_INTEGER |
| SMALLINT | ✅ | ✅ | ✅ | ✅ | TYPE_SMALLINT |
| BIGINT | ✅ | ✅ | ✅ | ✅ | TYPE_BIGINT |
| INT128 | ⚠️ | ✅ | ⚠️ | ⚠️ | EXT_TYPE_INT128 |
| UINT128 | ⚠️ | ✅ | ❌ | ⚠️ | EXT_TYPE_UINT128 |
| DECIMAL / NUMERIC | ✅ | ✅ | ✅ | ✅ | TYPE_NUMERIC |
| FLOAT | ✅ | ✅ | ✅ | ✅ | TYPE_FLOAT |
| DOUBLE PRECISION | ✅ | ✅ | ✅ | ✅ | TYPE_DOUBLE |
| REAL | ✅ | ✅ | ✅ | ❌ | TYPE_REAL |
| DECFLOAT (Firebird 4+) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |

### String Types

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| CHAR / CHARACTER | ✅ | ✅ | ✅ | ✅ | TYPE_CHAR |
| VARCHAR | ✅ | ✅ | ✅ | ✅ | TYPE_VARCHAR |
| TEXT | ✅ | ✅ | ✅ | ✅ | TYPE_TEXT |
| TINYTEXT (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |
| MEDIUMTEXT (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |
| LONGTEXT (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |

### Binary Types

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| BLOB | ✅ | ✅ | ❌ | ✅ | TYPE_BLOB |
| BYTEA | ❌ | ❌ | ✅ | ❌ | TYPE_BYTEA |
| BINARY | ✅ | ❌ | ❌ | ✅ | TYPE_BINARY |
| VARBINARY | ✅ | ✅ | ❌ | ✅ | TYPE_VARBINARY |

### Date/Time Types

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| DATE | ✅ | ✅ | ✅ | ✅ | TYPE_DATE |
| TIME | ✅ | ✅ | ✅ | ✅ | TYPE_TIME |
| TIMESTAMP | ✅ | ✅ | ✅ | ✅ | TYPE_TIMESTAMP |
| TIMESTAMP WITH TIME ZONE | ⚠️ | ✅ | ✅ | ❌ | TYPE_TIMESTAMPTZ |
| TIME WITH TIME ZONE | ⚠️ | ✅ | ✅ | ❌ | TYPE_TIMETZ |
| DATETIME (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |
| YEAR (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |
| INTERVAL | ⚠️ | ❌ | ✅ | ❌ | TYPE_INTERVAL |

### Boolean Type

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| BOOLEAN / BOOL | ✅ | ✅ | ✅ | ✅ | TYPE_BOOLEAN |

### Complex Types

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| JSON | ⚠️ | ❌ | ✅ | ✅ | TYPE_JSON |
| JSONB | ⚠️ | ❌ | ✅ | ❌ | TYPE_JSONB |
| ARRAY | ⚠️ | ✅ | ✅ | ❌ | TYPE_ARRAY |
| UUID | ✅ | ❌ | ✅ | ❌ | TYPE_UUID |
| XML | ❌ | ❌ | ✅ | ❌ | TYPE_XML |
| ENUM (MySQL-style) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |
| SET (MySQL-style) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |

### PostgreSQL-Specific Types

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| INET | ❌ | ❌ | ✅ | ❌ | TYPE_INET |
| CIDR | ❌ | ❌ | ✅ | ❌ | TYPE_CIDR |
| MACADDR | ❌ | ❌ | ✅ | ❌ | TYPE_MACADDR |
| MACADDR8 | ❌ | ❌ | ✅ | ❌ | TYPE_MACADDR8 |
| TSVECTOR | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_TSVECTOR |
| TSQUERY | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_TSQUERY |
| INT4RANGE | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_INT4RANGE |
| INT8RANGE | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_INT8RANGE |
| NUMRANGE | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_NUMRANGE |
| DATERANGE | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_DATERANGE |
| TSRANGE | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_TSRANGE |
| TSTZRANGE | ❌ | ❌ | ✅ | ❌ | EXT_TYPE_TSTZRANGE |

### Spatial Types (PostGIS-style)

| Type | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|------|----|----|----|----|-------------|
| POINT | ⚠️ | ❌ | ✅ | ✅ | EXT_TYPE_POINT |
| LINESTRING | ⚠️ | ❌ | ✅ | ✅ | EXT_TYPE_LINESTRING |
| POLYGON | ⚠️ | ❌ | ✅ | ✅ | EXT_TYPE_POLYGON |
| GEOMETRY | ❌ | ❌ | ✅ | ✅ | TYPE_GEOMETRY |

---

## Functions

### Aggregate Functions

| Function | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| COUNT(*) | ✅ | ✅ | ✅ | ✅ | AGG_COUNT |
| COUNT(expr) | ✅ | ✅ | ✅ | ✅ | AGG_COUNT |
| SUM(expr) | ✅ | ✅ | ✅ | ✅ | AGG_SUM |
| AVG(expr) | ✅ | ✅ | ✅ | ✅ | AGG_AVG |
| MIN(expr) | ✅ | ✅ | ✅ | ✅ | AGG_MIN |
| MAX(expr) | ✅ | ✅ | ✅ | ✅ | AGG_MAX |
| STDDEV_SAMP(expr) | ⚠️ | ✅ | ✅ | ✅ | EXT_STDDEV_SAMP |
| STDDEV_POP(expr) | ⚠️ | ✅ | ✅ | ✅ | EXT_STDDEV_POP |
| VAR_SAMP(expr) | ⚠️ | ✅ | ✅ | ✅ | EXT_VAR_SAMP |
| VAR_POP(expr) | ⚠️ | ✅ | ✅ | ✅ | EXT_VAR_POP |
| CORR(y, x) | ❌ | ✅ | ✅ | ❌ | EXT_CORR |
| COVAR_POP(y, x) | ❌ | ✅ | ✅ | ❌ | EXT_COVAR_POP |
| STRING_AGG (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | PostgreSQL-specific |
| ARRAY_AGG (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | PostgreSQL-specific |
| JSON_AGG (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | PostgreSQL-specific |
| XMLAGG (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | EXT_XMLAGG |
| GROUP_CONCAT (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |

### String Functions

| Function | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| LENGTH(str) | ✅ | ✅ | ✅ | ✅ | FUNC_LENGTH |
| SUBSTRING(str, pos, len) | ✅ | ✅ | ✅ | ✅ | FUNC_SUBSTRING |
| UPPER(str) | ✅ | ✅ | ✅ | ✅ | FUNC_UPPER |
| LOWER(str) | ✅ | ✅ | ✅ | ✅ | FUNC_LOWER |
| TRIM(str) | ✅ | ✅ | ✅ | ✅ | FUNC_TRIM |
| LTRIM(str) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_LTRIM |
| RTRIM(str) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_RTRIM |
| CONCAT(str1, ...) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_CONCAT |
| CONCAT_WS(sep, str1, ...) | ⚠️ | ❌ | ✅ | ✅ | EXT_FUNC_CONCAT_WS |
| REPLACE(str, from, to) | ✅ | ✅ | ✅ | ✅ | FUNC_REPLACE |
| REVERSE(str) | ✅ | ❌ | ✅ | ✅ | EXT_REVERSE |
| REPEAT(str, count) | ✅ | ❌ | ✅ | ✅ | EXT_REPEAT |
| INITCAP(str) | ⚠️ | ❌ | ✅ | ❌ | EXT_INITCAP |
| LPAD(str, len, fill) | ⚠️ | ❌ | ✅ | ✅ | EXT_LPAD |
| RPAD(str, len, fill) | ⚠️ | ❌ | ✅ | ✅ | EXT_RPAD |
| SPLIT_PART(str, delim, n) | ⚠️ | ❌ | ✅ | ❌ | EXT_SPLIT_PART |
| STRPOS(str, substr) | ⚠️ | ❌ | ✅ | ❌ | EXT_STRPOS |
| POSITION(substr IN str) | ⚠️ | ✅ | ✅ | ✅ | EXT_POSITION |

### Math Functions

| Function | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| ABS(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_ABS |
| CEIL(x) / CEILING(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_CEIL |
| FLOOR(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_FLOOR |
| ROUND(x, precision) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_ROUND |
| TRUNC(x, precision) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_TRUNC |
| MOD(x, y) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_MOD |
| POWER(x, y) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_POWER |
| SQRT(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_SQRT |
| CBRT(x) | ⚠️ | ❌ | ✅ | ❌ | EXT_FUNC_CBRT |
| EXP(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_EXP |
| LN(x) | ✅ | ✅ | ✅ | ❌ | EXT_FUNC_LN |
| LOG(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_LOG |
| LOG10(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_LOG10 |
| SIN(x), COS(x), TAN(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_SIN/COS/TAN |
| ASIN(x), ACOS(x), ATAN(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_ASIN/ACOS/ATAN |
| PI() | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_PI |
| SIGN(x) | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_SIGN |

### Date/Time Functions

| Function | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| CURRENT_DATE | ✅ | ✅ | ✅ | ✅ | Context variable |
| CURRENT_TIME | ✅ | ✅ | ✅ | ✅ | EXT_FUNC_CURRENT_TIME |
| CURRENT_TIMESTAMP | ✅ | ✅ | ✅ | ✅ | Context variable |
| NOW() | ✅ | ❌ | ✅ | ✅ | MySQL/PostgreSQL |
| EXTRACT(field FROM value) | ✅ | ✅ | ✅ | ✅ | EXT_EXTRACT |
| DATEADD (Firebird) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |
| DATEDIFF (Firebird/MySQL) | ⚠️ | ✅ | ❌ | ✅ | Firebird/MySQL |
| DATE_TRUNC (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | PostgreSQL-specific |
| AGE(ts1, ts2) (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | EXT_FUNC_AGE |

### Type Conversion

| Function | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| CAST(expr AS type) | ✅ | ✅ | ✅ | ✅ | CAST |
| CONVERT(expr, type) (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |
| expr::type (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | PostgreSQL cast |

### Conditional Functions

| Function | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| COALESCE(val1, ...) | ✅ | ✅ | ✅ | ✅ | FUNC_COALESCE |
| NULLIF(val1, val2) | ✅ | ✅ | ✅ | ✅ | FUNC_NULLIF |
| IIF(cond, true_val, false_val) | ⚠️ | ✅ | ❌ | ❌ | Firebird-specific |
| CASE WHEN ... THEN ... END | ✅ | ✅ | ✅ | ✅ | CASE expression |
| IF(cond, true, false) (MySQL) | ❌ | ❌ | ❌ | ✅ | MySQL-specific |

---

## Operators

### Arithmetic Operators

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| + | ✅ | ✅ | ✅ | ✅ | EXPR_ADD |
| - | ✅ | ✅ | ✅ | ✅ | EXPR_SUB |
| * | ✅ | ✅ | ✅ | ✅ | EXPR_MUL |
| / | ✅ | ✅ | ✅ | ✅ | EXPR_DIV |
| % | ✅ | ✅ | ✅ | ✅ | EXPR_MOD |

### Comparison Operators

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| = | ✅ | ✅ | ✅ | ✅ | EXPR_EQ |
| <> / != | ✅ | ✅ | ✅ | ✅ | EXPR_NE |
| < | ✅ | ✅ | ✅ | ✅ | EXPR_LT |
| <= | ✅ | ✅ | ✅ | ✅ | EXPR_LE |
| > | ✅ | ✅ | ✅ | ✅ | EXPR_GT |
| >= | ✅ | ✅ | ✅ | ✅ | EXPR_GE |
| IS NULL | ✅ | ✅ | ✅ | ✅ | EXT_EXPR_IS_NULL |
| IS NOT NULL | ✅ | ✅ | ✅ | ✅ | NOT + IS_NULL |
| IS DISTINCT FROM (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | PostgreSQL-specific |
| IS NOT DISTINCT FROM (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | EXT_NULL_SAFE_EQ |
| <=> (MySQL NULL-safe equal) | ❌ | ❌ | ❌ | ✅ | EXT_NULL_SAFE_EQ |

### Logical Operators

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| AND | ✅ | ✅ | ✅ | ✅ | EXPR_AND |
| OR | ✅ | ✅ | ✅ | ✅ | EXPR_OR |
| NOT | ✅ | ✅ | ✅ | ✅ | EXPR_NOT / EXT_EXPR_NOT |

### Pattern Matching

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| LIKE | ✅ | ✅ | ✅ | ✅ | LIKE |
| ILIKE (PostgreSQL) | ❌ | ❌ | ✅ | ❌ | ILIKE |
| SIMILAR TO (Firebird/PostgreSQL) | ⚠️ | ✅ | ✅ | ❌ | SIMILAR_TO |
| CONTAINING (Firebird) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |
| STARTING WITH (Firebird) | ❌ | ✅ | ❌ | ❌ | Firebird-specific |
| ~ (PostgreSQL regex) | ❌ | ❌ | ✅ | ❌ | EXT_REGEX_MATCH |
| ~* (PostgreSQL regex case-insens.) | ❌ | ❌ | ✅ | ❌ | EXT_REGEX_MATCH_CI |

### Array Operators (PostgreSQL)

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| && (array overlap) | ❌ | ❌ | ✅ | ❌ | EXT_ARRAY_OVERLAP |
| @> (array contains) | ❌ | ❌ | ✅ | ❌ | EXT_ARRAY_CONTAINS |
| <@ (array contained by) | ❌ | ❌ | ✅ | ❌ | EXT_ARRAY_CONTAINED_BY |
| array[index] subscript | ⚠️ | ✅ | ✅ | ❌ | EXT_ARRAY_SUBSCRIPT |

### JSON Operators (PostgreSQL/MySQL)

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| -> (JSON field access) | ❌ | ❌ | ✅ | ✅ | JSON_FIELD |
| ->> (JSON field as text) | ❌ | ❌ | ✅ | ✅ | JSON_FIELD_TEXT |
| @> (JSON contains) | ❌ | ❌ | ✅ | ❌ | JSON_CONTAINS |
| <@ (JSON contained by) | ❌ | ❌ | ✅ | ❌ | JSON_CONTAINED_BY |

### Bitwise Operators

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| & (bitwise AND) | ⚠️ | ✅ (BIN_AND) | ✅ | ✅ | EXT_BIT_AND |
| \| (bitwise OR) | ⚠️ | ✅ (BIN_OR) | ✅ | ✅ | EXT_BIT_OR |
| ^ (bitwise XOR) | ⚠️ | ✅ (BIN_XOR) | ✅ | ✅ | EXT_BIT_XOR |
| ~ (bitwise NOT) | ⚠️ | ✅ (BIN_NOT) | ✅ | ✅ | EXT_BIT_NOT |
| << (left shift) | ⚠️ | ✅ (BIN_SHL) | ✅ | ✅ | EXT_BIT_SHIFT_LEFT |
| >> (right shift) | ⚠️ | ✅ (BIN_SHR) | ✅ | ✅ | EXT_BIT_SHIFT_RIGHT |

### Range Operators (PostgreSQL)

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| && (range overlap) | ❌ | ❌ | ✅ | ❌ | EXT_RANGE_OVERLAPS |
| @> (range contains) | ❌ | ❌ | ✅ | ❌ | EXT_RANGE_CONTAINS_RANGE |
| <@ (range contained) | ❌ | ❌ | ✅ | ❌ | EXT_RANGE_CONTAINED_BY |
| << (strictly left) | ❌ | ❌ | ✅ | ❌ | EXT_RANGE_STRICTLY_LEFT |
| >> (strictly right) | ❌ | ❌ | ✅ | ❌ | EXT_RANGE_STRICTLY_RIGHT |
| -|- (adjacent) | ❌ | ❌ | ✅ | ❌ | EXT_RANGE_ADJACENT |

### Text Search Operators (PostgreSQL)

| Operator | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|----------|----|----|----|----|-------------|
| @@ (text search match) | ❌ | ❌ | ✅ | ❌ | EXT_TSMATCH |

---

## Security / Permissions

### User Management

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE USER | ⚠️ | ✅ | ✅ | ✅ | EXT_CREATE_USER |
| ALTER USER | ⚠️ | ✅ | ✅ | ✅ | EXT_ALTER_USER |
| DROP USER | ⚠️ | ✅ | ✅ | ✅ | EXT_DROP_USER |
| CREATE ROLE | ⚠️ | ✅ | ✅ | ✅ | EXT_CREATE_ROLE |
| DROP ROLE | ⚠️ | ✅ | ✅ | ✅ | EXT_DROP_ROLE |
| SET ROLE | ⚠️ | ✅ | ✅ | ❌ | EXT_SET_ROLE |

### Privilege Management

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| GRANT privileges ON object | ⚠️ | ✅ | ✅ | ✅ | EXT_GRANT |
| REVOKE privileges ON object | ⚠️ | ✅ | ✅ | ✅ | EXT_REVOKE |
| GRANT role TO user | ⚠️ | ✅ | ✅ | ✅ | EXT_GRANT_ROLE |
| REVOKE role FROM user | ⚠️ | ✅ | ✅ | ✅ | EXT_REVOKE_ROLE |
| WITH GRANT OPTION | ⚠️ | ✅ | ✅ | ✅ | EXT_GRANT_OPTION |

### Row-Level Security (PostgreSQL)

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE POLICY | ⚠️ | ❌ | ✅ | ❌ | EXT_CREATE_POLICY |
| DROP POLICY | ⚠️ | ❌ | ✅ | ❌ | EXT_DROP_POLICY |
| ALTER TABLE ... ENABLE ROW LEVEL SECURITY | ⚠️ | ❌ | ✅ | ❌ | EXT_ALTER_TABLE_RLS |

---

## Admin / SHOW Commands

### MySQL-Style SHOW

| Command | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| SHOW TABLES | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_TABLES |
| SHOW DATABASES | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_DATABASES |
| SHOW COLUMNS FROM table | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_COLUMNS |
| SHOW INDEXES FROM table | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_INDEXES |
| SHOW CREATE TABLE | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_CREATE_TABLE |
| DESCRIBE table | ✅ | ✅ | ✅ | ✅ | EXT_DESCRIBE_TABLE |

### Firebird-Style SHOW

| Command | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| SHOW TABLE object | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_TABLE |
| SHOW INDEX object | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_INDEX |
| SHOW TRIGGER object | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_TRIGGER |
| SHOW PROCEDURE object | ⚠️ | ✅ | ✅ | ✅ | EXT_SHOW_PROCEDURE |
| SHOW FUNCTION object | ⚠️ | ✅ | ✅ | ✅ | EXT_SHOW_FUNCTION |
| SHOW VIEW object | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_VIEW |
| SHOW DOMAIN object | ✅ | ✅ | ✅ | ❌ | EXT_SHOW_DOMAIN |
| SHOW GENERATOR object | ✅ | ✅ | ⚠️ | ❌ | EXT_SHOW_GENERATOR |
| SHOW SCHEMA | ✅ | ✅ | ✅ | ❌ | EXT_SHOW_SCHEMA |
| SHOW SQL DIALECT | ✅ | ✅ | ❌ | ❌ | EXT_SHOW_SQL_DIALECT |
| SHOW VERSION | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_VERSION |
| SHOW DATABASE | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_DATABASE |

### Session Variables

| Command | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| SHOW variable_name | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_VARIABLE |
| SHOW ALL | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_ALL |
| SHOW TRANSACTION ISOLATION LEVEL | ✅ | ✅ | ✅ | ✅ | EXT_SHOW_TRANSACTION_LEVEL |
| SET variable = value | ✅ | ✅ | ✅ | ✅ | EXT_SET_VARIABLE |
| SET SQL DIALECT n | ✅ | ✅ | ❌ | ❌ | EXT_SET_SQL_DIALECT |
| SET NAMES charset | ✅ | ✅ | ✅ | ✅ | EXT_SET_NAMES |

---

## Catalog Tables

### Firebird System Tables

| Table Prefix | V2 | Firebird | PostgreSQL | MySQL | Description |
|--------------|----|----|----|----|-------------|
| RDB$* | ✅ | ✅ | ❌ | ❌ | Metadata tables |
| MON$* | ✅ | ✅ | ❌ | ❌ | Monitoring tables |
| SEC$* | ✅ | ✅ | ❌ | ❌ | Security tables |

**Firebird Catalog:**
- RDB$DATABASE, RDB$RELATIONS, RDB$FIELDS, RDB$RELATION_FIELDS
- RDB$INDICES, RDB$INDEX_SEGMENTS, RDB$GENERATORS
- RDB$PROCEDURES, RDB$FUNCTIONS, RDB$TRIGGERS
- MON$DATABASE, MON$ATTACHMENTS, MON$TRANSACTIONS
- SEC$USERS, SEC$USER_ATTRIBUTES

### PostgreSQL System Catalogs

| Schema | V2 | Firebird | PostgreSQL | MySQL | Description |
|--------|----|----|----|----|-------------|
| pg_catalog | ❌ | ❌ | ✅ | ❌ | System catalog |
| information_schema | ⚠️ | ❌ | ✅ | ✅ | ANSI standard |

**PostgreSQL Catalog:**
- pg_class, pg_attribute, pg_index, pg_constraint
- pg_type, pg_proc, pg_namespace, pg_database
- pg_tables, pg_views, pg_indexes

### MySQL System Schemas

| Schema | V2 | Firebird | PostgreSQL | MySQL | Description |
|--------|----|----|----|----|-------------|
| information_schema | ⚠️ | ❌ | ✅ | ✅ | ANSI standard |
| mysql | ❌ | ❌ | ❌ | ✅ | MySQL system schema |

**MySQL Catalog:**
- information_schema.TABLES, information_schema.COLUMNS
- information_schema.STATISTICS, information_schema.KEY_COLUMN_USAGE
- mysql.user, mysql.db, mysql.tables_priv

---

## Triggers

### Trigger Creation

| Feature | V2 | Firebird | PostgreSQL | MySQL | SBLR Opcode |
|---------|----|----|----|----|-------------|
| CREATE TRIGGER | ❌ | ✅ | ✅ | ✅ | EXT_CREATE_TRIGGER |
| DROP TRIGGER | ❌ | ✅ | ✅ | ✅ | EXT_DROP_TRIGGER |
| BEFORE INSERT | ❌ | ✅ | ✅ | ✅ | Trigger timing |
| AFTER INSERT | ❌ | ✅ | ✅ | ✅ | Trigger timing |
| BEFORE UPDATE | ❌ | ✅ | ✅ | ✅ | Trigger timing |
| AFTER UPDATE | ❌ | ✅ | ✅ | ✅ | Trigger timing |
| BEFORE DELETE | ❌ | ✅ | ✅ | ✅ | Trigger timing |
| AFTER DELETE | ❌ | ✅ | ✅ | ✅ | Trigger timing |
| FOR EACH ROW | ❌ | ✅ | ✅ | ✅ | Row-level |
| FOR EACH STATEMENT | ❌ | ✅ | ✅ | ❌ | Statement-level |
| WHEN (condition) | ❌ | ✅ | ✅ | ❌ | Conditional |

### Trigger Variables

| Variable | V2 | Firebird | PostgreSQL | MySQL | Description |
|----------|----|----|----|----|-------------|
| NEW | ❌ | ✅ | ✅ | ✅ | New row values |
| OLD | ❌ | ✅ | ✅ | ✅ | Old row values |

---

## Summary of Critical Findings

### V2 Parser PostgreSQL Contamination

The following PostgreSQL-specific features are present in V2 parser:

| Feature | Location | Severity | Action |
|---------|----------|----------|--------|
| INSERT ... ON CONFLICT | Lines 2657-2741 | 🔴 HIGH | Document as intentional OR remove |
| UPDATE ... FROM | Lines 2782-2799 | 🔴 HIGH | Document as intentional OR remove |
| DELETE ... USING | Lines 2868-2885 | 🔴 HIGH | Document as intentional OR remove |
| DROP ... CASCADE | Various | ⚠️ MEDIUM | Firebird also has CASCADE |

**Decision Required:** Are these intentional V2 extensions or contamination?

### V2 Parser Critical Gaps

| Gap | Impact | SBLR Opcodes Exist |
|-----|--------|-------------------|
| PSQL (CREATE FUNCTION/PROCEDURE/TRIGGER) | Cannot define stored code | ✅ Yes (0x90-0xA8) |
| CTEs (WITH clause) | Cannot use common table expressions | ✅ Yes (0x60-0x62) |
| Advanced Grouping (ROLLUP/CUBE) | Cannot use advanced analytics | ✅ Yes (0x45-0x48) |

### MySQL Parser Gaps

| Gap | Impact |
|-----|--------|
| CREATE INDEX | Cannot create indexes |
| CREATE VIEW | Cannot create views |
| DROP TABLE/INDEX/VIEW | Cannot drop objects |

### PostgreSQL Parser Issues

| Issue | Impact |
|-------|--------|
| Bytecode format mismatches | ~70-80% statements fail at runtime |
| MERGE not supported by executor | Cannot execute MERGE |

---

**Related Documents:**
- `/docs/audit/parsers/CRITICAL_FINDINGS.md`
- `/docs/audit/parsers/SBLR_OPCODE_MAPPING.md`
- `/docs/audit/parsers/V2/SUMMARY.md`
- `/docs/audit/parsers/FirebirdSQL/SUMMARY.md`
- `/docs/audit/parsers/PostgreSQL/SUMMARY.md`
- `/docs/audit/parsers/MySQL/SUMMARY.md`
