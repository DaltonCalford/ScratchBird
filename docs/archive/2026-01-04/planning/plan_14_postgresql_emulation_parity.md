# Plan 14 - PostgreSQL Emulation Parity (Parser + Protocol + Catalog)

## Scope
Deliver 1:1 PostgreSQL 16 client compatibility over the PostgreSQL wire protocol, including SQL parser coverage, protocol flows (simple + extended), pg_catalog + information_schema catalogs, and strict dialect guardrails. Prevent non-PostgreSQL commands (e.g., MySQL SHOW) from being accepted.

## Priority
P0 (Alpha requirement).

## References
- `docs/specifications/postgresql_spec.md`
- `docs/specifications/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION.md`
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
- `docs/findings/postgresql_emulation_parity_audit.md`
- `docs/findings/postgresql_wire_protocol_gaps.md`
- `docs/planning/appendix_postgresql_catalog_columns.md`
- `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`

## Decisions / Constraints (Resolved)
- Emulated databases live under schema path `remote.emulated.postgresql.<server>.<db>` (full path `/remote/emulated/postgresql/<server>/<db>`).
  - Canonical path resolved in Plan 02B; parsers normalize slash/dot paths. Adapter/query compiler defaults still need alignment in some areas.
- On emulated DB creation/connection, create pg_catalog and information_schema views scoped to that DB only.
- Views must **appear as tables** where PostgreSQL expects tables (pg_class.relkind etc must reflect base tables).
- Auth methods required in Alpha: MD5 and SCRAM-SHA-256.
- PostgreSQL host restrictions are controlled by a server-side policy (pg_hba-like), not by role identity.
- Reject non-native SHOW commands (`SHOW TABLES`, `SHOW DATABASES`, etc). Only PostgreSQL `SHOW <GUC>` is accepted.
- Deterministic hash ID mapping for PostgreSQL OIDs with collision table.
- No emulated replication in Alpha; reject logical/streaming replication commands.
- Transaction semantics must match PostgreSQL:
  - Implicit transaction per statement unless in an explicit transaction block.
  - SQLSTATE `25P01` for "no active transaction" and `25P02` for "in failed transaction".

## Order of Implementation
1) Schema mapping + emulated catalog bootstrap (server/db schema + view generation + filter scoping).
2) PostgreSQL parser completion + dialect guardrails.
3) Protocol adapter parity (auth, extended query, COPY, CancelRequest, transaction state).
4) Catalog coverage (pg_catalog + information_schema) using appendix column lists.
5) Search_path + schema resolution behavior (pg_catalog/public).
6) FDW/UDR passthrough completion for PostgreSQL legacy migration.
7) Tests (parser, protocol, catalog, native client compatibility).

## Concrete Code Touchpoints (Exact Files + Functions)
- Parser:
  - `src/parser/postgresql/pg_parser_ddl.cpp`
    - `parseCreateDatabase` (implemented; emits EXT_CREATE_DATABASE)
    - `parseCreateSchema` (implemented; emits EXT_CREATE_SCHEMA)
  - `src/parser/postgresql/pg_parser_expr.cpp`
    - ESCAPE handling TODO in `parseLikeExpr()`
    - Array subscript TODO in `parsePostfixExpr()`
  - `src/parser/postgresql/pg_parser_misc.cpp`
    - Accepts non-native SHOW commands (must reject)
  - `src/parser/postgresql/pg_parser.cpp`
    - `parseQualifiedName` (restrict to schema.table / schema.table.column)
- Adapter:
  - `src/protocol/adapters/postgresql_adapter.cpp`
    - `ensurePostgresSystemCatalog` (placeholder views)
    - CancelRequest TODO
    - Auth validation TODO
    - COPY IN handling TODO
    - MD5 hash placeholder
- Catalog/Views:
  - `include/scratchbird/catalog/pg_catalog.h` (stub)
  - `include/scratchbird/catalog/information_schema.h` (stub)
  - `include/scratchbird/catalog/emulation_view_generator.h` (expand PostgreSQL views)
  - `src/catalog/virtual_catalog.cpp`
- Mapping:
  - `src/core/tid_resolver.*` (OID mapping table)
- FDW/UDR:
  - `src/fdw/postgresql_adapter.cpp` (TODOs for error parsing and type OIDs)
  - `src/fdw/protocol_adapter.cpp` (factory TODOs)
- Tests:
  - `tests/unit/test_postgresql_parser.cpp`
  - `tests/unit/test_protocol_adapter_dialects.cpp`
  - `tests/integration/test_client_server_integration.cpp`

## Required Data/Schema Changes
- Add PostgreSQL OID mapping to `sys.emulation.emulated_id_map` (see Plan 13 DDL).
- Create per-DB catalog view schemas:
  - `remote.emulated.postgresql.<server>.<db>.pg_catalog`
  - `remote.emulated.postgresql.<server>.<db>.information_schema`
  - `remote.emulated.postgresql.<server>.<db>.public` (default user schema)
  - These are views/synonyms that point to `sys.catalog.*`, `sys.cluster.configuration.*`, `sys.security.*`, and `sys.runtime.*` (no physical tables under the emulated schema).

## Implementation Tasks (Detailed)

### 1) Schema Mapping and Catalog Bootstrap
- Add `server_name` to `ProtocolAdapterConfig` and set it from listener config.
- Update `PostgresqlAdapter::ensurePostgresSystemCatalog`:
  - Use schema path `remote.emulated.postgresql.<server>.<db>`.
  - Create child schemas `pg_catalog`, `information_schema`, `public` (if missing).
  - Replace placeholder `WHERE 1=0` views with real view definitions.
