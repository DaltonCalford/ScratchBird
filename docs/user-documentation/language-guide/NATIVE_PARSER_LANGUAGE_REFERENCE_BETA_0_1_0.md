# Native Parser V3 Language Reference (Beta 0.1.0)
Last modified: 2026-02-21

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
- Utility/admin: `EXPLAIN`, `ANALYZE`, `VALIDATE INDEX`, `VALIDATE [DATABASE]`, `SWEEP DATABASE`, `BACKUP [DATABASE]`, `RESTORE [DATABASE]`, `EXECUTE`, `CALL`, `CANCEL JOB`, `RESYNC REPLICATION CHANNEL`
- DCL/security: `GRANT`, `REVOKE` (plus feature-gated `REVOKE TOKEN`)
- Connection: `CONNECT`, `DISCONNECT`
- Metadata: `COMMENT`
- PSQL/UDR entry points: `DECLARE EXTERNAL FUNCTION`, UDR compile surfaces
- Cluster/cube/service control: `CLUSTER ...`, `SHOW CLUSTER ...`, `CUBE ...`, `SHOW CUBE ...`, `REFRESH CUBE ...`, `SERVICE CHANNEL ...`
- NoSQL bridge: `CQL ...`, `MONGO ...`, `CYPHER ...`, `REDIS ...`, `MILVUS ...`
- vNext extension surfaces: doc-path filter, TS bucket aggregation, search DSL, vector ANN, hybrid bridge, graph path, Redis Lua/stream group

### 2.1 Context-Sensitive Command Family Blocks (Code-Derived)

The following command families are listed directly from parser dispatch in:
`parseCreate()`, `parseAlter()`, `parseDrop()`, `parseSelect()`, `parseSet()`, and `parseShow()`.

`CREATE` (`parseCreate`) dispatches:

- `CREATE INDEX`
- `CREATE MEASUREMENT`
- `CREATE SCHEDULE`
- `CREATE CONNECTION RULE`
- `CREATE TOKEN`
- `CREATE QUOTA PROFILE`
- `CREATE EXTENSION`
- `CREATE PUBLICATION`
- `CREATE SUBSCRIPTION`
- `CREATE ACCESS METHOD`
- `CREATE STATISTICS`
- `CREATE TRANSFORM`
- `CREATE REPLICATION CHANNEL`
- `CREATE CDC TABLE`
- `CREATE DATABASE CONNECTION`
- `CREATE CLUSTER WORKLOAD CLASS|ROUTE`
- `CREATE CLUSTER ADMISSION POLICY|BINDING`
- `CREATE CUBE`
- `CREATE SCHEMA`
- `CREATE DATABASE` (includes `CREATE DATABASE EMULATED ...`)
- `CREATE TABLESPACE`
- `CREATE DOMAIN`
- `CREATE TABLE`
- `CREATE VIEW`
- `CREATE SEQUENCE`
- `CREATE FUNCTION`
- `CREATE PROCEDURE`
- `CREATE TRIGGER`
- `CREATE PACKAGE`
- `CREATE EXCEPTION`
- `CREATE TYPE`
- `CREATE USER`
- `CREATE USER MAPPING`
- `CREATE ROLE`
- `CREATE GROUP`
- `CREATE POLICY`
- `CREATE SERVER`
- `CREATE FOREIGN TABLE`
- `CREATE FOREIGN DATA WRAPPER`
- `CREATE PUBLIC SYNONYM`
- `CREATE SYNONYM`
- `CREATE UDR`
- `CREATE JOB`

`ALTER` (`parseAlter`) dispatches:

- `ALTER INDEX`
- `ALTER MEASUREMENT`
- `ALTER SCHEDULE`
- `ALTER CONNECTION RULE`
- `ALTER TOKEN`
- `ALTER QUOTA PROFILE`
- `ALTER EXTENSION`
- `ALTER REPLICATION CHANNEL`
- `ALTER CDC TABLE`
- `ALTER DATABASE CONNECTION`
- `ALTER CLUSTER WORKLOAD CLASS|ROUTE`
- `ALTER CLUSTER ADMISSION POLICY|BINDING`
- `ALTER CLUSTER SET STATE`
- `ALTER CUBE`
- `ALTER TABLE`
- `ALTER SCHEMA`
- `ALTER DATABASE`
- `ALTER TABLESPACE`
- `ALTER TYPE`
- `ALTER DOMAIN`
- `ALTER JOB`
- `ALTER POLICY`
- `ALTER SYSTEM`
- `ALTER VIEW`
- `ALTER SEQUENCE`
- `ALTER TRIGGER`
- `ALTER FUNCTION`
- `ALTER PROCEDURE`
- `ALTER PACKAGE`
- `ALTER ROLE`
- `ALTER USER`
- `ALTER GROUP`
- `ALTER SYNONYM`
- `ALTER SERVER`
- `ALTER FOREIGN TABLE`

`DROP` (`parseDrop`) dispatches:

- `DROP INDEX`
- `DROP SCHEDULE`
- `DROP CONNECTION RULE`
- `DROP TOKEN`
- `DROP QUOTA PROFILE`
- `DROP EXTENSION`
- `DROP PUBLICATION`
- `DROP SUBSCRIPTION`
- `DROP REPLICATION CHANNEL`
- `DROP CDC TABLE`
- `DROP DATABASE CONNECTION`
- `DROP CLUSTER WORKLOAD CLASS|ROUTE`
- `DROP CLUSTER ADMISSION POLICY|BINDING`
- `DROP CUBE`
- `DROP SCHEMA`
- `DROP DATABASE`
- `DROP TABLESPACE`
- `DROP TABLE`
- `DROP VIEW`
- `DROP JOB`
- `DROP DOMAIN`
- `DROP TYPE`
- `DROP FUNCTION`
- `DROP PROCEDURE`
- `DROP TRIGGER`
- `DROP PACKAGE`
- `DROP ROLE`
- `DROP GROUP`
- `DROP POLICY`
- `DROP EXCEPTION`
- `DROP SEQUENCE`
- `DROP SYNONYM`
- `DROP UDR`
- `DROP SERVER`
- `DROP USER`
- `DROP USER MAPPING`
- `DROP FOREIGN TABLE`
- `DROP MATERIALIZED VIEW`

`SELECT` (`parseSelect`) command block parses:

- `SELECT [DISTINCT [ON (...)] | ALL] <select_list>`
- Optional Firebird prefix row controls: `FIRST ...`, `SKIP ...`
- Optional clauses: `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`
- Row limiting families: `LIMIT/OFFSET`, `FETCH FIRST|NEXT ... ROW[S] ONLY|WITH TIES`, Firebird `ROWS m [TO n]`
- Optional Firebird planner controls: `PLAN ...`, `OPTIMIZE FOR ... ROWS`
- Set operations: `UNION [ALL]`, `INTERSECT [ALL]`, `EXCEPT [ALL]`
- Locking block: `FOR UPDATE`, `FOR SHARE`, `FOR NO KEY UPDATE`, `FOR KEY SHARE` with `WITH LOCK`, `NOWAIT`, `SKIP LOCKED`

`SET` (`parseSet`) dispatches:

- `SET SESSION AUTHORIZATION ...`
- `SET TIME ZONE ...`
- `SET AUTOCOMMIT ...`
- `SET TRANSACTION ...`
- `SET CONSTRAINTS ...`
- `SET SQL DIALECT <1|2|3>`
- `SET NAMES <charset>`
- `SET LOCAL_TIMEOUT <seconds>`
- `SET CONSISTENCY ...`
- `SET SERIAL CONSISTENCY ...`
- `SET CONCURRENCY MODE ...`
- `SET SINGLE_WRITER ON|OFF`
- `SET SEQUENCE ... TO ...`
- `SET GENERATOR ... TO ...`
- `SET ROLE ...`
- `SET TERM <new_terminator> [old_terminator]`
- `SET SCHEMA [=|TO] <schema_path|DEFAULT>`
- `SET CURRENT_SCHEMA [=|TO] <schema_path|DEFAULT>`
- `SET PARSER VERSION` (parsed then explicitly rejected)
- Generic variable assignment: `SET [SESSION|LOCAL] <name>[.<name>...] =|TO <expr|DEFAULT>`

