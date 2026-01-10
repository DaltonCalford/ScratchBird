# Session Summary (ScratchBird Alpha Prep)

## Context and Goals
- ScratchBird is a new database engine based on Firebird-style MGA (no WAL). Users are always in a transaction.
- Alpha goal: full local/standalone functionality with no design dead-ends for eventual cluster support.
- Emulated engines (Firebird/MySQL/PostgreSQL) must be 1:1 compatible with their native clients and features. No ScratchBird-only features in emulated parsers or protocols.

## Key Design Decisions
- **Always-in-transaction**: every attachment always has an active transaction. COMMIT/ROLLBACK immediately starts a new default transaction.
- **Start transaction conflict action**: if a new transaction is started while one is active, apply explicit action or default (rollback/commit/reject).
- **Autocommit**: supported as a mode; each statement becomes its own transaction when enabled.
- **Attachments**: one connection can host multiple attachments; transactions are strictly attachment-scoped and cannot be shared.
- **Native protocol header**: attachment_id and txn_id are mandatory non-zero fields after AUTH_OK; no backward compatibility is required.
- **UUID v7**: used for object identity and conflict ordering (earlier timestamp wins ties; fallback to UUID bytes).

## Catalog Naming and Ownership (Canonical Names)
- **Canonical names only**: no physical tables/views with `sb_` prefix. `sb_` is documentation-only.
- **Domains**: authoritative tables in `sys.cluster.configuration.*`; user-facing views in `sys.catalog.*`.
- **Security**: authoritative cluster tables in `sys.cluster.security.*`; node-local session/audit in `sys.security.*`.
- **Runtime**: monitoring views in `sys.runtime.*`.
- **Emulation**: mapping tables in `sys.emulation.*`.
- **Core catalog**: tables in `sys.catalog.*` (e.g., `sys.catalog.tables`, `sys.catalog.constraints`, `sys.catalog.object_resolver`, `sys.catalog.index_versions`).

## Emulated Catalogs (Per-DB Schemas)
- Emulated databases live under `remote.emulated.<dialect>.<server>.<db>`.
- Emulated catalog schemas (e.g., `information_schema`, `pg_catalog`, `rdb$`, `mon$`) are **views/synonyms** that point to system catalogs (`sys.catalog.*`, `sys.cluster.configuration.*`, `sys.security.*`, `sys.runtime.*`).
- Emulated catalogs must **appear as tables** to native clients even if implemented as views.

## Domains (Cluster-Wide)
- Domains are cluster-wide, not schema-scoped.
- Conflict resolution uses deterministic UUID v7 ordering; non-canonical domains become SHADOW with no new dependencies.
- ALTER DOMAIN validates all dependent rows; failures report table_id + primary keys. Cluster validation uses PENDING state until all nodes report.
- Casts allowed only if storage matches or explicit cast exists.

## Security Architecture
- Role switching is transaction-bound; must commit/rollback (or default action) before changing.
- AuthKey/session binding is immutable for an active transaction.
- Security cache requires quorum checks; local cache can be disabled or require remote validation.
- Configurable security posture (open -> high security), including external auth mapping.

## Installation + Data Loaders
- `sb_install` is for installation/bootstrap; ongoing timezone/charset updates use standalone tools.
- Standalone tools: `sb_timezone_loader`, `sb_charset_loader`.
- Data sources: IANA TZDB (timezone), Unicode CLDR/ICU (charset/collation).

