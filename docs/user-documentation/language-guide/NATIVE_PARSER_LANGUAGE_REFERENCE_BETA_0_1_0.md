# Native Parser V3 Language Reference (Beta 0.1.0)

## 1. Scope And Method

This document is an implementation audit of the V3 native parser pipeline in
beta `0.1.0`, based on actual code paths:

- Parser: `src/parser/parser_v3.cpp`
- AST model: `include/scratchbird/parser/ast_v3.h`
- Emitter: `src/parser/v3_emitter.cpp`
- Executor: `src/sblr/executor.cpp`

Status words used below:

- `Parser`: syntax is accepted and produces AST.
- `Emitter`: AST is emitted to SBLR v3 opcode/payload.
- `Executor`: opcode has execution handling in V3 executor.

## 2. Top-Level Statement Dispatch

`parseStatementInternal()` accepts:

- DDL: `RECREATE`, `CREATE`, `ALTER`, `DROP`, `TRUNCATE`
- DML: `SELECT`, `INSERT`, `UPDATE`, `UPDATE OR INSERT`, `DELETE`, `COPY`, `MERGE`
- Transaction: `BEGIN`, `START`, `PREPARE`, `COMMIT`, `ROLLBACK`, `SAVEPOINT`, `RELEASE SAVEPOINT`
- Session/control: `SET`, `SHOW`, `RESET`, `DESCRIBE`, `SECURITY LABEL`
- Utility: `EXPLAIN`, `ANALYZE`, `VALIDATE INDEX`, `SWEEP DATABASE`, `EXECUTE`, `CALL`, `CANCEL JOB`
- DCL/security: `GRANT`, `REVOKE` (plus feature-gated `REVOKE TOKEN`)
- Connection: `CONNECT`, `DISCONNECT`
- Metadata: `COMMENT`
- PSQL/UDR entry points: `DECLARE EXTERNAL FUNCTION`, UDR compile surfaces
- vNext extension surfaces: doc-path filter, TS bucket aggregation, search DSL, vector ANN, hybrid bridge, graph path, Redis Lua/stream group

## 3. Feature Gates (Parser Capability Keys)

Default `native` profile enables all keys listed here:

- `F_DOC_PATH_FILTER`: doc path filter statements
- `F_TS_BUCKET_AGG`: time-bucket aggregation statements
- `F_RRULE_SCHEDULE_SURFACE`: `CREATE/ALTER/DROP SCHEDULE`
- `F_SEARCH_QUERY_DSL`: search DSL and join/percolator surface
- `F_VECTOR_ANN`: ANN/vector query surface
- `F_HYBRID_BRIDGE_HINT`: hybrid bridge hints
- `F_ENGINE_PROFILE_CREATE`: `CREATE DATABASE EMULATED ...`
- `F_SECURITY_USER_ACCOUNT_DDL`: `CREATE/ALTER/DROP USER`
- `F_SECURITY_CONNECTION_RULE_DDL`: connection rule DDL
- `F_SECURITY_TOKEN_DDL`: token DDL and `REVOKE TOKEN`
- `F_SECURITY_QUOTA_PROFILE_DDL`: quota profile DDL
- `F_SECURITY_MODEL_POLICY_DDL`: policy DDL
- `F_LANGUAGE_UDR_COMPILE_BRIDGE`: UDR compile statement/function forms
- `F_EMBEDDED_SQL_TEMPLATE_COMPILE`: embedded SQL template compile form

Disabled features fail deterministically with `PRS_0503`.

## 4. SQL Object Coverage Audit

### 4.1 CREATE

`parseCreate()` supports:

- Core objects: `TABLE`, `INDEX`, `VIEW`, `SEQUENCE`, `SCHEMA`, `DATABASE`, `TABLESPACE`, `DOMAIN`, `TYPE`, `FUNCTION`, `PROCEDURE`, `TRIGGER`, `PACKAGE`, `EXCEPTION`
- Security/admin: `USER`, `ROLE`, `GROUP`, `POLICY`, `JOB`
- FDW/federation: `SERVER`, `FOREIGN TABLE`, `FOREIGN DATA WRAPPER`, `USER MAPPING`, `SYNONYM`, `PUBLIC SYNONYM`, `UDR`
- Extension/replication/admin surfaces: `EXTENSION`, `PUBLICATION`, `SUBSCRIPTION`, `ACCESS METHOD`, `STATISTICS`, `TRANSFORM`
- Native extension objects: `SEARCH INDEX`, `VECTOR INDEX`, `MEASUREMENT`, `SCHEDULE`, `CONNECTION RULE`, `TOKEN`, `QUOTA PROFILE`