`SHOW` (`parseShow`) dispatches:

- `SHOW ALL`
- `SHOW TRANSACTION ISOLATION LEVEL`
- `SHOW TABLES [FROM ...] [LIKE ...]`
- `SHOW DATABASES [LIKE ...]`
- `SHOW COLUMNS FROM ... [LIKE ...]`
- `SHOW INDEXES [FROM ...]`
- `SHOW INDEX [HEALTH|USAGE|STORAGE|CONTENTION|OPTIONS] <name>`
- `SHOW CREATE TABLE <name>`
- `SHOW TABLE [name]`
- `SHOW TRIGGER[S] [name]`
- `SHOW VIEW[S] [name]`
- `SHOW PROCEDURE[S] [name]`
- `SHOW FUNCTION[S] [name]`
- `SHOW DOMAIN[S] [name]`
- `SHOW GENERATOR[S]|SEQUENCE[S] [name]`
- `SHOW SCHEMA[S] [name]`
- `SHOW ROLE[S] [name]`
- `SHOW GRANTS [FOR name]`
- `SHOW JOBS [LIKE ...]`
- `SHOW JOB [name]`
- `SHOW JOB RUNS [FOR] name`
- `SHOW CHECK[S] [name]`
- `SHOW COLLATION[S] [LIKE ...]`
- `SHOW COMMENT[S] [name]`
- `SHOW DEPENDENCIES|DEPENDENCY [name]`
- `SHOW PACKAGE[S] <name>`
- `SHOW SQL DIALECT`
- `SHOW TIME ZONE`
- `SHOW VERSION`
- `SHOW DATABASE`
- `SHOW SYSTEM`
- `SHOW METRICS`
- `SHOW PARSER VERSION`
- Generic variable show: `SHOW <name>[.<name>...]`

Additional top-level show/control families (outside `parseShow()` proper):

- `SHOW CLUSTER STATE|ROUTING PLAN|ADMISSION STATUS`
- `SHOW CUBE STATS [<cube_name>]` and `CUBE SHOW STATS [<cube_name>]`

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
- Extension/replication/admin surfaces: `EXTENSION`, `PUBLICATION`, `SUBSCRIPTION`, `ACCESS METHOD`, `STATISTICS`, `TRANSFORM`, `REPLICATION CHANNEL`, `CDC TABLE`, `DATABASE CONNECTION`, `CLUSTER WORKLOAD CLASS|ROUTE`, `CLUSTER ADMISSION POLICY|BINDING`
- Native extension objects: `MEASUREMENT`, `SCHEDULE`, `CONNECTION RULE`, `TOKEN`, `QUOTA PROFILE`

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
- Extension/replication/admin: `EXTENSION`, `MEASUREMENT`, `CONNECTION RULE`, `TOKEN`, `QUOTA PROFILE`, `SCHEDULE`, `REPLICATION CHANNEL`, `CDC TABLE`, `DATABASE CONNECTION`, `CLUSTER WORKLOAD CLASS|ROUTE`, `CLUSTER ADMISSION POLICY|BINDING`, `CLUSTER SET STATE`

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
- Extension/replication/admin: `EXTENSION`, `PUBLICATION`, `SUBSCRIPTION`, `CONNECTION RULE`, `TOKEN`, `QUOTA PROFILE`, `SCHEDULE`, `REPLICATION CHANNEL`, `CDC TABLE`, `DATABASE CONNECTION`, `CLUSTER WORKLOAD CLASS|ROUTE`, `CLUSTER ADMISSION POLICY|BINDING`
- Materialized view form: `DROP MATERIALIZED VIEW ...` (parsed through `DropViewStmt` with materialized flag)

### 4.4 RECREATE And TRUNCATE

- `RECREATE`: `TABLE`, `VIEW`, `PROCEDURE`, `FUNCTION`, `TRIGGER`, `PACKAGE`, `EXCEPTION`, `SEQUENCE`, `JOB`
- `TRUNCATE [TABLE] ... [RESTART IDENTITY | CONTINUE IDENTITY] [CASCADE]`

### 4.5 RESYNC Surface

- Standalone command: `RESYNC REPLICATION CHANNEL <name> ...`
- Current parser/emitter contract normalizes this as `AlterSystemStmt` with key prefix `replication.channel.resync.<name>`.

### 4.6 SQL Object Lifecycle Matrix (Create/Alter/Show/Drop)

Legend:

- `Y`: explicit parser command family exists
- `P`: partial lifecycle (one or more lifecycle phases missing)
- `N`: no explicit parser command family

| SQL object family | CREATE | ALTER | SHOW/DESCRIBE | DROP | Lifecycle status |
|---|---|---|---|---|---|
| `DATABASE` | Y | Y | Y (`SHOW DATABASE`) | Y | Y |
| `SCHEMA` | Y | Y | Y (`SHOW SCHEMA`) | Y | Y |
| `TABLESPACE` | Y | Y | N | Y | P |
| `TABLE` | Y | Y | Y (`SHOW TABLE`, `SHOW TABLES`, `DESCRIBE`) | Y (`TRUNCATE` also) | Y |
| `VIEW` | Y | Y | Y (`SHOW VIEW`) | Y (`DROP MATERIALIZED VIEW` path) | Y |
| `SEQUENCE/GENERATOR` | Y | Y | Y (`SHOW GENERATOR`, `SHOW SEQUENCE`) | Y | Y |
| `DOMAIN` | Y | Y | Y (`SHOW DOMAIN`) | Y | Y |
| `TYPE` | Y | Y | N | Y | P |
| `INDEX` | Y | Y | Y (`SHOW INDEX`, `SHOW INDEXES`) | Y | Y |
| `FUNCTION` | Y | Y | Y (`SHOW FUNCTION`) | Y | Y |
| `PROCEDURE` | Y | Y | Y (`SHOW PROCEDURE`) | Y | Y |
| `TRIGGER` | Y | Y | Y (`SHOW TRIGGER`) | Y | Y |
| `PACKAGE` | Y | Y | Y (`SHOW PACKAGE`) | Y | Y |
| `EXCEPTION` | Y | N | N | Y | P |
| `USER` | Y | Y | N | Y | P |
| `ROLE` | Y | N | Y (`SHOW ROLE`) | Y | P |
| `GROUP` | Y | N | N | Y | P |
| `POLICY` | Y | Y | N | Y | P |
| `JOB` | Y | Y | Y (`SHOW JOB`, `SHOW JOBS`, `SHOW JOB RUNS`) | Y | Y |
| `SCHEDULE` | Y | Y | N | Y | P |
| `CONNECTION RULE` | Y | Y | N | Y | P |
| `TOKEN` | Y | Y | N | Y | P |
| `QUOTA PROFILE` | Y | Y | N | Y | P |
| `EXTENSION` | Y | Y | N | Y | P |
| `REPLICATION CHANNEL` | Y | Y | N | Y | P |
| `PUBLICATION` | Y | N | N | Y | P |
| `SUBSCRIPTION` | Y | N | N | Y | P |
| `CDC TABLE` | Y | Y | N | Y | P |
| `DATABASE CONNECTION` | Y | Y | N | Y | P |
| `CLUSTER WORKLOAD/ADMISSION` | Y | Y | Y (`SHOW CLUSTER ...`) | Y | Y |
| `SERVER` (`FOREIGN SERVER`) | Y | Y (generic object alter path) | N | Y | P |
| `FOREIGN TABLE` | Y | Y (generic object alter path) | N | Y | P |
| `FOREIGN DATA WRAPPER` | Y | N | N | N | P |
| `USER MAPPING` | Y | N | N | Y | P |
| `SYNONYM` | Y | Y (generic object alter path) | N | Y | P |
| `UDR` | Y | N | N | Y | P |
| `MEASUREMENT` | Y | Y | N | N | P |
| `ACCESS METHOD` | Y | N | N | N | P |
| `STATISTICS` | Y | N | N | N | P |
| `TRANSFORM` | Y | N | N | N | P |

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

