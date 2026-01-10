# PostgreSQL Emulation Parity Audit

Date: 2025-12-20

This audit focuses on PostgreSQL protocol parity with native PostgreSQL clients,
including parser coverage, wire protocol behavior, and metadata/catalog API surfaces.

## Scope
- Parser: `src/parser/postgresql/` -> SBLR bytecode
- Wire protocol adapter: `src/protocol/adapters/postgresql_adapter.cpp`
- Catalog/metadata: `include/scratchbird/catalog/pg_catalog.h`,
  `include/scratchbird/catalog/information_schema.h`,
  `include/scratchbird/catalog/emulation_view_generator.h`
- Executor output used by SHOW opcodes: `src/sblr/executor.cpp`

## Reference Specs
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `docs/specifications/postgresql_spec.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
- `docs/specifications/WIRE_PROTOCOL_SPECIFICATIONS.md`
- `docs/archive/2026-01-09/findings/postgresql_wire_protocol_gaps.md` (existing gap list)

## Parser Gaps (Missing or Stubbed Features)

### CREATE DATABASE is a placeholder
- `parseCreateDatabase` emits `EXT_SHOW_DATABASE` as a placeholder, which does
  not create a database/schema and is not valid PostgreSQL semantics.
  `src/parser/postgresql/pg_parser_ddl.cpp:943`.

### CREATE SCHEMA emits no opcode
- `parseCreateSchema` parses identifiers but never emits a CREATE SCHEMA opcode,
  so schema creation is effectively a no-op.
  `src/parser/postgresql/pg_parser_ddl.cpp:976`.

### Expression gaps
- LIKE/ILIKE/SIMILAR ESCAPE is parsed but ignored for LIKE, so escape behavior
  is incorrect. `src/parser/postgresql/pg_parser_expr.cpp:202`.
- Array subscript uses `EXT_ARRAY_LENGTH` as a placeholder; actual subscript
  evaluation is missing. `src/parser/postgresql/pg_parser_expr.cpp:353`.

### Qualified name parsing allows feature bleed
- `parseQualifiedName` accepts unlimited dotted segments, which allows
  ScratchBird-style multi-level schema paths not supported by PostgreSQL.
  `src/parser/postgresql/pg_parser.cpp:391`.

### Non-native SHOW commands accepted
- Parser accepts `SHOW TABLES`, `SHOW DATABASES`, `SHOW COLUMNS`, `SHOW INDEXES`
  which are MySQL-style and not valid in PostgreSQL SQL.
  `src/parser/postgresql/pg_parser_misc.cpp:174`.
  These should be rejected for strict parity.

## Wire Protocol and Session API Gaps
- Cancel request handling is TODO (no actual cancellation).
  `src/protocol/adapters/postgresql_adapter.cpp:426`.
- Password validation is TODO for both MD5 and cleartext auth.
  `src/protocol/adapters/postgresql_adapter.cpp:443`.
- COPY IN protocol is unimplemented (`CopyData`, `CopyDone`, `CopyFail`).
  `src/protocol/adapters/postgresql_adapter.cpp:955`.
- MD5 hashing has a TODO and uses placeholder logic without OpenSSL.
  `src/protocol/adapters/postgresql_adapter.cpp:1952`.

## Catalog and Metadata API Gaps

### pg_catalog handler is a stub
- `PgCatalogHandler` returns empty results and placeholder columns for all
  pg_catalog tables. `include/scratchbird/catalog/pg_catalog.h`.

### information_schema is stubbed
- `InformationSchemaHandler` returns empty results for all standard tables.
  `include/scratchbird/catalog/information_schema.h`.

### Emulation view generator is minimal
- Only `pg_tables` and `pg_views` are defined, no core pg_catalog views.
  `include/scratchbird/catalog/emulation_view_generator.h:520`.

### Adapter creates empty placeholder views
- `ensurePostgresSystemCatalog` creates `pg_database`, `pg_namespace`, etc
  as views with `WHERE 1 = 0`, so they are always empty.
  `src/protocol/adapters/postgresql_adapter.cpp:1086`.
  Native clients depend heavily on these tables for startup and metadata.

## Executor / SHOW Output Mismatch
- SHOW opcodes are handled by ScratchBird executor with ScratchBird column
  layouts and schema scoping (PUBLIC only). If PostgreSQL parser continues
  to accept SHOW TABLES/DATABASES/COLUMNS/INDEXES, results will not match
  PostgreSQL expectations. `src/sblr/executor.cpp:22637`.

## Test Coverage Gaps
- Parser tests exist but do not cover CREATE DATABASE/SCHEMA semantics,
  pg_catalog queries, or wire protocol metadata flows.
  `tests/unit/test_postgresql_parser.cpp`.
- No integration tests for native PostgreSQL clients (psql, JDBC, ODBC)
  querying pg_catalog/information_schema.

## Summary (PostgreSQL Parity Risk)
PostgreSQL emulation currently accepts some non-native commands and lacks
core catalog coverage. The wire protocol is incomplete for cancellation,
COPY, and authentication. pg_catalog/information_schema are stubbed or empty,
so native clients will fail to initialize and introspect. These gaps must be
resolved for 1:1 parity with PostgreSQL clients.