`CREATE DATABASE` parser supports both native and emulated forms:

- Native form: `CREATE DATABASE <schema_path> ...`
- Emulated form: `CREATE DATABASE EMULATED <dialect> [ON SERVER <server>] <source_spec> ...`
  - Dialect allow-list check is enforced in parser.
  - Source spec can encode server/path patterns and is normalized into internal remote emulation path components.
  - Supports `WITH OPTIONS(...)` and `ALIAS/ALIASES ...`.

### 4.2 ALTER

`parseAlter()` supports:

- Core objects: `TABLE`, `SCHEMA`, `DATABASE`, `TABLESPACE`, `TYPE`, `DOMAIN`, `VIEW`, `INDEX`, `SEQUENCE`, `TRIGGER`, `FUNCTION`, `PROCEDURE`, `PACKAGE`
- Security/admin: `USER`, `ROLE`, `GROUP`, `POLICY`, `JOB`, `SYSTEM`
- FDW/federation: `SYNONYM`, `SERVER`, `FOREIGN TABLE`
- Extension/replication/admin: `EXTENSION`, `CONNECTION RULE`, `TOKEN`, `QUOTA PROFILE`, `SCHEDULE`
- Native extension indexes: `ALTER SEARCH INDEX`, `ALTER VECTOR INDEX`

`ALTER INDEX` actions include:

- `SET (...)`, `RESET (...)`
- `REBUILD [ONLINE|OFFLINE] [WITH (...)]`
- `REBALANCE [ONLINE|OFFLINE] [WITH (...)]`
- `RELOCATE TO FILESPACE|TABLESPACE ... [ONLINE|OFFLINE] [WITH (...)]`
- `LIGHT SCAN [WITH (...)]`
- `DIAGNOSTIC SCAN [WITH (...)]`
- `ALTER INDEX DEFAULTS FOR <index_type> SET|RESET (...)`
- rename and move-to-schema forms

### 4.3 DROP

`parseDrop()` supports:

- Core objects: `SCHEMA`, `DATABASE`, `TABLESPACE`, `TABLE`, `INDEX`, `VIEW`, `SEQUENCE`, `DOMAIN`, `TYPE`, `FUNCTION`, `PROCEDURE`, `TRIGGER`, `PACKAGE`, `EXCEPTION`
- Security/admin: `USER`, `ROLE`, `GROUP`, `POLICY`, `JOB`
- FDW/federation: `SYNONYM`, `UDR`, `SERVER`, `FOREIGN TABLE`, `USER MAPPING`
- Extension/replication/admin: `EXTENSION`, `PUBLICATION`, `SUBSCRIPTION`, `CONNECTION RULE`, `TOKEN`, `QUOTA PROFILE`, `SCHEDULE`
- Native extension indexes: `DROP SEARCH INDEX`, `DROP VECTOR INDEX`

### 4.4 RECREATE And TRUNCATE

- `RECREATE`: `TABLE`, `VIEW`, `PROCEDURE`, `FUNCTION`, `TRIGGER`, `PACKAGE`, `EXCEPTION`, `SEQUENCE`, `JOB`
- `TRUNCATE [TABLE] ... [RESTART IDENTITY | CONTINUE IDENTITY] [CASCADE]`

## 5. Data Type Audit

### 5.1 Parser Type Grammar (`parseTypeName`)

Supported type syntax includes:

- Bare and schema-qualified type names
- Firebird-style `TYPE OF <type_ref>` and `TYPE OF COLUMN <column_ref>`
- Two-word aliases normalized during parse:
  - `DOUBLE PRECISION`
  - `CHARACTER VARYING` -> `VARCHAR`
  - `BIT VARYING` -> `VARBIT`
