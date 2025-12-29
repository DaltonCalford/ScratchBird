# Plan 02B - Schema/Database DDL Infrastructure

Version: 1.0
Date: 2025-12-28
Status: Draft (audit-aligned)

Scope: Provide SBLR opcodes, parser emission, executor handlers, and catalog integration for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE. Align emulated database lifecycle with catalog schema paths and view generation.

References:
- `docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`
- `docs/audit/AUDIT_SCHEMA_DATABASE_DDL_GAP.md`
- `docs/planning/PLAN_04_PREREQUISITES.md`
- `docs/planning/plan_13_mysql_emulation_parity.md`
- `docs/planning/plan_14_postgresql_emulation_parity.md`
- `docs/planning/plan_15_firebird_emulation_parity.md`

## Current Implementation (from audit)
- CatalogManager supports schema CRUD and schema paths:
  - `createSchema`, `createSchemaPath`, `dropSchema` (RESTRICT-only).
- Emulation catalog records exist:
  - `createEmulationType`, `createEmulationServer`, `createEmulatedDatabase`, `dropEmulatedDatabase`.
- Adapters create emulated schemas directly; view generation is partially implemented per adapter.
- SBLR opcodes, executor handlers, and parser DDL support are missing or incorrect.
- Emulation view generator header is out of sync with current CatalogManager APIs.

## Decisions Required (before implementation)
1) Canonical emulation schema path:
   - Option A: `emulation.<dialect>.<server>.<db>` (matches catalog root tree).
   - Option B: `remote.emulated.<dialect>.<server>.<db>` (matches current plans/adapters).
   - Resolve dot-path vs slash-path representations used in parsers/query compilers.
2) CREATE DATABASE semantics:
   - Must it also create an emulated database record and views?
   - Required flags (IF NOT EXISTS, owner, encoding, template, server).
3) DROP DATABASE semantics:
   - Cascade behavior for schema tree and views.
   - Record cleanup for emulated database entries.
4) CREATE/DROP SCHEMA cascade rules (RESTRICT vs CASCADE).

## Schema Resolution Mechanics (authoritative)
- Default schema is always set from user/role/group; if missing, treat root as current schema.
- Unqualified object names resolve using current schema, then search_path.
- Leading dot `.tablename` forces current schema only (no search_path fallback); error if not found.
- Relative paths like `.dev.myproj.tablename` are resolved by prefixing the full current schema path
  (e.g., `users.username` + `.dev.myproj.tablename` => `users.username.dev.myproj.tablename`).
- Paths without a leading dot are absolute schema paths (e.g., `users.username.dev.tablename`).
- Path tokens PARENT/CURRENT/ABSOLUTE remain valid in ScratchBird parser.
  - Emulated parsers should accept these tokens for internal rewrites.
  - Remote clients must not be exposed to these tokens.
- Internally the engine resolves by UUID; names are user-facing only.

## Work Items

### 1) Opcode Definitions and Payloads
- Add extended opcodes in `include/scratchbird/sblr/opcodes.h`:
  - EXT_CREATE_SCHEMA, EXT_DROP_SCHEMA, EXT_ALTER_SCHEMA
  - EXT_CREATE_DATABASE, EXT_DROP_DATABASE, EXT_ALTER_DATABASE
- Define payload structures for flags, names/IDs, owner, and cascade options.

### 2) Executor Handlers
- Implement handlers in `src/sblr/executor.cpp` for each opcode:
  - Map to CatalogManager schema CRUD and emulated database CRUD.
  - Enforce IF EXISTS / IF NOT EXISTS semantics.
  - Apply cascade vs restrict behavior for drop operations.
  - Update connection context schema/search_path as needed.
  - Align schema resolution with leading-dot and relative path rules where applicable.

### 3) Parser Implementations
- PostgreSQL (`src/parser/postgresql/pg_parser_ddl.cpp`):
  - Replace placeholder CREATE DATABASE opcode; emit full payload.
  - Emit opcode + payload for CREATE SCHEMA.
  - Add DROP/ALTER DATABASE and DROP/ALTER SCHEMA parsing.
- MySQL (`src/parser/mysql/mysql_parser.cpp`):
  - Implement CREATE/DROP DATABASE and CREATE/DROP SCHEMA synonyms.
  - Implement ALTER DATABASE if needed for dialect parity.
  - Normalize `parseUseStmt` to the chosen schema path format.
- Firebird (`src/parser/firebird/firebird_parser.cpp`):
  - Implement CREATE/DROP DATABASE in dialect-appropriate syntax.
- ScratchBird v2 (`src/parser/parser_v2.cpp`):
  - Add CREATE/DROP SCHEMA/DATABASE if native SQL requires them.

### 4) Catalog Integration
- Align schema path creation:
  - Use `createSchemaPath` for hierarchical emulation paths.
  - Ensure `getSchema` by name works for emulated schemas.
- Drop behavior:
  - Implement cascade semantics or explicitly enforce restrict behavior with consistent errors.
  - On DROP DATABASE, prune schema tree and clean emulated database records.
- Connect emulated database records to schema creation/deletion.

### 5) Emulation View Generation
- Update or replace `EmulationViewGenerator` to use current CatalogManager APIs.
- Decide whether view generation is centralized (generator) or per-adapter; make it consistent.

### 6) Adapter and Compiler Alignment
- Update adapters and query compilers to use the canonical path format.
- Remove slash-path defaults in MySQL parser/compiler if dot paths are canonical.
- Ensure search_path updates and schema resolution match the chosen path format.
- Ensure adapters do not expose PARENT/CURRENT/ABSOLUTE tokens to remote clients.

### 7) Testing
- Unit tests for opcode emission and executor handling.
- Parser tests for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE.
- Integration tests for emulated DB create/drop across PostgreSQL/MySQL/Firebird adapters.

## Acceptance Criteria
- Emulated database creation and drop work end-to-end for PostgreSQL, MySQL, Firebird.
- Schema and database DDL opcodes are defined, emitted, and executed with correct semantics.
- Emulation schema paths are consistent across catalog, adapters, parsers, and plans.
- View generation works and is not reliant on stale APIs.
