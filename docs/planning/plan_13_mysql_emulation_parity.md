# Plan 13 - MySQL Emulation Parity (Parser + Protocol + Catalog)

## Scope
Achieve 1:1 MySQL 8.0 client compatibility over the MySQL wire protocol, including SQL parser coverage, protocol flows, system catalogs (information_schema, mysql, performance_schema), SHOW/DESCRIBE behavior, authentication semantics, and FDW/UDR passthrough. Prevent ScratchBird-only features from leaking into the MySQL dialect.

## Priority
P0 (Alpha requirement).

## References
- `docs/specifications/mysql_mariadb_spec.md`
- `docs/specifications/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/findings/mysql_emulation_parity_audit.md`
- `docs/findings/mysql_wire_protocol_gaps.md`
- `docs/planning/appendix_mysql_catalog_columns.md`
- `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`
- `docs/specifications/draft_security_architecture_specification.md`

## Decisions / Constraints (Resolved)
- Emulated databases live under schema path `remote.emulated.mysql.<server>.<db>` (full path `/remote/emulated/mysql/<server>/<db>`). The server name is part of the path to avoid collisions.
- On emulated database creation/connection, create catalog views scoped to that database only (no cross-database leakage).
- Catalog objects implemented as views must **appear as tables** where MySQL expects tables (use metadata overrides in the emulated catalogs).
- MySQL user identity is `(user, host)` (host-specific user records). Host-specific auth precedence and wildcard matching must be honored.
- Auth plugins required in Alpha: `mysql_native_password`, `caching_sha2_password`.
- `SHOW` is supported via adapter rewrite to information_schema queries.
- No emulated replication (binlog/GTID dump) in Alpha; reject with clear errors.
- Deterministic hash ID mapping for MySQL-specific IDs (table_id, index_id, column_id, constraint_id, etc) with collision table.
- MySQL transaction semantics must match native:
  - Default autocommit = ON.
  - Explicit `START TRANSACTION` / `BEGIN` opens a transaction block; autocommit is suspended until COMMIT/ROLLBACK.
  - `SET autocommit=0/1` is supported and must match MySQL state flags.

## Order of Implementation
1) Schema mapping + emulated catalog bootstrap (server/db schema + view generation + filter scoping).
2) MySQL parser completion + dialect guardrails (no ScratchBird feature bleed).
3) Protocol adapter parity (handshake/auth/session/prepared statements + transaction state).
4) Catalog coverage (information_schema, mysql, performance_schema) with exact columns from appendix.
5) SHOW/DESCRIBE rewrite and metadata output parity.
6) FDW/UDR passthrough completion for MySQL legacy migration.
7) Tests (parser, protocol, catalog, native client compatibility).

## Concrete Code Touchpoints (Exact Files + Functions)
- Parser:
  - `src/parser/mysql/mysql_parser.cpp`
    - `parseAlterStmt` (stub)
    - `parseDropStmt` (stub)
    - `parseTruncateStmt` (stub)
    - `parseCreateIndex` / `parseCreateView` / `parseCreateDatabase` / `parseCreateProcedure` / `parseCreateFunction` / `parseCreateTrigger`
    - `parseIndexDef`, `parseForeignKeyDef` (TODO)
    - `parseComparisonExpr` (`<=>` NULL-safe semantics)
    - `parseLikeExpr` (ESCAPE)
    - Placeholder handling (currently emits `LITERAL_NULL`)
    - `parseQualifiedName` (restrict to db.table, db.table.column)
    - `parseUseStmt` (default_schema path must include server name)
  - `include/scratchbird/parser/mysql/mysql_parser.h`
    - Update schema path comment and any hard-coded `localhost` semantics.
- Adapter:
  - `src/protocol/adapters/mysql_adapter.cpp`
    - `ensureRemoteClient` (search_path, server name, database path)
    - `bootstrapInformationSchema` (replace with view-based catalogs)
    - `handleChangeUser`, `handleResetConnection` (TODOs)
    - `COM_INIT_DB` validation (TODO)
    - `computeNativePasswordAuth` (use AuthManager instead of trust mode)
    - Auth plugin negotiation (`mysql_native_password`, `caching_sha2_password`)
  - `include/scratchbird/protocol/adapters/protocol_adapter.h`
    - `ProtocolAdapterConfig` (add `server_name` or `emulated_server_name`).