- Numeric params: `(length)` / `(precision, scale)`
- Generic type argument lists: `(arg1, arg2, ...)`
- Array suffix: `[]` or `[n]`
- Time zone suffixes: `WITH TIME ZONE`, `WITHOUT TIME ZONE`

### 5.2 Emitter Type Mapping (`buildTypeSpec`)

Mapped textual names include:

- Integers/unsigned: `INT`, `INTEGER`, `BIGINT`, `SMALLINT`, `TINYINT`, `INT128`, `INT2`, `INT4`, `INT8`, `UINT8`, `UINT16`, `UINT32`, `UINT64`, `UINT128`
- Numeric/float: `DECIMAL`, `NUMERIC`, `BIGNUM`, `REAL`, `FLOAT`, `DOUBLE`, `DOUBLE PRECISION`, `MONEY`, `MEDIUMINT`
- Character/binary/blob: `CHAR`, `CHARACTER`, `VARCHAR`, `TEXT`, `BINARY`, `VARBINARY`, `BYTEA`, `BLOB`, `BLOB_TEXT`, `BIT`, `QBIT`
- Temporal: `DATE`, `TIME`, `TIMESTAMP`, `TIME_TZ`, `TIMESTAMP_TZ`, `INTERVAL`, `DATETIME`, `YEAR`
- JSON/XML/search/vector: `JSON`, `JSONB`, `JSONPATH`, `XML`, `TSVECTOR`, `TSQUERY`, `VECTOR`
- Network: `INET`, `CIDR`, `MACADDR`, `MACADDR8`
- Geometry: `GEOMETRY`, `POINT`, `LINESTRING`, `POLYGON`, `MULTIPOINT`, `MULTILINESTRING`, `MULTIPOLYGON`, `GEOMETRYCOLLECTION`
- Advanced logical/container: `ENUM`, `SET`, `ROW`, `COMPOSITE`, `DOMAIN`, `VARIANT`, `DYNAMIC`, `ARRAY`
- Range: `INT4RANGE`, `INT8RANGE`, `NUMRANGE`, `DATERANGE`, `TSRANGE`, `TSTZRANGE`

Unmapped names fall back to domain/reference encoding (`SBLR3_TYPE_DOMAIN`) using full type signature text.

### 5.3 Executor Type Opcode Mapping

Executor maps V3 type opcodes to core types, including all emitter outputs above plus opcodes such as:

- `SBLR3_TYPE_DECFLOAT16`
- `SBLR3_TYPE_DECFLOAT34`
- `SBLR3_TYPE_NULL`

## 6. Domain And Type System Audit

### 6.1 CREATE DOMAIN

`parseCreateDomain()` supports:

- `IF NOT EXISTS`
- Domain kinds:
  - `BASIC` (`AS <type>`)
  - `RECORD (...)`
  - `ENUM (...)`
  - `SET OF <type>`
  - `VARIANT (<type>, ...)`
- `INHERITS(<parent_domain>)`
- Domain constraints:
  - `NOT NULL`, `NULL`
  - `DEFAULT <expr>`
  - `CHECK (<expr>)`
  - optional named constraints via `CONSTRAINT <name>`
- `COLLATE <collation>`
- `WITH` blocks:
  - `WITH DIALECT(<name>)`
  - `WITH COMPAT(<name>)`
  - `WITH INTEGRITY(...)`
  - `WITH SECURITY(...)`
  - `WITH VALIDATION(...)`
  - `WITH QUALITY(...)`
  - `WITH OPTIONS(...)`

`WITH` block keys currently parsed:

- `INTEGRITY`: `UNIQUENESS`, `NORMALIZATION`, `NORMALIZATION_FUNCTION`
- `SECURITY`: `MASKING`, `MASK_PATTERN`, `ENCRYPTION`, `AUDIT_ACCESS`, `REQUIRE_PRIVILEGE` / `REQUIRE PRIVILEGE`
- `VALIDATION`: `FUNCTION`, `ERROR_MESSAGE`
- `QUALITY`: `PARSE_FUNCTION`, `STANDARDIZE_FUNCTION`, `ENRICH_FUNCTION`
- `OPTIONS`: `WRAP`