- Adapter rewrite for schema-qualified references:
  - Rewrite `pg_catalog.` and `information_schema.` to the fully qualified emulated schema path.
  - Example: `pg_catalog.pg_class` -> `"remote.emulated.postgresql.<server>.<db>.pg_catalog"."pg_class"`.
- Enforce default search_path:
  - `pg_catalog`, `public` (in that order) for each new session.

### 2) Parser Coverage and Guardrails
- Fix DDL stubs:
  - `CREATE DATABASE` must create a new emulated DB schema under `remote.emulated.postgresql.<server>.<db>`.
  - `CREATE SCHEMA` must create a schema under the emulated DB root.
- Expression gaps:
  - Implement LIKE/ILIKE/SIMILAR ESCAPE semantics.
  - Implement array subscripts (emit opcode for subscript access).
- Dialect guardrails:
  - Reject MySQL-style SHOW commands (`SHOW TABLES`, `SHOW DATABASES`, etc).
  - Reject ScratchBird schema path tokens (PARENT/CURRENT/ABSOLUTE).
  - Restrict qualified names to `schema.table` and `schema.table.column`.

### 3) Protocol Adapter Parity
- Authentication:
  - Implement MD5 auth with proper `md5(md5(password+user)+salt)`.
  - Implement SCRAM-SHA-256 using `scram_auth` module.
- CancelRequest:
  - Store backend_pid + secret key and honor CancelRequest to terminate running queries.
- COPY protocol:
  - Implement COPY IN/OUT/BOTH framing and error handling.
- ParameterStatus:
  - Send required parameters (`server_version`, `client_encoding`, `DateStyle`, `TimeZone`, `integer_datetimes`, `standard_conforming_strings`).
- Transaction state:
  - Track ReadyForQuery transaction status (`I`, `T`, `E`).
  - Implicit transaction per statement when not inside explicit block.
  - Return SQLSTATE `25P01` and `25P02` exactly as PostgreSQL does.

### 4) Catalog Coverage (pg_catalog + information_schema)
- Implement all `pg_catalog` base tables and views from `appendix_postgresql_catalog_columns.md`.
- Implement all `information_schema` views from `appendix_postgresql_catalog_columns.md`.
- Map OIDs using deterministic hash + collision table.
- Ensure `pg_class.relkind`, `pg_type.typcategory`, `pg_attribute.attnum` etc match PostgreSQL expectations.
- `pg_database` should list all emulated DBs for the server.

### 5) SHOW and GUC Behavior
- Only accept `SHOW <GUC>` and `SET <GUC>` per PostgreSQL semantics.
- Ensure `SHOW` results match PostgreSQL column layout.
- Reject `SHOW TABLES/DATABASES/COLUMNS/INDEXES` with clear PostgreSQL error.

### 6) FDW/UDR Passthrough
- Complete PostgreSQL FDW adapter:
  - Parse ErrorResponse fields.
  - Use proper type OIDs in parameter binding.
- Implement `ProtocolAdapterFactory` instantiation for PostgreSQL.

## Completion Checklist (Developer)
- [ ] DDL parser stubs removed; CREATE DATABASE/SCHEMA emits correct opcodes.
- [ ] MySQL-style SHOW rejected; ScratchBird-only features rejected.
- [ ] Postgres protocol supports auth, cancel, COPY, extended query.
- [ ] Transaction state tracking matches PostgreSQL ReadyForQuery semantics.
- [ ] pg_catalog and information_schema views match column lists.
- [ ] Search_path semantics match Postgres default (pg_catalog, public).
- [ ] FDW passthrough works to legacy PostgreSQL.

## Completion Checklist (Auditor)
- [ ] Parser accepts only PostgreSQL dialect features; rejects non-native syntax.
- [ ] Protocol traces match PostgreSQL 16 expectations.
- [ ] SQLSTATE errors for invalid transaction state match PostgreSQL.
- [ ] Catalog tables/views match `appendix_postgresql_catalog_columns.md`.
- [ ] All items in `docs/findings/postgresql_wire_protocol_gaps.md` closed or deferred with explicit note.

## Testing Requirements
- Unit tests:
  - Extend `tests/unit/test_postgresql_parser.cpp` for CREATE DATABASE/SCHEMA, ESCAPE, array subscripts, guardrails.
- Integration tests:
  - Native `psql` connections to ScratchBird.
  - Validate `SELECT * FROM pg_catalog.pg_class` and `information_schema.tables` outputs.
  - COPY IN/OUT test with large payload.
- Protocol tests:
  - CancelRequest behavior.
  - MD5 and SCRAM authentication.
  - ReadyForQuery transaction status and error codes.

## Acceptance Criteria
- Native PostgreSQL clients connect and operate without protocol or catalog errors.
- SQL syntax supported is 1:1 with PostgreSQL 16 (no MySQL/ScratchBird bleed).
- pg_catalog/information_schema queries return correct columns and data types.

## Implementation Notes (Concrete)
- Use `appendix_postgresql_catalog_columns.md` as the authoritative column list for all pg_catalog and information_schema objects.
- If a column has no ScratchBird equivalent, return NULL but keep column ordering.
- Add code comments for deterministic OID mapping and collision handling.
- Replication commands (logical/streaming) must fail explicitly in Alpha.