- Catalog/Views:
  - `include/scratchbird/catalog/emulation_view_generator.h` (expand MySQL view definitions, add placeholder substitution)
  - `include/scratchbird/catalog/mysql_catalog.h` (implement; no longer stub)
  - `include/scratchbird/catalog/information_schema.h` (implement; no longer stub)
  - `src/catalog/virtual_catalog.cpp` (register handlers; route by protocol)
- Mapping:
  - `src/core/tid_resolver.*` (add MySQL ID mapping table + hash)
  - `docs/planning/plan_02_uuid_resolution_and_rename_move.md` (resolver interface reference)
- FDW/UDR:
  - `src/fdw/mysql_adapter.cpp` (TODOs for result parsing, auth)
  - `src/fdw/protocol_adapter.cpp` (factory TODOs)
- Tests:
  - `tests/unit/test_mysql_parser.cpp`
  - `tests/unit/test_protocol_adapter_dialects.cpp`
  - `tests/integration/test_client_server_integration.cpp`
  - Add new tests under `tests/integration` and `tests/unit` (see Testing section).

## Required Data/Schema Changes
- Add MySQL emulation ID mapping table (shared with other dialects):
  ```sql
  CREATE TABLE sys.emulation.emulated_id_map (
    emulated_id BIGINT NOT NULL,
    object_uuid UUID NOT NULL,
    object_type SMALLINT NOT NULL,
    dialect_tag TEXT NOT NULL, -- 'mysql'
    created_time BIGINT NOT NULL,
    PRIMARY KEY (dialect_tag, object_type, object_uuid),
    UNIQUE (dialect_tag, object_type, emulated_id)
  );
  CREATE INDEX emulated_id_map_uuid_idx ON sys.emulation.emulated_id_map(object_uuid);
  CREATE INDEX emulated_id_map_emul_idx ON sys.emulation.emulated_id_map(emulated_id);
  ```
- Add collision table for deterministic hash collisions:
  ```sql
  CREATE TABLE sys.emulation.emulated_id_collision (
    collision_id UUID PRIMARY KEY,
    dialect_tag TEXT NOT NULL,
    object_type SMALLINT NOT NULL,
    emulated_id BIGINT NOT NULL,
    first_uuid UUID NOT NULL,
    second_uuid UUID NOT NULL,
    created_time BIGINT NOT NULL,
    resolved SMALLINT NOT NULL
  );
  ```
- Catalog views for MySQL must be created per emulated DB schema:
  - `remote.emulated.mysql.<server>.<db>.information_schema.*`
  - `remote.emulated.mysql.<server>.<db>.mysql.*`
  - `remote.emulated.mysql.<server>.<db>.performance_schema.*`
  - These are views/synonyms that point to `sys.catalog.*`, `sys.cluster.configuration.*`, `sys.security.*`, and `sys.runtime.*` (no physical tables under the emulated schema).

## Implementation Tasks (Detailed)

### 1) Schema Mapping and Emulated Catalog Bootstrap
- Add `server_name` to `ProtocolAdapterConfig` and set it from listener config (default: `local`).
- Update MySQL adapter default schema path to:
  - `remote.emulated.mysql.<server>.<db>` for the active database.
- Replace hard-coded `localhost` paths in:
  - `Parser::parseUseStmt` and any schema path comments.
- Update `MySqlAdapter::ensureRemoteClient`:
  - `search_path` must be `"remote.emulated.mysql.<server>.<db>"`.
  - Call `EmulationViewGenerator::generateServerSchema()` and `generateEmulatedViews()` on connect.
- Implement placeholder substitution in `EmulationViewGenerator::createEmulatedView`:
  - Replace `{schema_id}`, `{server_name}`, `{database_name}` in the SQL text before create.
  - `schema_id` must be the emulated DB schema id.