## Updated Plans / Specs (Highlights)
- `docs/archive/2026-01-04/planning/plan_16_attachment_transaction_model.md`: attachment model, runtime views, transaction defaults.
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`: header includes attachment_id/txn_id; non-zero required.
- `docs/specifications/Appendix_A_SBLR_BYTECODE.md` + `docs/archive/2026-01-04/planning/plan_03_sblr_version2_extended_opcodes.md`: SBLR v2 + extended opcodes.
- `docs/archive/2026-01-04/planning/plan_10_cluster_domains_and_conflict_resolution.md`: domain DDL, conflict algorithm, canonical names.
- `docs/archive/2026-01-09/planning/plan_06_metadata_show_and_catalog.md`: canonical catalog DDL, SHOW mapping, emulated view rules.
- `docs/archive/2026-01-04/planning/plan_03_security_context_auth_audit_quorum.md`: security tables + quorum/cache rules.
- `docs/archive/2026-01-04/planning/plan_13/14/15_*_emulation_parity.md`: emulated engine parity with per-DB view schemas.
- `docs/archive/2026-01-04/planning/Beta_Phase_0_Implementation_Plan.md`: expanded documentation requirements + `sb_install` spec.
- `docs/archive/2026-01-04/planning/alpha_exit_checklist_matrix.md`: alpha exit criteria checklist.

## Current Workspace State
- Many uncommitted changes across planning/spec files and some code/tests. Do not commit without coordination (other AI is implementing Plan 01).
- The old planning directory cleanup has already happened; new plans live in `docs/archive/2026-01-09/planning/`.
- Added dormant transaction catalog scaffolding (CatalogRootPage pointer, `DormantTransactionRecord`, CatalogManager CRUD + TOAST-backed text fields). General catalog TOAST manager still pending.
- Wired statement tracking in ServerSession/ProtocolAdapter and added Database-level dormant detach/reattach registry (keeps ProcArray/locks alive). Server restart purges dormant records from prior instances; GC/lease enforcement still pending.
- CatalogManager now persists sequences, views, triggers, procedures/functions (including parameter records), synonyms, and foreign tables and reloads them on startup; rename/move updates are persisted for those types.
- SHOW TRIGGER/PROCEDURE/FUNCTION/VIEW/COMMENTS/DEPENDENCIES/PACKAGE/DOMAIN/GRANTS/CHECKS read from catalog data with body redaction hooks when source text is missing.
- Constraint + FDW/server-registry/UDR engine/module persistence is wired; catalog load now backfills missing table pages via `allocateCatalogPage` and rewrites the root.
- Added persistence/restart tests for constraints + FDW/user mappings + server registry + UDR engines/modules in `tests/unit/test_catalog_persistence_phase_b.cpp`; tests now build and pass.
- Added a global test environment that defaults `SCRATCHBIRD_LONG_TRANSACTIONS_ENABLED=0` to suppress long-transaction monitor checks during unit tests unless explicitly enabled.
- ScratchBird parser v2 now parses WAIT/NO WAIT, LOCK TIMEOUT, RESERVING, AUTOCOMMIT, and ON CONFLICT for START/SET TRANSACTION; bytecode emission now includes full v2 transaction payload and SET AUTOCOMMIT.
- ScratchBird parser v2 parses PREPARE/COMMIT/ROLLBACK PREPARED and emits 2PC opcodes; PostgreSQL parser emits PREPARE/COMMIT/ROLLBACK PREPARED; Firebird parser now implements SET TRANSACTION payload options including READ COMMITTED variants; added v2 parser/bytecode tests for transaction payload flags + 2PC opcodes.
- MySQL parser now supports SET TRANSACTION isolation/access modes and has test coverage for common transaction variants.
- Added executor tests for READ COMMITTED READ CONSISTENCY and NO RECORD VERSION payload handling in `tests/unit/test_executor_transaction_payload.cpp`.
- Table reservations now resolve to UUIDs via catalog resolver (search path aware) before locking; lock acquisition uses UUID identity rather than `[sys]` name lookup.
- Prepared transaction persistence added (catalog table + root pointer); TransactionManager now supports PREPARE/COMMIT/ROLLBACK PREPARED with CLOG/TIP PREPARED state tracking and OAT pinning.
- Executor now handles PREPARE/COMMIT/ROLLBACK PREPARED opcodes and autocommit transition tests were added alongside 2PC executor tests in `tests/unit/test_executor_transaction_payload.cpp`.
- Fixed Firebird lexer/parser keyword mismatch (`KW_RENAME` added as non-reserved), restored `ObjectType::UNKNOWN` sentinel, and resolved a constraint lookup deadlock in `getConstraintByName`.
- Redaction policy details are deferred to the security/visibility workstream; current SHOW paths only guarantee missing-body redaction where implemented.

## Next Steps for New Session
- Keep all catalog references canonical (no `sb_` prefix in physical names).
- Ensure emulated catalogs are views under `remote.emulated.<dialect>.<server>.<db>`.
- Preserve always-in-transaction semantics in all adapters and tooling.
- Avoid new commits until the other AI finishes its work unless explicitly directed.
