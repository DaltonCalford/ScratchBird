# Plan 02B - Schema/Database DDL Infrastructure

Version: 1.1
Date: 2025-12-31
Status: In progress (core implementation complete; adapter/compiler alignment complete; cascade semantics/testing remaining)

Scope: Provide SBLR opcodes, parser emission, executor handlers, and catalog integration for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE. Align emulated database lifecycle with catalog schema paths and view generation.

References:
- `docs/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md`
- `docs/audit/AUDIT_SCHEMA_DATABASE_DDL_GAP.md`
- `docs/planning/PLAN_04_PREREQUISITES.md`
- `docs/planning/plan_13_mysql_emulation_parity.md`
- `docs/planning/plan_14_postgresql_emulation_parity.md`
- `docs/planning/plan_15_firebird_emulation_parity.md`

## Current Implementation (as of 2025-12-31)
Completed:
- SBLR extended opcodes defined for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE.
- V2 semantic analyzer + bytecode generator emit schema/database DDL payloads and flags.
- Executor handlers implemented for CREATE/DROP/ALTER SCHEMA and CREATE/DROP/ALTER DATABASE.
- PostgreSQL parser emits CREATE/DROP/ALTER SCHEMA/DATABASE opcodes.
- MySQL parser emits CREATE/DROP/ALTER DATABASE (SCHEMA synonym supported).
- Firebird parser builds emulated database paths for CREATE/DROP/ALTER DATABASE (RENAME only for ALTER).
- ScratchBird v2 parser supports CREATE/DROP/ALTER SCHEMA/DATABASE.
- Emulated database lifecycle wired: createSchemaPath + createEmulationType/server/db record; dropEmulatedDatabase.
- Emulation view generator updated and integrated into CREATE/DROP DATABASE for non-native protocols.
- MySQL/PostgreSQL parsers normalize slash/dot paths to canonical dot paths.

Remaining:
- DROP SCHEMA/DATABASE cascade behavior (CatalogManager dropSchema is RESTRICT-only; cascade flag ignored).
- CREATE SCHEMA owner assignment is read but not persisted (catalog API gap).
- ALTER SCHEMA SET PATH is not supported.
- Emulated adapter guardrails (PARENT/CURRENT/ABSOLUTE tokens must not leak to remote clients).
- Dedicated unit/integration tests for schema/database DDL and emulated view generation.

## Decisions (Resolved)
1) Canonical emulation schema path:
   - `remote.emulated.<dialect>.<server>.<db>` is canonical (dot-path).
   - Slash paths (`/remote/emulated/...`) are normalized to dot-path at parser boundaries.
2) CREATE DATABASE semantics:
   - Creates schema path under `remote.emulated.<dialect>.<server>.<db>`.
   - Creates/ensures emulation type/server records.
   - Creates emulated database record and generates protocol-specific views.
3) DROP DATABASE semantics:
   - Drops emulation views (non-native protocols), then drops schema (native).
   - Drops emulated database record.
4) Emulated databases are virtual:
   - No dedicated database files; they are branches on the schema tree within an existing ScratchBird database.

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
Status: Completed
- Extended opcodes added in `include/scratchbird/sblr/opcodes.h`.
- Payloads emitted by v2 bytecode generator (flags + schema/database path + optional owner/rename).

### 2) Executor Handlers
Status: Completed (cascade semantics pending)
- Implemented handlers in `src/sblr/executor.cpp` for schema/database opcodes.
- IF EXISTS / IF NOT EXISTS enforced.
- Emulated database records created/dropped.
- Cascade flag is passed but CatalogManager dropSchema remains RESTRICT-only.

### 3) Parser Implementations
Status: Completed (per-dialect scope)
- PostgreSQL: CREATE/DROP/ALTER SCHEMA and DATABASE emit extended opcodes.
- MySQL: CREATE/DROP/ALTER DATABASE implemented; SCHEMA synonym supported; USE path normalized.
- Firebird: CREATE/DROP/ALTER DATABASE parses to emulated database paths; ALTER supports RENAME only.
- ScratchBird v2: CREATE/DROP/ALTER SCHEMA/DATABASE supported.

### 4) Catalog Integration
Status: Completed (cascade pending)
- Schema paths created via `createSchemaPath` with REMOTE_EMULATED schema type.
- Emulated database records created/dropped via CatalogManager.
- DROP DATABASE prunes views and drops emulated database record.
- DROP SCHEMA is still RESTRICT-only (cascade pending).

### 5) Emulation View Generation
Status: Completed (core integration)
- `EmulationViewGenerator` updated to current CatalogManager APIs.
- CREATE/DROP DATABASE invokes centralized view generation for emulated protocols.

### 6) Adapter and Compiler Alignment
Status: Partial (path alignment complete; guardrails pending)
- Parsers normalize slash/dot paths to canonical dot paths.
- Adapters/query compilers aligned to dot-path defaults.
- Ensure adapters do not expose PARENT/CURRENT/ABSOLUTE tokens to remote clients.

### 7) Testing
Status: In progress
- Add unit tests for opcode emission and executor handling.
- Add parser tests for CREATE/DROP/ALTER SCHEMA/DATABASE.
- Add integration tests for emulated DB create/drop across PostgreSQL/MySQL/Firebird adapters.

## Acceptance Criteria
- Emulated database creation and drop work end-to-end for PostgreSQL, MySQL, Firebird.
- Schema and database DDL opcodes are defined, emitted, and executed with correct semantics.
- Emulation schema paths are consistent across catalog, adapters, parsers, and plans.
- View generation works via the centralized generator.
- Cascade semantics and adapter/query compiler path alignment are validated.
