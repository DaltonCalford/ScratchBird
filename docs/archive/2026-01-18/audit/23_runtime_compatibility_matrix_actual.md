# Runtime Compatibility Matrix (Actual)

Purpose: Consolidated matrix that cross-references protocol adapter → parser/compiler → known execution outcomes.

Status: derived from static code review; no runtime execution performed.

## Pipeline mapping (server-side)

| Protocol | Adapter | Compiler | Parser pipeline | Notes |
| --- | --- | --- | --- | --- |
| Native | `NativeAdapter` | `QueryCompilerV2` | Parser V2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 | Native ScratchBird SQL |
| PostgreSQL | `PostgresqlAdapter` | `PostgreSQLQueryCompiler` | PostgreSQL parser (direct SBLR) | PostgreSQL 16 surface |
| MySQL | `MySqlAdapter` | `MySQLQueryCompiler` | MySQL parser (direct SBLR) | MySQL 8.x surface |
| Firebird | `FirebirdAdapter` | `FirebirdQueryCompiler` | Firebird parser → SemanticAnalyzerV2 → BytecodeGeneratorV2 | Firebird 5.0 surface |

## Execution outcome summary (high level)

Legend: Works = bytecode matches executor, Partial = executes with limitations, Fails = bytecode mismatch or missing executor support.

| Protocol | Core DDL (CREATE/DROP/TRUNCATE) | Core DML (SELECT/INSERT/UPDATE/DELETE) | Transaction (BEGIN/COMMIT/ROLLBACK/SAVEPOINT) | Admin/SHOW/DCL | Notes |
| --- | --- | --- | --- | --- | --- |
| Native | Partial | Partial | Works | Partial | Follows Parser V2 compatibility limits (see `14_parser_v2_statement_reference_actual.md`) |
| PostgreSQL | Fails | Fails | Works | Partial/Fails | Transaction ops and some EXT_* admin ops execute; GRANT/REVOKE/SHOW/EXPLAIN/COPY do not |
| MySQL | Fails | Fails | Works | Partial | SHOW/DESCRIBE/SET AUTOCOMMIT/USE mostly work; core DDL/DML mismatched |
| Firebird | Partial | Partial | Works | Fails | V2 pipeline works for some DDL/DML; Firebird-specific parser stubs cause additional gaps |

## Per-statement compatibility grids

Legend: Pass = executor-compatible, Partial = executes with limitations, Fail = parse error or opcode mismatch.

### DDL core

| Statement | Native | PostgreSQL | MySQL | Firebird | Notes |
| --- | --- | --- | --- | --- | --- |
| CREATE DATABASE | Pass | Pass | Pass | Pass | Emulated schema path in all pipelines |
| CREATE SCHEMA | Pass | Pass | Fail | Fail | MySQL has CREATE DATABASE only; Firebird parser errors |
| CREATE DOMAIN | Pass | Pass | Fail | Pass | Firebird supports BASIC domains |
| CREATE TABLE | Partial | Fail | Fail | Partial | V2 bytecode limits; PG/MySQL format mismatch |
| CREATE INDEX | Fail | Fail | Fail | Fail | Executor expects name-based layout |
| CREATE VIEW | Fail | Fail | Fail | Fail | Executor expects SQL definition string |
| CREATE SEQUENCE | Fail | Fail | Fail | Fail | V2/Firebird reject; PG mismatch |
| CREATE ROLE | Fail | Pass | Fail | Fail | Executor expects rolename string only |
| CREATE USER | Fail | Fail | Fail | Fail | PG payload missing flags byte |
| CREATE POLICY (RLS) | Fail | Fail | Fail | Fail | Executor opcode exists; no parser emits payload |
| ALTER TABLE | Partial | Fail | Partial | Partial | MySQL rename-only; V2 limited actions |
| DROP TABLE | Fail | Fail | Fail | Fail | Bytecode mismatch or not parsed |
| DROP INDEX | Fail | Fail | Fail | Fail | Bytecode mismatch or not parsed |
| DROP VIEW | Fail | Fail | Fail | Fail | Bytecode mismatch or not parsed |
| DROP DATABASE | Pass | Pass | Pass | Pass | |
| DROP SCHEMA | Pass | Pass | Pass | Fail | MySQL aliases to DROP DATABASE |
| DROP DOMAIN | Pass | Pass | Fail | Pass | |
| TRUNCATE TABLE | Fail | Fail | Fail | Fail | |

### DML