### 6.2 ALTER/DROP DOMAIN

`parseAlterDomain()` supports:

- `SET DEFAULT <expr>`
- `SET COMPAT <name>`
- `DROP DEFAULT`
- `DROP CONSTRAINT <name>`
- `DROP COMPAT`
- `ADD CHECK (<expr>)`
- `RENAME TO <name>`

`parseDropDomain()` supports `IF EXISTS`, multi-domain list, `RESTRICT`.

### 6.3 CREATE/ALTER/DROP TYPE

`parseCreateType()` supports:

- Kinds: `ENUM`, `RECORD`, `RANGE`, `BASE`, `SHELL`
- `WITH DIALECT(...)`, `WITH COMPAT(...)`, optional `COMMENT`
- Rich range/base option parsing (subtype/opclass/canonical/storage/etc.)

`parseAlterType()` supports:

- `RENAME TO`
- `RENAME VALUE ... TO ...`
- `SET SCHEMA`
- `SET (...)` range/base option updates
- `ADD VALUE ... [BEFORE|AFTER ...]`
- `FINALIZE`

`parseDropType()` supports `IF EXISTS`, multi-type list, `CASCADE|RESTRICT`.

## 7. Security Audit

### 7.1 Security DDL Surfaces

Implemented parser coverage:

- Accounts: `CREATE USER`, `ALTER USER`, `DROP USER`
- Roles/groups: `CREATE ROLE`, `DROP ROLE`, `CREATE GROUP`, `DROP GROUP`
- Policies: `CREATE POLICY`, `ALTER POLICY`, `DROP POLICY`
- Labels: `SECURITY LABEL [FOR provider] ON <object_type> <object_path> IS <text|NULL>`
- Connection/token/quota controls:
  - `CREATE/ALTER/DROP CONNECTION RULE`
  - `CREATE/ALTER/DROP TOKEN`
  - `CREATE/ALTER/DROP QUOTA PROFILE`
  - `REVOKE TOKEN` (feature-gated)

### 7.2 GRANT/REVOKE

`GRANT` privilege tokens:

- `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `TRUNCATE`, `REFERENCES`, `TRIGGER`
- `EXECUTE`, `EXECUTE EXTERNAL JOB`
- `USAGE`, `COPY`, `CREATE JOB`, `VIEW JOB HISTORY`, `ALL [PRIVILEGES]`

`REVOKE` privilege tokens:

- `SELECT`, `INSERT`, `UPDATE`, `DELETE`
- `EXECUTE`, `EXECUTE EXTERNAL JOB`
- `COPY`, `CREATE JOB`, `VIEW JOB HISTORY`, `ALL [PRIVILEGES]`
- optional `GRANT OPTION FOR`

Object scopes parsed in both:

- `ON TABLE|JOB|SEQUENCE|FUNCTION|PROCEDURE|SCHEMA|DATABASE ...`
- default object type fallback is table
- `TO`/`FROM` `PUBLIC` or named grantees
- `WITH GRANT OPTION` and `CASCADE|RESTRICT` handling where applicable

Executor-side security checks enforce ownership/superuser/job-admin constraints for sensitive grant/revoke paths.

## 8. Session, Transaction, And Metadata Audit

### 8.1 Transaction Statements

Implemented parse coverage:

- `BEGIN` / `START TRANSACTION`
- `PREPARE TRANSACTION`
- `COMMIT` (`AND CHAIN`, `AND NO CHAIN`, `RETAINING`, `PREPARED`)
- `ROLLBACK` (`TO SAVEPOINT`, `AND CHAIN`, `AND NO CHAIN`, `RETAINING`, `PREPARED`)
- `SAVEPOINT`, `RELEASE [SAVEPOINT]`

Transaction characteristic parsing includes:

- Isolation: `READ UNCOMMITTED`, `READ COMMITTED` variants, `REPEATABLE READ`, `SERIALIZABLE`, `SNAPSHOT [TABLE STABILITY]`
- Access mode: `READ ONLY`, `READ WRITE`
- Deferrability: `DEFERRABLE`, `NOT DEFERRABLE`
- Wait/timeout: `WAIT`, `NO WAIT`, `LOCK TIMEOUT`
- Firebird-style `RESERVING ... FOR SHARED|PROTECTED READ|WRITE`
- `AUTOCOMMIT` and `ON CONFLICT {COMMIT|ROLLBACK|ERROR [code]|KEEP}`

### 8.2 SET/RESET/SHOW

`SET` coverage includes:

- `SET TIME ZONE ...`
- `SET AUTOCOMMIT ...`
- `SET TRANSACTION ...`
- `SET CONSTRAINTS ...`
- `SET SQL DIALECT <1|2|3>`
- `SET NAMES <charset>`
- `SET LOCAL_TIMEOUT <seconds>`
- `SET SESSION AUTHORIZATION ...`
- `SET ROLE ...`
- `SET CONSISTENCY ...`
- `SET SERIAL CONSISTENCY ...`
- `SET CONCURRENCY MODE ...`
- `SET SINGLE_WRITER ON|OFF`
- `SET SEQUENCE|GENERATOR ... TO ...`
- Generic variable assignment `SET <name> =|TO <expr|DEFAULT>`

`RESET` coverage:

- `RESET ALL`
- `RESET SESSION AUTHORIZATION`
- `RESET ROLE`
- `RESET TIME ZONE`
- `RESET <variable>`

`SHOW` coverage includes:

- `SHOW ALL`, `SHOW TRANSACTION ISOLATION LEVEL`
- `SHOW TABLES`, `DATABASES`, `COLUMNS`, `INDEXES`, `CREATE TABLE`
- `SHOW TABLE|INDEX|TRIGGER|VIEW|PROCEDURE|FUNCTION|DOMAIN|GENERATOR|SCHEMA|ROLE`
- `SHOW GRANTS`
- `SHOW JOBS`, `SHOW JOB`, `SHOW JOB RUNS`
- `SHOW CHECKS`, `COLLATIONS`, `COMMENTS`, `DEPENDENCIES`, `PACKAGE`
- `SHOW SQL DIALECT`, `VERSION`, `DATABASE`, `SYSTEM`, `METRICS`
- `SHOW <variable>`

`DESCRIBE`/`DESC` maps to column-show behavior.

### 8.3 Metadata Statements

- `COMMENT ON` supports table/column/index/view/sequence/function/procedure/trigger/schema/database/role/constraint objects.

## 9. DML And Query Surface Highlights

Implemented parse coverage includes:

- `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `COPY`, `MERGE`
- CTE entry (`WITH`) for select/insert/update/delete
- Set operators: `UNION`, `INTERSECT`, `EXCEPT`
- Row limiting variants:
  - `LIMIT/OFFSET`
  - `FETCH FIRST|NEXT ... ROW[S] ONLY|WITH TIES`
  - Firebird `FIRST`, `SKIP`, `ROWS m [TO n]`
- Locking:
  - `FOR UPDATE`, `FOR NO KEY UPDATE`, `FOR SHARE`, `FOR KEY SHARE`
  - modifiers `WITH LOCK`, `NOWAIT`, `SKIP LOCKED`
- Deterministic checks:
  - `WITH TIES` requires `ORDER BY`
  - `WITH TIES` rejected with `SKIP LOCKED`

### 9.1 WITH / CTE Detail (Requested Audit Depth)

`WITH` clause implementation (`parseWithClause`) supports:

- `WITH RECURSIVE`
- Per-CTE column list: `cte_name(col1, col2, ...)`
- `AS (...)` CTE body with nested `WITH` + `SELECT`
- Materialization hinting:
  - `AS MATERIALIZED (...)`
  - `AS NOT MATERIALIZED (...)`
- Recursive traversal controls:
  - `SEARCH BREADTH FIRST BY ... SET ...`
  - `SEARCH DEPTH FIRST BY ... SET ...`
  - `CYCLE col[, ...] SET mark_col [TO expr] [DEFAULT expr] USING path_col`

CTEs are attached to `SELECT`, and also route through `WITH` statement entry for `INSERT`, `UPDATE`, and `DELETE`.

### 9.2 DML Detail (Requested Audit Depth)