### 6.4 System Domain Registry And Defaults (Engine-Backed)

System domain initialization is engine-backed and split across:

- Legacy bootstrap list: `kLegacySystemDomains` in `src/core/domain_manager.cpp`
- Canonical fixed rows: `kCanonicalSystemDomains` from `include/scratchbird/core/system_domain_registry_rows.inc`
- System-key domain family: `kSystemKeyDomains` in `src/core/domain_manager.cpp`

System-table/default binding flow is in `CatalogManager::applySystemDomainDefaults(...)`:

- table+column specific map (`kSystemDomainByTableColumn`)
- then column-name map (`kSystemDomainByColumn`)
- then fallback by physical type (`defaultDomainForType(DataType)`)

Full source-derived inventory (all domain names, family counts, and default-domain mappings):

- `docs/audit/SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`

### 6.5 Object Naming, Case, And UTF-8 Limits

Object-name rules are engine-backed (catalog layer), not guessed:

- max identifier length: `128` characters (`MAX_IDENTIFIER_CHARS`)
- max identifier storage: `512` bytes (`MAX_IDENTIFIER_BYTES`, UTF-8 worst case)
- unquoted identifiers are case-insensitive and normalized through uppercase compare
- quoted identifiers are case-sensitive and compared as-is

Practical implication for style choices like `camelCase` / `PascalCase`:

- if you need exact case preserved and case-sensitive lookup, use quoted names
- if you use unquoted names, lookup/uniqueness is case-insensitive regardless of original case
- UTF-8 identifiers are supported within the `128` character / `512` byte limits above

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

Grouped command families from direct parser dispatch:

`SET`:

- `SET SESSION AUTHORIZATION ...`
- `SET TIME ZONE ...`
- `SET AUTOCOMMIT ...`
- `SET TRANSACTION ...`
- `SET CONSTRAINTS ...`
- `SET SQL DIALECT <1|2|3>`
- `SET NAMES <charset>`
- `SET LOCAL_TIMEOUT <seconds>`
- `SET CONSISTENCY ...`
- `SET SERIAL CONSISTENCY ...`
- `SET CONCURRENCY MODE ...`
- `SET SINGLE_WRITER ON|OFF`
- `SET SEQUENCE ... TO ...`
- `SET GENERATOR ... TO ...`
- `SET ROLE ...`
- `SET PARSER VERSION` (parsed and rejected)
- Generic variable assignment: `SET [SESSION|LOCAL] <name>[.<name>...] =|TO <expr|DEFAULT>`

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
- `SHOW SQL DIALECT`, `TIME ZONE`, `VERSION`, `DATABASE`, `SYSTEM`, `METRICS`, `PARSER VERSION`
- Generic variable query: `SHOW <name>[.<name>...]`

`DESCRIBE` maps to column-show behavior.

### 8.3 Schema Paths And Schema Session Controls

Schema path grammar is a first-class v3 surface (`parseSchemaPath`):

- unqualified: `orders` (resolved through current/search path)
- current/home-relative: `.orders`, `.sub.orders`
- parent-relative: `..orders`, `..finance.orders`
- absolute: `sys.catalog.tables`
- no-search prefix: `!:orders` (disables search-path resolution)

Schema/path resolution semantics are enforced in executor/catalog path resolution:

- current schema is taken from connection/executor context when set
- if current schema is not set, default fallback resolution is `users.public` then `public`
- search path defaults to `public` when unset

Schema session-control commands in v3 `0.1.0`:

- implemented:
  - `SET SEARCH_PATH =|TO <expr_or_list>`
  - `SET SCHEMA <schema_path>`
  - `SET CURRENT_SCHEMA [TO] <schema_path|DEFAULT>`
  - `RESET SEARCH_PATH` (via generic `RESET <variable>`)
  - `SHOW SCHEMA [name]`
  - `SHOW search_path`
  - `SHOW current_schema`

Runtime behavior note:

- `SET SCHEMA`/`SET CURRENT_SCHEMA` resolve and store canonical schema paths (for example, `root.test`).
- `SET CURRENT_SCHEMA DEFAULT` resolves to the canonical public path (for example, `root.users.public`).
- Both commands set current schema context and collapse search path to that schema entry.

### 8.4 Metadata Statements

- `COMMENT ON` supports table/column/index/view/sequence/function/procedure/trigger/schema/database/role/constraint objects.

### 8.5 Admin, Maintenance, And Connection Commands

Direct parser coverage in v3 includes:

- `CONNECT [TO] <database> [USER <user>] [PASSWORD <password>] [ROLE <role>] [CHARSET [SET] <charset>]`
- `DISCONNECT [ALL|CURRENT|<connection_name>]`
- `BACKUP [DATABASE] ...`
- `RESTORE [DATABASE] ...`
- `VALIDATE INDEX ...`
- `VALIDATE [DATABASE] ...` (admin bridge surface)
- `CLUSTER SET STATE ...`
- `SHOW CLUSTER STATE|ROUTING PLAN|ADMISSION STATUS`
- `SERVICE CHANNEL BACKUP|EVENTS|PROGRESS ...`
- `ANALYZE [VERBOSE] <table> [(<column>)] [COLUMN <column>] [SAMPLE <rate>]`
- `ANALYZE [VERBOSE] INDEX <index_path> [SAMPLE <rate> | WITH (SAMPLE_RATE=<rate>)]`
- `VALIDATE INDEX <index_path> [WITH (<option>=<value>[, ...])]`
- `SWEEP DATABASE`
- `EXPLAIN [options] <SELECT|INSERT|UPDATE|DELETE>`

Code-backed notes:

- `VALIDATE INDEX ...` and admin `VALIDATE [DATABASE] ...` are both dispatched explicitly.
- `SWEEP` remains `SWEEP DATABASE` only.
- `CONNECT`/`DISCONNECT` are explicit top-level statements (`parseConnect`, `parseDisconnect`).

External database connection surfaces currently available:

- `CREATE SERVER <server_name> [TYPE <type>] [VERSION <version>] FOREIGN DATA WRAPPER <fdw_name> [OPTIONS (...)]`
- `CREATE USER MAPPING FOR <user_target> SERVER <server_name> [OPTIONS (...)]`
- `CREATE DATABASE EMULATED <dialect> [ON SERVER <server>] <source_spec> [WITH OPTIONS (...)] [ALIAS|ALIASES ...]`
- `CREATE DATABASE CONNECTION <name> ...`
- `ALTER DATABASE CONNECTION <name> ...`
- `DROP DATABASE CONNECTION <name> [IF EXISTS]`

Operational note for external mounts/security/auth:

- `CREATE/ALTER DATABASE CONNECTION` parser clauses validate endpoint (`HOST|ENDPOINT|URI`), `MOUNT`, auth mode (`AUTH_MODE|SECURITY` + `SHARED|NAMED`), and credential principal payload (`PASSWORD|ROLE|GROUP`).
- In `0.1.0`, this connection object family is normalized through `AlterSystemStmt` key contracts (`external.database_connection.*`) rather than a dedicated catalog object type/opcode family.

Required gap (for beta hardening / 0.2.0 planning):

- Runtime closure still needs promotion from `AlterSystemStmt` keys to a dedicated connection-object catalog/DDL path (single canonical object lifecycle with explicit metadata persistence rules).

Top-level parser dispatch now includes explicit handlers for:

- `BACKUP`, `RESTORE`, `CLUSTER`, `SERVICE CHANNEL`, and `SHOW CLUSTER ...`