| Statement | Native | PostgreSQL | MySQL | Firebird | Notes |
| --- | --- | --- | --- | --- | --- |
| SELECT | Partial | Fail | Fail | Partial | V2 multi-table/join limits |
| INSERT | Partial | Fail | Fail | Partial | Multi-row/SELECT sources dropped in V2/Firebird |
| UPDATE | Partial | Fail | Fail | Partial | |
| DELETE | Partial | Fail | Fail | Partial | |
| MERGE | Fail | Fail | Fail | Fail | PG emits EXT_MERGE; executor lacks support |
| UPDATE OR INSERT | Fail | Fail | Fail | Partial | Firebird compiles as INSERT only |

### Transaction and session

| Statement | Native | PostgreSQL | MySQL | Firebird | Notes |
| --- | --- | --- | --- | --- | --- |
| BEGIN/START TRANSACTION | Pass | Pass | Pass | Pass | |
| COMMIT | Pass | Pass | Pass | Pass | |
| ROLLBACK | Pass | Pass | Pass | Pass | |
| SAVEPOINT/RELEASE | Pass | Pass | Pass | Pass | |
| SET TRANSACTION | Pass | Pass | Pass | Pass | |
| SET AUTOCOMMIT | Pass | Fail | Pass | Fail | PG parser rejects; Firebird not implemented |
| SET SQL DIALECT | Pass | Fail | Fail | Fail | Firebird-style SET; only V2 parser emits EXT_SET_SQL_DIALECT |
| SET NAMES | Pass | Fail | Fail | Fail | Firebird-style SET; only V2 parser emits EXT_SET_NAMES |
| SET LOCAL_TIMEOUT | Pass | Fail | Fail | Fail | Firebird-style SET; only V2 parser emits EXT_SET_LOCAL_TIMEOUT |
| SET ROLE | Fail | Fail | Fail | Fail | Payload mismatch or not parsed |
| SET SESSION AUTHORIZATION | Fail | Fail | Fail | Fail | Payload mismatch or not parsed |
| SET CONSTRAINTS | Fail | Fail | Fail | Fail | PG payload mismatch; others not parsed |
| SET search_path | Pass | Pass | Pass | Fail | Executor only supports search_path |

### Admin, DCL, utility

| Statement | Native | PostgreSQL | MySQL | Firebird | Notes |
| --- | --- | --- | --- | --- | --- |
| SHOW TABLES | Pass | Fail | Pass | Fail | |
| SHOW DATABASES | Pass | Fail | Pass | Fail | |
| SHOW COLUMNS | Pass | Fail | Pass | Fail | |
| SHOW INDEXES | Pass | Fail | Pass | Fail | |
| SHOW CREATE TABLE | Pass | Fail | Pass | Fail | |
| DESCRIBE | Fail | Fail | Pass | Fail | |
| ANALYZE | Fail | Pass | Fail | Fail | |
| EXPLAIN | Fail | Fail | Fail | Fail | |
| COPY | Fail | Fail | Fail | Fail | |
| GRANT | Fail | Fail | Fail | Fail | |
| REVOKE | Fail | Fail | Fail | Fail | |
| LOCK/UNLOCK TABLES | Fail | Fail | Partial | Fail | MySQL emits LITERAL_NULL no-op |

## Known-good groups per protocol

- Native (Parser V2): CREATE DATABASE/SCHEMA/DOMAIN, transaction control, SET AUTOCOMMIT, SELECT/INSERT/UPDATE/DELETE with V2 limits; CREATE INDEX/VIEW and DROP/TRUNCATE fail due to bytecode format mismatch.
- PostgreSQL: EXT_CREATE_DATABASE/SCHEMA/DOMAIN, CREATE ROLE (not USER), EXT_ANALYZE, transaction control, SET search_path; most core DDL/DML fail due to bytecode mismatches.
- MySQL: EXT_CREATE_DATABASE, SHOW TABLES/DATABASES/COLUMNS/INDEXES/CREATE TABLE, DESCRIBE, SET AUTOCOMMIT, USE (search_path), transaction control; core DDL/DML fail due to bytecode mismatches.
- Firebird: CREATE DATABASE/TABLE/DOMAIN (partial), SELECT/INSERT/UPDATE/DELETE (partial), transaction control; CREATE INDEX/VIEW and DROP TABLE/INDEX/VIEW fail due to bytecode mismatches.

## Primary incompatibility drivers

- Parser emits SBLR layouts that do not match executor expectations (especially for CREATE TABLE/INDEX/VIEW, DROP/TRUNCATE, SELECT list/aliases, INSERT value lists).
- Executor only accepts one payload layout per opcode (no dialect-aware variants).
- Firebird/PostgreSQL/MySQL parsers emit richer constructs (CTE, MERGE, RETURNING, ON CONFLICT) not supported by executor.