- Implement mapping of system schemas for MySQL:
  - Adapter rewrites `information_schema.`, `mysql.`, `performance_schema.` to the emulated schema path (fully qualified) before compilation.
  - Example rewrite: `information_schema.tables` -> `"remote.emulated.mysql.<server>.<db>.information_schema"."tables"`.

### 2) Parser Coverage and Dialect Guardrails
- Remove all MySQL parser stubs and TODOs listed in the audit:
  - `parseAlterStmt`, `parseDropStmt`, `parseTruncateStmt`.
  - CREATE INDEX/VIEW/DATABASE/PROCEDURE/FUNCTION/TRIGGER.
  - Table constraints and foreign key parsing.
  - `<=>` NULL-safe equality semantics.
  - ESCAPE handling for LIKE.
  - Proper placeholder emission (not `LITERAL_NULL`).
  - Geometry types mapping.
- Dialect guardrails (reject ScratchBird-only features):
  - Reject schema path tokens (PARENT/CURRENT/ABSOLUTE).
  - Reject `CREATE DOMAIN`, `ALTER DOMAIN`, ScratchBird domain blocks.
  - Reject `ALTER SCHEMA ... RENAME` and multi-segment schema paths.
  - Enforce MySQL `SCHEMA` == `DATABASE` (no additional hierarchy).
- Qualified name restrictions:
  - Table references: allow `table` or `db.table` only.
  - Column references: allow `col`, `table.col`, `db.table.col` only.

### 3) Protocol Adapter Parity
- Implement full authentication for MySQL:
  - `mysql_native_password` using proper SHA1 handshake.
  - `caching_sha2_password` with fast auth and full auth (RSA key exchange if TLS not used).
  - Validate credentials via ScratchBird AuthManager and MySQL-specific user/host lookup.
- Implement MySQL host-based user semantics:
  - User identity key `(User, Host)`; choose most specific host match.
  - Support wildcard hosts (`%`, `_`), `localhost` special case.
  - If multiple matches, select highest specificity (exact > subnet > wildcard).
- Implement session API gaps:
  - `COM_INIT_DB` must validate database existence in emulated schema.
  - `COM_CHANGE_USER` must re-run authentication and reset session state.
  - `COM_RESET_CONNECTION` must reset variables and transaction state.
  - Prepared statement flows (`COM_STMT_PREPARE`, `COM_STMT_EXECUTE`, `COM_STMT_SEND_LONG_DATA`, cursor operations).
- Implement MySQL transaction behavior:
  - Maintain server_status `AUTOCOMMIT` flag and set it correctly in OK packets.
  - Autocommit ON:
    - For each statement when not in an explicit transaction block, execute in its own transaction:
      1) Ensure a transaction exists (ScratchBird always has one).
      2) Execute statement.
      3) COMMIT.
      4) Immediately start a new default transaction for the next statement.
  - Autocommit OFF:
    - Statements run in the current transaction until COMMIT/ROLLBACK.
  - `START TRANSACTION` / `BEGIN`:
    - If autocommit ON, begin explicit block; do not auto-commit until COMMIT/ROLLBACK.
    - If a transaction is already active, apply ScratchBird conflict action default (rollback then start new).
  - `SET AUTOCOMMIT = 0|1`:
    - Treat as transaction-bound command; handle current transaction per ScratchBird conflict-action rules.

### 4) Catalog Coverage (information_schema, mysql, performance_schema)
- Implement view definitions for **all** tables in `docs/planning/appendix_mysql_catalog_columns.md`.
- All views must filter to the emulated DB schema id (`{schema_id}` placeholder).
- Use per-table mapping rules:
  - information_schema views map to ScratchBird catalog tables (`sys.catalog.*` and `sys.catalog.domain_*` views).
  - mysql.* tables map to ScratchBird security catalog tables (`sys.security.users`, `sys.security.roles`,
    `sys.security.permissions`, `sys.security.auth_plugins`).
  - performance_schema views map to ScratchBird runtime instrumentation (or return empty sets if not implemented, but with exact columns).