`INSERT`:

- `OVERRIDING SYSTEM|USER VALUE`
- Sources: `VALUES (...)`, `SELECT ...`, `DEFAULT VALUES`
- `ON CONFLICT` target by column list or `ON CONSTRAINT ...`
- Actions: `DO NOTHING` / `DO UPDATE SET ... [WHERE ...]`
- Native consistency controls: `WITH|USING CONSISTENCY ... [AND SERIAL CONSISTENCY ...]`
- Conditional write controls: `IF|WHEN EXISTS|NOT EXISTS|<expr>`
- `RETURNING ...`

`UPDATE`:

- Alias handling
- `SET ...`
- PostgreSQL-style `FROM` + joins
- `WHERE ...`
- Consistency + conditional write controls
- `RETURNING ...`

`DELETE`:

- `DELETE FROM ...`
- Alias handling
- PostgreSQL-style `USING` + joins
- `WHERE ...`
- Consistency + conditional write controls
- `RETURNING ...`

`COPY`:

- Table or query form: `COPY table ...` or `COPY (SELECT ...) TO ...`
- Directions/targets: `FROM|TO`, `PROGRAM`, `STDIN`, `STDOUT`, file literal
- Option block (`WITH (...)` or bare `(...)`) with keys including:
  - `FORMAT` (`CSV|TEXT|BINARY`)
  - `DELIMITER`, `NULL`, `HEADER`, `QUOTE`, `ESCAPE`, `ENCODING`
  - `BATCH_SIZE`, `MAX_ERRORS`, `ON_ERROR` (`ABORT|SKIP`)

`MERGE`:

- `MERGE INTO ... USING ... ON ...`
- `WHEN MATCHED THEN UPDATE|DELETE`
- `WHEN NOT MATCHED [BY TARGET] THEN INSERT ... VALUES ...`
- `WHEN NOT MATCHED BY SOURCE THEN UPDATE|DELETE`

## 10. Known Partial/Gap Areas In 0.1.0

- `CREATE TYPE` parser is rich, but emitter currently writes minimal placeholder payload for `SBLR3_CREATE_TYPE` (`src/parser/v3_emitter.cpp`), and no `SBLR3_CREATE_TYPE` execution handler is present in `src/sblr/executor.cpp`.
- `CREATE DATABASE EMULATED` parser is detailed, but `CreateDatabaseStmt` emission currently uses minimal `SBLR3_CREATE_DATABASE` payload fields (`name` + placeholders) and does not carry the full parser-derived emulation contract (`source_spec`, option list, aliases, normalized remote path) in that emitter path.
- V3 executor opcode routing explicitly handles `SBLR3_CREATE_DATABASE_EMULATED` in vNext-contract dispatch, not `SBLR3_CREATE_DATABASE` in the V3 mutation switch. This means database-create behavior currently spans mixed paths and requires normalization/closure for a single canonical V3 contract.
- `ALTER SEARCH INDEX` and `ALTER VECTOR INDEX` currently support only `REBUILD` (with optional `ONLINE|OFFLINE`).
- `SET PARSER VERSION` is explicitly parsed then rejected as unsupported.
- `REVOKE` privilege token set is narrower than `GRANT` (`TRUNCATE`, `REFERENCES`, `TRIGGER`, `USAGE` are not accepted in current `parseRevoke()`).
- Domain/type backend supports `DomainKind` values beyond the current `CREATE DOMAIN` syntax surface; some advanced kinds are presently reached through type flows, not direct domain grammar.

## 11. Deterministic Rejection Contracts

Common deterministic parser codes used by this surface:

- `PRS_0503`: feature-gate not enabled
- `PRS_0504`: invalid payload/form/contract for otherwise recognized syntax
- `PRS_0505`: unsupported or incomplete statement form/object/action
- `PRS_0507`: schedule RRULE validation contract failures
- Security-specific paths also emit `SEC_*` deterministic codes for connection/token rule contracts

## 12. Evidence Files

- `src/parser/parser_v3.cpp`
- `include/scratchbird/parser/ast_v3.h`
- `src/parser/v3_emitter.cpp`
- `src/sblr/executor.cpp`