Runtime closure note:

- These command families emit dedicated v3 bridge opcodes (`SBLR3_ADMIN_*`, `SBLR3_CLUSTER_*`,
  `SBLR3_SERVICE_CHANNEL_*`), but `0.1.0` executor routing still treats them as vNext semantic-bridge stubs
  and returns deterministic `BRG_0406` until explicit semantic handlers are implemented.

### 8.6 NoSQL/Polyglot Command Application Paths

The v3 parser currently applies NoSQL-family commands in two distinct ways:

- **Path A: vNext contract opcodes (parsed+emitted, bridge-rejected in executor 0.1.0)**
  - `DOC PATH FILTER ...` -> `AST_DOC_PATH_FILTER` -> `SBLR3_OP_DOC_PATH_FILTER`
  - `TS BUCKET AGG ...` -> `AST_TS_BUCKET_AGG` -> `SBLR3_OP_TS_BUCKET_AGG`
  - `SEARCH QUERY DSL ...` / `SEARCH DSL ... ON INDEX ...` -> `AST_SEARCH_QUERY_DSL` -> `SBLR3_OP_SEARCH_DSL_EVAL`
  - `VECTOR ANN QUERY ...` / `ANN INDEX ...` -> `AST_VECTOR_ANN_QUERY` -> `SBLR3_OP_VECTOR_ANN`
  - `HYBRID BRIDGE EXCHANGE ...` / `BRIDGE SOURCE ... TARGET ...` -> `AST_HYBRID_BRIDGE` -> `SBLR3_OP_HYBRID_BRIDGE_EXCHANGE`
  - Current executor behavior: these opcodes route through `executeVNextOpcode` and return deterministic `BRG_0406` (`semantic class change requires explicit bridge`).

- **Path B: normalized to `ALTER SYSTEM` config writes (parser+emitter+executor closed)**
  - Graph path compatibility surface:
    - `GRAPH PATH MATCH ...` / `MATCH GRAPH PATH ...`
    - normalized to key `graph.path.quantified` with canonical SQL payload text.
  - Redis Lua compatibility surface:
    - `REDIS LUA EVAL ...`
    - normalized to key `redis.lua.eval`.
  - Redis stream-group compatibility surface:
    - `REDIS STREAM GROUP CREATE|READ|CLAIM ...`
    - normalized to keys `redis.stream.group.create|read|claim`.
  - Search compatibility admin surface:
    - `SEARCH JOIN FIELD MAPPING ...`
    - `SEARCH PERCOLATOR FIELD ...`
    - normalized to keys `search.join_field.mapping` and `search.percolator.field`.
  - Current executor behavior: `SBLR3_ALTER_SYSTEM` sets `core::Config(section, setting, value)` (superuser only). This is configuration application, not execution of dedicated NoSQL runtime operators.

NoSQL bridge opcode families are generated by native v3 parser/emitter for:

- Cassandra: `SBLR3_CQL_*`
- Mongo: `SBLR3_MONGO_*`
- Cypher: `SBLR3_CYPHER_*`
- Redis data ops: `SBLR3_REDIS_*` (string/hash/list/set/zset/stream/pubsub families)
- Milvus: `SBLR3_MILVUS_*`

Current runtime status in `0.1.0` remains bridge-partial: executor vNext routing rejects these opcodes with `BRG_0406` until semantic handlers are implemented.

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

### 9.0 SELECT Command Group (Direct `parseSelect()` Dispatch)

- `SELECT [DISTINCT [ON (...)] | ALL] ...`
- Firebird prefix row controls: `FIRST ...`, `SKIP ...`
- Core clause flow: `FROM`, `WHERE`, `GROUP BY`, `HAVING`, `ORDER BY`
- Advanced grouping forms under `GROUP BY`: `ROLLUP(...)`, `CUBE(...)`, `GROUPING SETS (...)`
- Row limiting families:
  - `LIMIT/OFFSET`
  - `FETCH FIRST|NEXT ... ROW[S] ONLY|WITH TIES`
  - Firebird `ROWS m [TO n]`
- Firebird planner controls: `PLAN ...`, `OPTIMIZE FOR ... ROWS`
- Set operations: `UNION`, `INTERSECT`, `EXCEPT` with optional `ALL`
- Locking family: `FOR UPDATE|FOR SHARE|FOR NO KEY UPDATE|FOR KEY SHARE [OF ...]` with `WITH LOCK`, `NOWAIT`, `SKIP LOCKED`

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

### 9.3 Operator Coverage (Requested Audit Depth)

Operator precedence chain implemented in parser:

- logical: `OR`, `AND`, `NOT`
- comparison/predicate: `IS`, `IN`, `BETWEEN`, `LIKE`, `ILIKE`, `SIMILAR TO`, regex forms, JSON existence, array containment
- concat/bitwise/shift/arithmetic: `||`, `|`, `^`, `&`, `<<`, `>>`, `+`, `-`, `*`, `/`, `DIV`, `%`
- explicit cast suffix: `::type`

Fully closed (parser+emitter+executor) operator families:

- arithmetic: `+`, `-`, `*`, `/`, `DIV`, `%`
- comparison: `=`, `<>`, `<`, `<=`, `>`, `>=`, null-safe compare (`IS [NOT] DISTINCT FROM` path)
- logical three-valued: `AND`, `OR`, `NOT`, `IS [NOT] NULL`
- bitwise/shift: `&`, `|`, `^`, `<<`, `>>`, unary `~`
- predicate-special forms: `STARTING WITH`, `CONTAINING`
- JSON extract: `->`, `->>`
- pattern operators: `LIKE`, `ILIKE`, regex operators (`~`, `~*`, `!~`, `!~*`)
- membership/value-list operators: `IN (...)`, `NOT IN (...)`
- JSON hash extract operators: `#>`, `#>>`
- array containment operators: `@>`, `<@`, `&&`
- concat operator: `||` (runtime `SBLR3_FUNC_CONCAT`)

Partial semantics:

- `IN (subquery)` / `NOT IN (subquery)` remains bridge-partial in `0.1.0` (value-list membership is closed; subquery-membership routing is not fully closed).
- `?`, `?|`, `?&` are parsed as distinct ops (`JSON_EXISTS`, `JSON_EXISTS_ANY`, `JSON_EXISTS_ALL`) but emitter collapses all three to `SBLR3_FUNC_JSON_EXISTS`, so `ANY`/`ALL` semantics are not preserved in emitted opcode selection.
- array containment is implemented with structural JSON-array membership semantics in current evaluator path (not engine-specific GIN/GiST operator-class semantics).

Operator quick-reference examples:

| Family | Example | Typical use |
|---|---|---|
| Equality / inequality | `status = 'open'`, `amount <> 0` | row filtering and joins |
| Ordering compare | `event_ts >= NOW()` | range filtering |
| Null-safe compare | `a IS DISTINCT FROM b` | reliable comparisons with nullable columns |
| Membership | `id IN (1,2,3)` | key-set lookup |
| Pattern | `name ILIKE 'acme%'` | text prefix matching |
| Regex | `code ~ '^[A-Z]{3}[0-9]+$'` | strict token validation |
| JSON extract | `payload->'user'`, `payload#>>'{user,id}'` | semi-structured field projection |
| Array containment | `tags @> ARRAY['beta']` | feature/tag filters |
| Concat | `first_name || ' ' || last_name` | formatting and display projection |
| Arithmetic | `unit_price * qty` | derived metric computation |
| Bitwise / shift | `flags & 4`, `mask << 1` | flag math and bit-encoded controls |

### 9.4 Cast And Conversion Coverage (Requested Audit Depth)

Explicit casts:

- `CAST(expr AS type [USING format])`
- `expr::type`
- executor recognizes `USING` formats: `HEX`, `BASE64`, `ESCAPE`

Implicit coercion behavior in operator evaluation:

- text-to-numeric coercion for arithmetic operator families when counterpart is numeric
- text-to-temporal coercion for eligible temporal arithmetic/comparison paths
- text-to-comparison coercion for supported comparison target families
- temporal arithmetic supports temporal +/- interval and temporal-temporal subtraction-to-interval

Strict-mode control:

- `operator.strict_mode` disables implicit operator coercions for these paths
- parse/execution accepts `SET operator.strict_mode ON|OFF|TRUE|FALSE|1|0`

Known conversion limits:

- implicit comparison coercion is intentionally unsupported for some operand class combinations
- wide numeric (`INT256`/`UINT256`/`DECIMAL256`) support is not universal for `DIV_INT`, modulo, and bitwise operator families

Explicit and implicit conversion examples:

```sql
-- Explicit cast
SELECT CAST('2026-02-19' AS DATE);
SELECT '7f'::BYTEA;

-- Implicit numeric/text coercion (disabled when operator.strict_mode = ON)
SELECT '10' + 5;
SELECT '42' = 42;

-- Transaction-time vs wall-clock context
SELECT CURRENT_TIMESTAMP, NOW;
```

Guidance:

- Use explicit casts in DDL defaults, persistent ETL transforms, and cross-dialect SQL.
- Use implicit coercion only for compatibility shims; keep `operator.strict_mode=ON` in production validation pipelines.

### 9.5 Function Coverage (Requested Audit Depth)

Parser function surface:

- generic function-path calls with argument list
- aggregate call modifiers: `ORDER BY` inside call, `FILTER (WHERE ...)`
- window requirement validation for recognized window functions
- feature-gated builtin names for `DOC_PATH_*`, `TS_*`, `SEARCH_*`, `VECTOR_*`

Emitter function mapping includes:

- scalar families (examples): `COALESCE`, `NULLIF`, `ABS`, `SIN`, `COS`, `TAN`, `POWER`, `CONCAT`, JSON/text/date helpers
- aggregate families: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, statistical/regr families, `XMLAGG`, `ARRAY_AGG`
- text-search helpers: `TO_TSVECTOR`, `PLAINTO_TSQUERY`, `TO_TSQUERY`, `TSMATCH`, `TS_RANK`

Function/runtime closure status:
- scalar runtime coverage for `ABS`, `SIN`, `COS`, `TAN`, `POWER`, and `CONCAT` is now closed in current V3 evaluator path.
- window function opcodes are emitted for `ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, and `NTH_VALUE`.
- aggregate `DISTINCT` is parsed and emitted from function-call syntax (`COUNT(DISTINCT ...)` and related aggregate forms).

Emitter-mapped builtin function inventory (beta 0.1.0):

- Scalar/context: `POWER`, `ABS`, `SIN`, `COS`, `TAN`, `CONCAT`, `NOW`, `CURRENT_TIMESTAMP`, `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_USER`, `SESSION_USER`, `CURRENT_ROLE`, `CURRENT_CONNECTION`, `CURRENT_SESSION`, `CURRENT_TRANSACTION`
- Array/text: `ARRAY_POSITION`, `ARRAY_SLICE`, `ARRAY_SUBSCRIPT`, `REPLACE`, `ENDS_WITH`
- JSON: `JSON_EXTRACT`, `JSON_EXISTS`, `JSON_HAS_KEY`, `JSON_OBJECT`, `JSON_ARRAY`, `JSON_SET`, `JSON_INSERT`, `JSON_REMOVE`
- Formatting/conversion: `TO_CHAR`, `TO_DATE`, `TO_TIMESTAMP`, `LEAST`, `GREATEST`
- Aggregates/statistics: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, `STDDEV`, `STDDEV_SAMP`, `STDDEV_POP`, `VARIANCE`, `VAR_SAMP`, `VAR_POP`, `CORR`, `COVAR_POP`, `REGR_SLOPE`, `REGR_INTERCEPT`, `REGR_COUNT`, `REGR_R2`, `REGR_AVGX`, `REGR_AVGY`, `REGR_SXX`, `REGR_SYY`, `REGR_SXY`, `ARRAY_AGG`
- Window opcodes: `ROW_NUMBER`, `RANK`, `DENSE_RANK`, `LAG`, `LEAD`, `FIRST_VALUE`, `LAST_VALUE`, `NTH_VALUE`

### 9.6 Context Variables And Trigger Context Surfaces

Bare context-variable keywords accepted by native parser v3:

- `CURRENT_USER`
- `SESSION_USER`
- `CURRENT_ROLE`
- `CURRENT_CONNECTION`
- `CURRENT_SESSION` (normalized to `CURRENT_CONNECTION`)
- `CURRENT_TRANSACTION`
- `NOW`
- `CURRENT_DATE`
- `CURRENT_TIME`
- `CURRENT_TIMESTAMP`

Emitter/runtime closure:

- `NOW` -> `SBLR3_FUNC_NOW` (wall-clock at evaluation time)
- `CURRENT_TIMESTAMP` -> `SBLR3_FUNC_NOW` with CURRENT_TIMESTAMP semantic flag (transaction-start anchored when an active transaction exists; falls back to wall-clock when no transaction is active)
- `CURRENT_DATE` -> `SBLR3_FUNC_CURRENT_DATE`
- `CURRENT_TIME` -> `SBLR3_FUNC_CURRENT_TIME`
- `CURRENT_USER` / `SESSION_USER` -> `SBLR3_FUNC_CURRENT_USER`
- `CURRENT_ROLE` -> `SBLR3_FUNC_CURRENT_ROLE`
- `CURRENT_CONNECTION` / `CURRENT_SESSION` -> `SBLR3_FUNC_CURRENT_CONNECTION`
- `CURRENT_TRANSACTION` -> `SBLR3_FUNC_CURRENT_TRANSACTION`

Trigger context model in executor:

- row-level: `TriggerContext::getOldValue(column)` / `TriggerContext::getNewValue(column)` (`OLD.<col>` / `NEW.<col>` semantics)
- statement-level: `StatementTriggerContext` old/new transition-table accessors

### 9.7 Full Index/Domain Inventory Reference

System domain registry families in current engine inventory:

- `[sb_dom]` (core native family, 56 canonical rows)
- `[sb_pg_dom]` (PostgreSQL emulation family, 25 canonical rows)
- `[sb_my_dom]` (MySQL emulation family, 13 canonical rows)
- `[sb_cas_dom]` (Cassandra emulation family, 7 canonical rows)
- `[sb_mongo_dom]` (MongoDB emulation family, 13 canonical rows)
- `[sb_redis_dom]` (Redis emulation family, 8 canonical rows)
- `[sb_mil_dom]` (Milvus emulation family, 1 canonical row)

Core lifecycle note for system domains:

- system schema/domain bootstrapping applies table+column specific domain bindings first, then column-name defaults, then datatype fallback.
- unresolved system-domain mapping fails deterministically during DDL/bootstrap.

Parser-accepted index method tokens (native v3):

- `BTREE`, `HASH`, `HNSW`, `FULLTEXT`, `GIN`, `GIST`, `BRIN`, `RTREE`, `SPGIST`, `BITMAP`, `COLUMNSTORE`, `LSM`, `IVF`, `ZONEMAP`, `ART`, `BLOOM`
- `VECTOR_FLAT`, `VECTOR_BIN_FLAT`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`, `ANNOY`, `NSG`, `DISKANN`, `SCANN`, `GPU_CAGRA`
- `MINHASH_LSH`, `SPARSE_INVERTED`, `SPARSE_WAND`, `TRIE`, `INVERTED`, `STL_SORT`, `NGRAM`
- `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `MONGODB_GEO_HAYSTACK`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`
- `NEO4J_LOOKUP`, `NEO4J_TEXT`, `NEO4J_RANGE`, `NEO4J_POINT`, `NEO4J_VECTOR`
- `CASSANDRA_SASI`, `CASSANDRA_SAI`
- `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`

