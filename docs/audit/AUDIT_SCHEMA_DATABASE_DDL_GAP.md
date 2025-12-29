# Audit: Schema/Database DDL Gap - Plan/Code Alignment

Date: 2025-12-28
Scope: Align plans with current code for schema/database DDL, emulation schema paths, and related parser/executor/catalog gaps.

## Summary
- Core catalog primitives for schemas and emulated databases exist, but there is no SBLR opcode/executor path to reach them from SQL.
- Emulation schema path conventions are inconsistent across plans, catalog defaults, adapters, and parsers (dot vs slash paths, emulation vs remote.emulated).
- Emulation view generation is partially implemented in adapters; the shared generator header is out of sync with CatalogManager.

## Schema Resolution Mechanics (authoritative)
- Default schema is always set from user/role/group; if missing, treat root as current schema.
- Unqualified object names resolve using current schema, then search_path.
- Leading dot `.tablename` forces current schema only (no search_path fallback); error if not found.
- Relative paths like `.dev.myproj.tablename` are resolved by prefixing the full current schema path.
- Paths without a leading dot are absolute schema paths.
- Path tokens PARENT/CURRENT/ABSOLUTE remain valid in ScratchBird parser.
  - Emulated parsers should accept these tokens for internal rewrites.
  - Remote clients must not be exposed to these tokens.
- Internally the engine resolves by UUID; names are user-facing only.

## Current Implementation (confirmed)
- Catalog schema CRUD exists: `CatalogManager::createSchema`, `CatalogManager::createSchemaPath`, `CatalogManager::dropSchema` in `src/core/catalog_manager.cpp`.
  - `dropSchema` ignores cascade and enforces RESTRICT-only behavior.
- Emulation catalog records exist: `createEmulationType`, `createEmulationServer`, `createEmulatedDatabase`, `dropEmulatedDatabase` in `src/core/catalog_manager.cpp` and `include/scratchbird/core/catalog_manager.h`.
  - `dropEmulatedDatabase` only invalidates the record; it does not prune a schema tree or views.
- Default schema hierarchy is created at init and includes `emulation.mysql`, `emulation.postgres`, `emulation.firebird` under `root` in `src/core/catalog_manager.cpp`.
- Adapters create emulation schemas directly:
  - PostgreSQL: `ensurePostgresSystemCatalog` in `src/protocol/adapters/postgresql_adapter.cpp`.
  - Firebird: `ensureFirebirdSystemTables` in `src/protocol/adapters/firebird_adapter.cpp`.
  - MySQL: `bootstrapInformationSchema` in `src/protocol/adapters/mysql_adapter.cpp` issues SQL `CREATE SCHEMA/TABLE` statements.

## Gaps / Blockers (confirmed)
- Missing SBLR opcodes for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE.
  - No opcode definitions in `include/scratchbird/sblr/opcodes.h`.
  - No executor handlers in `src/sblr/executor.cpp`.
- Parsers are missing or incorrect for schema/database DDL:
  - PostgreSQL: placeholder opcode for CREATE DATABASE and no opcode for CREATE SCHEMA in `src/parser/postgresql/pg_parser_ddl.cpp`.
  - MySQL: CREATE DATABASE stub in `src/parser/mysql/mysql_parser.cpp`.
  - Firebird: CREATE/DROP DATABASE not implemented in `src/parser/firebird/firebird_parser.cpp`.
  - ScratchBird v2 parser does not expose CREATE SCHEMA/CREATE DATABASE in `src/parser/parser_v2.cpp`.
- Emulation view generator is out of sync with current CatalogManager APIs:
  - `include/scratchbird/catalog/emulation_view_generator.h` references `getSchemaByName` and `createSchema(SchemaInfo)` which are not present.
- Adapter schema creation uses dotted schema names with `createSchema`, but name resolution uses hierarchical path splitting.
  - `CatalogManager::getSchema(const std::string&)` splits paths, so `remote.emulated.postgresql.<db>` created as a single name is not discoverable by name.
- Multiple path formats coexist:
  - Dot paths: `remote.emulated.mysql.<server>.<db>` in plans/adapters.
  - Slash paths: `/remote/emulated/mysql/localhost/` in MySQL parser and query compiler.
  - Default catalog tree uses `emulation.*` rather than `remote.emulated.*`.
  - Resolver semantics for leading-dot and relative paths need to be verified and aligned with the authoritative rules above.

## Plan Conflicts / Drift
- Plan 02B is referenced but does not exist as a planning document; its tasks are partially implemented in catalog code.
- Plan 04 prerequisites and status assume no catalog support exists; this is no longer accurate.
- Plans 13/14/15 assert `remote.emulated.<dialect>.<server>.<db>` as a resolved decision, but catalog initialization uses `emulation.*` and parsers use slash paths.
- EmulationViewGenerator is referenced in plans but is not usable with current APIs.

## Outstanding Work (minimum to close the gap)
1) Define schema/database DDL opcodes and payloads, wire into executor dispatch.
2) Implement parser emission for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE for PostgreSQL, MySQL, Firebird (and ScratchBird v2 if required for native SQL).
3) Decide and normalize emulation schema path format (dot vs slash, emulation vs remote.emulated), then update:
   - Catalog defaults and lookup behavior.
   - Adapters (PostgreSQL/MySQL/Firebird).
   - Query compilers and parser defaults (`parseUseStmt`, default_schema_).
4) Update catalog behavior for emulated DB lifecycle:
   - Use `createSchemaPath` for hierarchical paths.
   - Implement cascade-aware drop (or explicit restrict semantics + error surface) for schema/database drops.
   - Connect emulated database records to schema creation/deletion.
5) Fix or replace `EmulationViewGenerator` to use current CatalogManager APIs.
6) Add tests covering opcode emission, executor handling, and emulated schema creation/drop flows.

## Plan Adjustments Recommended
- Create/refresh Plan 02B as the single source of truth for schema/database DDL work; mark existing catalog pieces as complete and list remaining tasks.
- Update Plan 04 prerequisites/status/checklist to reflect partial catalog implementation and to add the path normalization decision as a blocker.
- Update Plans 13/14/15 to downgrade the emulation path decision to "pending until Plan 02B resolves path normalization".
- Update Plan 05 dependency notes to point to the Plan 02B document (once created).

## Files Referenced
- `docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`
- `include/scratchbird/sblr/opcodes.h`
- `src/sblr/executor.cpp`
- `src/parser/postgresql/pg_parser_ddl.cpp`
- `src/parser/mysql/mysql_parser.cpp`
- `src/parser/firebird/firebird_parser.cpp`
- `src/parser/parser_v2.cpp`
- `src/core/catalog_manager.cpp`
- `include/scratchbird/core/catalog_manager.h`
- `include/scratchbird/catalog/emulation_view_generator.h`
- `src/protocol/adapters/postgresql_adapter.cpp`
- `src/protocol/adapters/mysql_adapter.cpp`
- `src/protocol/adapters/firebird_adapter.cpp`