- Ensure **TABLE_TYPE** and related metadata reflect MySQL expectations:
  - If a MySQL catalog expects a base table, return `BASE TABLE` even if implemented as a view.
- Use `appendix_mysql_catalog_columns.md` as the authoritative column list. Do not omit or reorder columns.

### 5) SHOW/DESCRIBE Rewrite
- Implement SHOW rewrites in the MySQL adapter (server-side rewrite before compilation):
  - `SHOW DATABASES` -> `SELECT SCHEMA_NAME AS Database FROM information_schema.SCHEMATA`.
  - `SHOW TABLES` -> `SELECT TABLE_NAME AS Tables_in_<db> FROM information_schema.TABLES WHERE TABLE_SCHEMA = DATABASE()`.
  - `SHOW COLUMNS FROM tbl` -> map to `information_schema.COLUMNS` columns.
  - `SHOW INDEXES FROM tbl` -> map to `information_schema.STATISTICS`.
- Ensure column names and types in SHOW results match MySQL output.

### 6) FDW/UDR Passthrough
- Complete MySQL FDW adapter:
  - Implement result row parsing (text/binary).
  - Use MySQL client auth with `mysql_native_password` and `caching_sha2_password`.
- Implement `ProtocolAdapterFactory` instantiation for MySQL.
- Ensure passthrough queries can be executed through the UDR pipeline without leaking ScratchBird semantics.

## Completion Checklist (Developer)
- [ ] MySQL parser TODOs removed; DDL/DML features match MySQL spec.
- [ ] Dialect guardrails reject ScratchBird-only features.
- [ ] Protocol adapter supports native MySQL clients for auth and statements.
- [ ] MySQL autocommit semantics and status flags match native behavior.
- [ ] information_schema/mysql/performance_schema tables exist with exact columns.
- [ ] SHOW and DESCRIBE results match MySQL outputs.
- [ ] FDW passthrough works with legacy MySQL.

## Completion Checklist (Auditor)
- [ ] Parser tests include positive/negative cases for MySQL-only syntax.
- [ ] No multi-level ScratchBird schema paths accepted by MySQL parser.
- [ ] Protocol traces for handshake/auth match MySQL 8.0.
- [ ] Autocommit ON executes per-statement transactions; OFF leaves transaction open.
- [ ] Catalog column lists exactly match `appendix_mysql_catalog_columns.md`.
- [ ] SHOW outputs match MySQL client expectations (column names/types).
- [ ] All items in `docs/findings/mysql_wire_protocol_gaps.md` closed or explicitly deferred.

## Testing Requirements
- Unit tests:
  - Extend `tests/unit/test_mysql_parser.cpp` for DDL, constraints, `<=>`, placeholders, LIKE ESCAPE.
  - Add guardrail tests rejecting ScratchBird-only features.
- Integration tests:
  - Native MySQL client connections (mysql CLI, JDBC) against ScratchBird.
  - `SHOW`/`DESCRIBE` outputs compared to MySQL 8.0.
  - information_schema queries on core tables (TABLES, COLUMNS, STATISTICS, ROUTINES).
- Protocol tests:
  - Auth flows for `mysql_native_password` and `caching_sha2_password`.
  - Prepared statement lifecycle.
  - COM_CHANGE_USER / COM_RESET_CONNECTION.
  - Autocommit ON/OFF state transitions and server_status flags.
  - Explicit `START TRANSACTION` block handling.

## Acceptance Criteria
- Any native MySQL client connects and operates without protocol or catalog errors.
- MySQL SQL syntax supported is 1:1 with MySQL 8.0, no ScratchBird-only features.
- System catalog queries return correct columns and reasonable values.

## Implementation Notes (Concrete)
- Use `appendix_mysql_catalog_columns.md` to create exact column lists for every catalog table.
- For columns without ScratchBird equivalents, populate `NULL` or MySQL default values but **never omit** columns.
- Add explicit comments in code where deterministic hash IDs are used, and point to collision table usage.
- Do not implement replication commands; return explicit error codes for binlog/GTID requests.