For full source-derived inventories of canonical + legacy system domain names, default-domain maps, index canonical-name validation sets, and context-variable opcode mapping, see:

- `docs/audit/SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`

### 9.8 SQL Object Lifecycle Playbooks (Requested Full-Lifecycle Coverage)

The following playbooks use parser-accepted forms and `SHOW` surfaces (not direct system-table probing).

`TABLE` + `INDEX` lifecycle:

```sql
CREATE TABLE sales.orders (
    order_id BIGINT PRIMARY KEY,
    customer_id BIGINT NOT NULL,
    total_amount DECIMAL(12,2) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX idx_orders_customer ON sales.orders(customer_id);
SHOW TABLE sales.orders;
SHOW INDEXES FROM sales.orders;
ALTER TABLE sales.orders ADD status VARCHAR(16) DEFAULT 'new';
ALTER INDEX idx_orders_customer REBUILD ONLINE;
DROP INDEX idx_orders_customer;
DROP TABLE sales.orders;
```

Why: this is the canonical relational lifecycle for transactional entities plus secondary-access tuning.

`SCHEMA` + `DATABASE` + `TABLESPACE` lifecycle:

```sql
CREATE DATABASE beta_main;
CREATE SCHEMA accounting;
CREATE TABLESPACE ts_fast LOCATION '/srv/scratchbird/ts_fast';
ALTER SCHEMA accounting RENAME TO acct;
ALTER DATABASE beta_main SET OWNER TO dba_role;
ALTER TABLESPACE ts_fast RESIZE AUTOEXTEND ON NEXT 256MB MAXSIZE 32GB;
SHOW DATABASE;
SHOW SCHEMA acct;
DROP TABLESPACE ts_fast;
DROP SCHEMA acct RESTRICT;
DROP DATABASE beta_main;
```

Why: this controls physical placement, ownership, and namespace boundaries before object-level DDL.

`DOMAIN` lifecycle:

```sql
CREATE DOMAIN positive_amount AS DECIMAL(12,2) CHECK (VALUE > 0);
SHOW DOMAIN positive_amount;
ALTER DOMAIN positive_amount SET DEFAULT 0.01;
DROP DOMAIN positive_amount RESTRICT;
```

Why: domains centralize validation/default rules so multiple tables stay consistent.

`TYPE` lifecycle:

```sql
CREATE TYPE priority_enum AS ENUM ('low', 'normal', 'high');
ALTER TYPE priority_enum ADD VALUE 'urgent' AFTER 'high';
DROP TYPE priority_enum RESTRICT;
```

Why: custom types lock down legal values and remove duplicated validation logic in application code.

`VIEW` + `SEQUENCE` lifecycle:

```sql
CREATE SEQUENCE seq_order_id START WITH 1 INCREMENT BY 1;
CREATE VIEW sales.order_totals AS
SELECT customer_id, SUM(total_amount) AS total_amount
FROM sales.orders
GROUP BY customer_id;
ALTER VIEW sales.order_totals SET SCHEMA reporting;
ALTER SEQUENCE seq_order_id RESTART WITH 1000;
SHOW VIEW reporting.order_totals;
SHOW SEQUENCE seq_order_id;
DROP VIEW reporting.order_totals;
DROP SEQUENCE seq_order_id;
```

Why: views publish stable read contracts; sequences provide deterministic surrogate key generation.

`PROCEDURE` + `EXCEPTION` + `PACKAGE` lifecycle:

```sql
CREATE EXCEPTION order_invalid 'Order is not valid for processing';

CREATE OR ALTER PROCEDURE update_status(order_id BIGINT)
AS
DECLARE order_total DECIMAL(12,2);
BEGIN
  SELECT total_amount INTO order_total FROM sales.orders WHERE order_id = :order_id;
  IF (order_total > 1000) THEN
    UPDATE sales.orders SET status = 'priority' WHERE order_id = :order_id;
  ELSIF (order_total > 500) THEN
    UPDATE sales.orders SET status = 'standard' WHERE order_id = :order_id;
  ELSE
    UPDATE sales.orders SET status = 'economy' WHERE order_id = :order_id;
  END IF;
WHEN ANY DO
  EXCEPTION order_invalid;
END;

CREATE PACKAGE order_pkg AS
BEGIN
  PROCEDURE update_status(order_id BIGINT);
END;
SHOW PROCEDURE update_status;
SHOW PACKAGE order_pkg;
DROP PACKAGE order_pkg;
DROP PROCEDURE update_status;
DROP EXCEPTION order_invalid;
```

Why: routines/packages encapsulate business rules and privileged workflows.

PSQL control-flow audit notes:

- supported: `IF`/`ELSIF`/`ELSE`, `CASE`, `WHILE`, `FOR`, `LOOP`, `LEAVE`, `CONTINUE`, `EXIT`, `WHEN`, `EXCEPTION`
- unsupported in native v3 parser: `TRY ...` block syntax
- `SET TERM <new_terminator> [old_terminator]` is parser-accepted and emitted through `SetStmt::SetType::TERM`.
- dollar-quoted routine bodies (`$$ ... $$`) remain unsupported in native v3 parser flow.
- native v3 routine bodies are parsed as structured blocks directly; delimiter-switch script splitting is still caller/session workflow.

`TRIGGER` lifecycle:

```sql
CREATE TRIGGER trg_orders_audit
BEFORE UPDATE ON sales.orders
FOR EACH ROW
AS
BEGIN
  IF (NEW.status IS DISTINCT FROM OLD.status) THEN
    INSERT INTO sales.order_audit(order_id, old_status, new_status, changed_at)
    VALUES (OLD.order_id, OLD.status, NEW.status, CURRENT_TIMESTAMP);
  END IF;
END;

ALTER TRIGGER trg_orders_audit INACTIVE;
ALTER TRIGGER trg_orders_audit ACTIVE;
SHOW TRIGGER trg_orders_audit;
DROP TRIGGER trg_orders_audit;
```

Why: triggers enforce row-level invariants and audit semantics close to data mutation boundaries.

`REPLICATION` + `CDC` lifecycle:

```sql
CREATE REPLICATION CHANNEL repl_orders DIRECTION ONE_WAY SOURCE sales TARGET analytics;
ALTER REPLICATION CHANNEL repl_orders DIRECTION BIDIRECTIONAL;
RESYNC REPLICATION CHANNEL repl_orders;
DROP REPLICATION CHANNEL repl_orders;

CREATE CDC TABLE sales.orders TRACK (LAST_MODIFIED_TXN_ID, ROW_UUID);
ALTER CDC TABLE sales.orders TRACK (LAST_MODIFIED_TXN_ID, ROW_UUID);
DROP CDC TABLE sales.orders;
```

Why: replication controls topology/sync direction; CDC gives row-change lineage for ETL and replay.

Important contract: CDC parser requires `TRACK` plus both `LAST_MODIFIED_TXN_ID` and `ROW_UUID`.

`PUBLICATION` + `SUBSCRIPTION` lifecycle:

```sql
CREATE PUBLICATION pub_orders FOR TABLE sales.orders;
CREATE SUBSCRIPTION sub_orders CONNECTION 'pg://subscriber/orders' PUBLICATION pub_orders;
DROP SUBSCRIPTION sub_orders;
DROP PUBLICATION pub_orders;
```

Why: publications define source-change streams; subscriptions bind downstream consumers.

`EXTENSION` lifecycle:

```sql
CREATE EXTENSION sb_vector WITH VERSION '1.0.0';
ALTER EXTENSION sb_vector UPDATE TO '1.1.0';
DROP EXTENSION sb_vector;
```

Why: extension lifecycle controls deployment of optional capabilities without core engine drift.

`EXTERNAL DATABASE CONNECTION` lifecycle:

```sql
CREATE DATABASE CONNECTION pg_finance
  HOST 'pg.internal'
  MOUNT 'finance'
  AUTH_MODE NAMED
  ROLE 'finance_reader'
  PASSWORD '***';

ALTER DATABASE CONNECTION pg_finance AUTH_MODE SHARED GROUP 'etl_workers';
DROP DATABASE CONNECTION pg_finance;
```

Why: this defines mount/authn/authz metadata for controlled external access from ScratchBird.

Required parser contract: endpoint (`HOST|ENDPOINT|URI`), `MOUNT`, `AUTH_MODE` (`SHARED|NAMED`), and at least one identity detail (`PASSWORD|ROLE|GROUP`) must all be present.

`FDW/Federation` lifecycle:

```sql
CREATE FOREIGN DATA WRAPPER pg_fdw HANDLER pg_fdw_handler;
CREATE SERVER pg_sales FOREIGN DATA WRAPPER pg_fdw OPTIONS (host 'pg01', port '5432');
CREATE USER MAPPING FOR CURRENT_USER SERVER pg_sales OPTIONS (user 'reader', password '***');
CREATE FOREIGN TABLE ext_orders (
  order_id BIGINT,
  total_amount DECIMAL(12,2)
) SERVER pg_sales OPTIONS (schema_name 'public', table_name 'orders');

ALTER SERVER pg_sales SET SCHEMA integration;
DROP FOREIGN TABLE ext_orders;
DROP USER MAPPING FOR CURRENT_USER SERVER pg_sales;
DROP SERVER pg_sales;
```

Why: this is the canonical pattern for controlled external table access with explicit connection metadata.

`SECURITY` lifecycle (`USER`/`ROLE`/`GROUP`/`POLICY`/`TOKEN`/`QUOTA PROFILE`):

```sql
CREATE USER app_user PASSWORD '***';
CREATE ROLE app_readwrite;
CREATE GROUP app_team;
CREATE POLICY orders_rls ON sales.orders USING (tenant_id = CURRENT_USER);
CREATE TOKEN api_token FOR USER app_user EXPIRY '2026-12-31';
CREATE QUOTA PROFILE q_profile LIMITS (cpu_ms=5000, io_mb=256);

ALTER USER app_user SET ROLE app_readwrite;
ALTER POLICY orders_rls USING (tenant_id = CURRENT_USER);
ALTER TOKEN api_token ROTATE;
ALTER QUOTA PROFILE q_profile LIMITS (cpu_ms=8000, io_mb=512);

DROP TOKEN api_token;
DROP POLICY orders_rls;
DROP GROUP app_team;
DROP ROLE app_readwrite;
DROP USER app_user;
DROP QUOTA PROFILE q_profile;
```

Why: this enforces identity, authorization, and resource controls as first-class database objects.

`JOB` + `SCHEDULE` lifecycle:

```sql
CREATE SCHEDULE sch_hourly RRULE 'FREQ=HOURLY';
CREATE JOB job_rollup
  SCHEDULE sch_hourly
  AS EXECUTE PROCEDURE reporting.refresh_rollups();

ALTER JOB job_rollup ENABLE;
SHOW JOB job_rollup;
SHOW JOB RUNS FOR job_rollup;
DROP JOB job_rollup;
DROP SCHEDULE sch_hourly;
```

Why: jobs and schedules operationalize repeatable maintenance/reporting workflows with auditable history.

`EMULATED DATABASE` lifecycle:

```sql
CREATE DATABASE EMULATED POSTGRESQL
  ON SERVER pg_main
  'postgresql://pg_main/erp'
  WITH OPTIONS (READ_ONLY = TRUE)
  ALIAS erp_pg;

SHOW DATABASE;
DROP DATABASE erp_pg;
```

Why: emulated database registration is the bridge for dialect compatibility and remote-source mapping.

`INDEX` + `MEASUREMENT` lifecycle:

```sql
CREATE INDEX idx_docs_search ON docs.articles USING FULLTEXT (content);
ALTER INDEX idx_docs_search REBUILD ONLINE;
DROP INDEX idx_docs_search;

CREATE INDEX idx_vec_docs ON docs.embeddings USING HNSW (embedding) WITH (metric='COSINE', topk_default=25);
ALTER INDEX idx_vec_docs REBUILD OFFLINE;
DROP INDEX idx_vec_docs;

CREATE MEASUREMENT ts.cpu_usage ON telemetry.events(timestamp, host, value);
ALTER MEASUREMENT ts.cpu_usage RETENTION 30d;
```

Why: these objects back text/vector/time-series query classes; they are tuned separately from classic btree/hash indexing.

`CLUSTER` lifecycle:

```sql
CREATE CLUSTER WORKLOAD CLASS oltp_high CONFIG '{"priority":"high"}';
CREATE CLUSTER WORKLOAD ROUTE route_oltp CONFIG '{"class":"oltp_high"}';
CREATE CLUSTER ADMISSION POLICY strict_policy CONFIG '{"max_qps":2000}';
CREATE CLUSTER ADMISSION BINDING bind_oltp CONFIG '{"route":"route_oltp","policy":"strict_policy"}';

ALTER CLUSTER WORKLOAD CLASS oltp_high CONFIG '{"priority":"critical"}';
CLUSTER SET STATE '{"mode":"active"}';
SHOW CLUSTER ROUTING PLAN;

DROP CLUSTER ADMISSION BINDING bind_oltp;
DROP CLUSTER ADMISSION POLICY strict_policy;
DROP CLUSTER WORKLOAD ROUTE route_oltp;
DROP CLUSTER WORKLOAD CLASS oltp_high;
```

Why: this defines routing/admission governance for multi-tenant or multi-workload deployments.

`CUBE` lifecycle:

```sql
CREATE CUBE sales_cube
  AS SELECT region, product, SUM(amount)
     FROM sales.fact_orders
     GROUP BY CUBE(region, product);

ALTER CUBE sales_cube REBUILD INCREMENTAL;
REFRESH CUBE sales_cube FULL;
SHOW CUBE STATS sales_cube;
DROP CUBE IF EXISTS sales_cube;
```

Why: cube objects materialize pre-aggregated OLAP rollups so repeated dimensional analytics avoid re-scanning large fact tables.

`ACCESS METHOD` + `STATISTICS` + `TRANSFORM` creation surfaces (lifecycle currently partial):

```sql
CREATE ACCESS METHOD am_custom TYPE INDEX HANDLER am_custom_handler;
CREATE STATISTICS st_orders (dependencies) ON customer_id, status FROM sales.orders;
CREATE TRANSFORM FOR jsonb LANGUAGE plpythonu (
  FROM SQL WITH FUNCTION jsonb_to_py,
  TO SQL WITH FUNCTION py_to_jsonb
);
```

Why: these objects register execution/planner extensibility metadata, but `ALTER`/`DROP`/`SHOW` lifecycle closure is not complete in `0.1.0`.

### 9.9 DML, NoSQL, And Cube Usage Rationale (Requested)

DML families and typical usage:

- `INSERT`: append new facts/events; pair with `ON CONFLICT` for idempotent ingest.
- `UPDATE`: in-place state changes where row identity is stable.
- `DELETE`: lifecycle cleanup and policy enforcement.
- `MERGE`: deterministic two-stream reconciliation in one statement.
- `COPY`: bulk ingest/export paths for large data movement.

NoSQL family equivalents exposed by native v3 parser/emitter:

- Cassandra surface:
  - `CQL KEYSPACE ...`
  - `CQL BATCH ...`
  - `CQL TTL ...`
  - `CQL WRITETIME ...`
- Mongo surface:
  - `MONGO FIND ...`
  - `MONGO AGGREGATE ...`
  - `MONGO FIND AND MODIFY ...`
  - `MONGO BULK WRITE ...`
- Cypher surface:
  - `CYPHER MATCH ...`
  - `CYPHER MERGE ...`
  - `CYPHER UNWIND ...`
  - `CYPHER CALL ...`
- Redis surface:
  - `REDIS STRING|HASH|LIST|SET|ZSET|STREAM|PUBSUB ...`
- Milvus surface:
  - `MILVUS CREATE/DROP COLLECTION ...`
  - `MILVUS CREATE/DROP INDEX ...`
  - `MILVUS INSERT|DELETE|SEARCH|QUERY ...`

Why: these forms keep a single SQL entrypoint while preserving dialect-specific intent in emitted bridge opcodes.

Concrete NoSQL command examples and usage rationale:

```sql
-- Cassandra-style keyspace and write-path controls
CQL KEYSPACE {'name':'orders_ks','replication':'NetworkTopologyStrategy'};
CQL BATCH QUERY {'statements':[...], 'consistency':'QUORUM'};
CQL TTL QUERY {'table':'orders','ttl':86400};
CQL WRITETIME QUERY {'table':'orders','column':'status'};

-- Mongo query and aggregation flows
MONGO FIND QUERY {'collection':'orders','filter':{'status':'open'}};
MONGO AGGREGATE QUERY {'collection':'orders','pipeline':[...]};
MONGO FIND AND MODIFY QUERY {'collection':'orders','query':{'_id':1},'update':{'$set':{'status':'done'}}};
MONGO BULK WRITE QUERY {'collection':'orders','ops':[...]} ;

-- Cypher graph access
CYPHER MATCH QUERY "MATCH (n:Order)-[:BELONGS_TO]->(c:Customer) RETURN n,c";
CYPHER MERGE QUERY "MERGE (n:Tag {name:'beta'})";
CYPHER UNWIND QUERY "UNWIND $rows AS r RETURN r";
CYPHER CALL QUERY "CALL db.labels()";

-- Redis data-family operations
REDIS STRING QUERY {'cmd':'SET','key':'order:1','value':'open'};
REDIS HASH QUERY {'cmd':'HSET','key':'order:1','field':'status','value':'open'};
REDIS LIST QUERY {'cmd':'LPUSH','key':'queue:orders','value':'1'};
REDIS SET QUERY {'cmd':'SADD','key':'tags','value':'beta'};
REDIS ZSET QUERY {'cmd':'ZADD','key':'scores','score':42,'member':'order:1'};
REDIS STREAM QUERY {'cmd':'XADD','stream':'orders','fields':{'id':'1','status':'open'}};
REDIS PUBSUB QUERY {'cmd':'PUBLISH','channel':'orders','payload':'updated'};

-- Milvus vector lifecycle
MILVUS CREATE COLLECTION {'name':'embeddings','dim':768};
MILVUS CREATE INDEX {'collection':'embeddings','field':'vec','index':'HNSW'};
MILVUS INSERT QUERY {'collection':'embeddings','rows':[...]} ;
MILVUS SEARCH QUERY {'collection':'embeddings','vector':[...], 'topk':20};
```

Why these NoSQL families are used:

- CQL commands: partitioned-write consistency and TTL/writetime semantics.
- Mongo commands: document query/update pipelines without flattening JSON ahead of time.
- Cypher commands: graph-pattern traversal where joins are less expressive.
- Redis commands: low-latency key/value, stream, and pub/sub operational paths.
- Milvus commands: high-dimensional vector indexing/search with ANN controls.

Runtime status for beta `0.1.0`: these NoSQL opcode families are emitted but still routed through semantic-bridge stubs (`BRG_0406`) until explicit runtime handlers land.

Cube/advanced-grouping usage:

```sql
SELECT region, product, SUM(amount)
FROM sales.fact_orders
GROUP BY CUBE(region, product);
```

Why: `ROLLUP`/`CUBE`/`GROUPING SETS` compute multi-level aggregates in one pass (detail + subtotal + grand-total style outputs).

Implementation note: advanced grouping executes, but `GROUPING()` value semantics are currently heuristic-based in executor for some shapes.

Cube materialization/object lifecycle note:

- Query-level `GROUP BY CUBE(...)` is parser+emitter+executor available.
- Cube-object command families are parser+emitter available in `0.1.0`:
  - `CREATE/ALTER/DROP CUBE`
  - `REFRESH CUBE`
  - `SHOW CUBE STATS` and `CUBE SHOW STATS`
- Runtime status remains bridge-partial in `0.1.0`: cube opcodes are dispatched through the same vNext semantic bridge path and still require explicit runtime handlers to remove `BRG_0406`.

## 10. Known Partial/Gap Areas In 0.1.0

- `CREATE TYPE` parser is rich, but emitter currently writes minimal placeholder payload for `SBLR3_CREATE_TYPE` (`src/parser/v3_emitter.cpp`), and no `SBLR3_CREATE_TYPE` execution handler is present in `src/sblr/executor.cpp`.
- `CREATE DATABASE EMULATED` parser is detailed, but `CreateDatabaseStmt` emission currently uses minimal `SBLR3_CREATE_DATABASE` payload fields (`name` + placeholders) and does not carry the full parser-derived emulation contract (`source_spec`, option list, aliases, normalized remote path) in that emitter path.
- V3 executor opcode routing explicitly handles `SBLR3_CREATE_DATABASE_EMULATED` in vNext-contract dispatch, not `SBLR3_CREATE_DATABASE` in the V3 mutation switch. This means database-create behavior currently spans mixed paths and requires normalization/closure for a single canonical V3 contract.
- legacy `SEARCH INDEX`/`VECTOR INDEX` command families were removed from native v3; canonical surface is `INDEX ... USING <method>`.
- `SET PARSER VERSION` is explicitly parsed then rejected as unsupported.
- `SET TERM` is parser+emitter accepted as a control statement, but full client-style delimiter-switch script splitting remains external to native v3 `parseStatements()` flow.
- `REVOKE` privilege token set is narrower than `GRANT` (`TRUNCATE`, `REFERENCES`, `TRIGGER`, `USAGE` are not accepted in current `parseRevoke()`).
- Domain/type backend supports `DomainKind` values beyond the current `CREATE DOMAIN` syntax surface; some advanced kinds are presently reached through type flows, not direct domain grammar.
- Value-list expression gaps are mostly closed in evaluator path; remaining partial is subquery-membership (`IN (subquery)` / `NOT IN (subquery)`).
- `JSON_EXISTS_ANY` / `JSON_EXISTS_ALL` parse distinctions currently collapse to a single emitted `JSON_EXISTS` opcode family in emitter mapping.
- Window function emission is only fully specialized for `ROW_NUMBER`, `RANK`, `DENSE_RANK`; additional parsed window names currently route through fallback opcode behavior.
- Dollar-quoted routine bodies (`$$ ... $$`) are not implemented in native v3 parser flow.
- Admin-level `CLUSTER`/`BACKUP`/`RESTORE` are now top-level dispatched and emitted, but still runtime-bridge partial in `0.1.0` (`BRG_0406` until semantic handlers are implemented).
- External connection-object commands (`CREATE/ALTER/DROP DATABASE CONNECTION`) are parsed and normalized in native v3, but are currently emitted as `AlterSystemStmt` key contracts (`external.database_connection.*`) rather than a dedicated catalog object lifecycle/opcode family.
- Lifecycle closures are incomplete for several create surfaces (no full `CREATE+ALTER+SHOW+DROP` closure in `0.1.0`), including:
  - `ACCESS METHOD`
  - `STATISTICS`
  - `TRANSFORM`
  - `FOREIGN DATA WRAPPER`
  - `MEASUREMENT` (no drop path)
  - `PUBLICATION`/`SUBSCRIPTION` (no alter path)
  - `EXCEPTION` (no alter path)
- Cube-object command families are parser+emitter mapped in `0.1.0`, but runtime remains vNext bridge-partial until explicit semantic handlers replace `BRG_0406` for cube opcodes.
- Full code-backed matrix of all currently identified missing/partial elements is maintained in `docs/audit/PARSER_V3_MISSING_PARTIAL_MATRIX_BETA_0_1_0.md`.

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
