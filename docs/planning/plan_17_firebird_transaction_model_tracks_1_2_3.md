# Plan 17 - Transaction Model Alignment + Object Persistence + Txn Opcodes

## Scope
Address the three requested tracks in a single coordinated plan:
1) Object persistence for all SQL objects (catalog durability).
2) Complete SBLR v2 transaction payloads, autocommit, and 2PC opcodes.
3) Storage/MVCC/GC alignment to the Firebird transaction model (MGA).

## Assumptions / Constraints
- Items 1/2/3 map to the three tracks above; correct if this mapping is off.
- `include/scratchbird/sblr/opcodes.h` is authoritative if specs diverge.
- All objects are identified by UUID internally; names/paths are user-facing only.
- All metadata changes are transactional; older transactions see earlier layouts until commit/rollback.
- Emulated parsers remain dialect-accurate and schema-isolated; no extra behavior beyond the emulated engine.
- Confirmed: 1/2/3 map to (1) object persistence, (2) transaction payload v2 + 2PC/autocommit, (3) storage/MVCC/GC alignment.
- Autocommit semantics: commit after each statement and immediately start a new transaction.
- 2PC storage: dedicated catalog table for now (cluster/replication can revise later).
- Reattach semantics: retain last active DDL/DML statement text for reference; no cursor state.
- Reattach scope: original node only (no cross-node reattach in current scope).

## Firebird Cache Research Summary (Local Docs)
- Page cache sizing is explicit via `MON$PAGE_BUFFERS` (database page cache) and grows with page size. (`docs/specifications/firebird_docs_split/App_E_Monitoring_Tables.md`)
- Monitoring counters indicate **Classic** and **SuperClassic** have no common cache; shared cache exists only in **SuperServer**. (`docs/specifications/firebird_docs_split/App_E_Monitoring_Tables.md`)
- SuperServer can keep database files and caches open after last disconnect via LINGER. (`docs/specifications/FirebirdReferenceDocument.md`)
- Metadata cache is per-attachment and can remain stale for pre-cached routines:
  - Procedures already cached continue using old sequence increment until connections are closed. (`docs/specifications/FirebirdReferenceDocument.md`)
  - Triggers without explicit SQL Security need reload to pick up changes. (`docs/specifications/FirebirdReferenceDocument.md`)
- Commit/rollback retaining preserves row cache and cursor state within the attachment. (`docs/specifications/FirebirdReferenceDocument.md`)

## References
- `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `include/scratchbird/sblr/opcodes.h`
- `docs/specifications/Appendix_A_SBLR_BYTECODE.md`
- `docs/findings/engine_gap_report.md`
- `docs/planning/plan_01_core_storage_gc.md`
- `docs/planning/plan_02_uuid_resolution_and_rename_move.md`
- `docs/planning/plan_03_sblr_version2_extended_opcodes.md`

## Track 1 - Object Persistence (Catalog Durability)

### 1.0 Status (Current)
Completed:
- Synonym + foreign table persistence wired end-to-end (record structs, read/write/delete, cache load on startup, resolver/name lookups).
- Rename/move now updates synonym/foreign table records on disk.
- Constraint persistence wired (record struct, read/write/delete, cache load on startup).
- Foreign server/user mapping/server registry/UDR engine/module persistence wired (record structs, CRUD, cache load on startup).
- Rename now updates foreign servers, server registry entries, UDR engines, and UDR modules on disk.
- Catalog load now backfills missing catalog table pages for older databases and persists the updated root.
- SHOW DOMAIN/GRANTS/CHECKS switched to catalog-backed queries with metadata redaction hooks stubbed (policy still pending).
- Persistence tests for constraints/FDW/server registry/UDR engines/modules compile and pass (see `tests/unit/test_catalog_persistence_phase_b.cpp`).
- SHOW CHECKS parse/bytecode test restored.

Partial / Outstanding:
- Redaction enforcement policy is still a stub (always false); missing-body handling is not enforced in all SHOW paths.
- Restart/rename coverage still missing beyond the new persistence tests.

### 1.1 Inventory + Gap Audit
- Enumerate all catalog caches and root page slots in `CatalogManager`.
- Identify which object types have read/write/delete paths (schemas, tables, columns, indexes) vs missing (views, sequences, triggers, functions, procedures, synonyms, foreign tables, etc.).
- Produce a per-object checklist: record struct, record IO, catalog table page, cache population, rename/move update, drop cleanup, dependency links.

### 1.2 Record Definitions + Disk Layout
- Validate/extend on-disk record structs for:
  - Views (definition OID, materialized flags, refresh time, name_is_delimited).
  - Sequences (current value, min/max, increment, cycle, cache size, name_is_delimited).
  - Triggers (timing/events/row-level, body OID, enabled flag, name_is_delimited).
  - Functions/Procedures (body OID, input/output params, SQL security, name_is_delimited).
  - Synonyms (target object UUID + type, name_is_delimited).
  - Foreign tables/servers/user mappings (definition OIDs / connection data).
  - Database triggers (on connect/disconnect/transaction events).
- Add/verify catalog heap pages for all object tables in the catalog root page.
- Ensure all persistent records include: object UUID, owner UUID, schema UUID, created/modified timestamps, is_valid flag.

### 1.3 Write/Read/Delete Paths
- Implement write/read/delete helpers for each object type using existing heap-page utilities (same pattern as schema/table/index).
- Wire create/alter/drop operations to persist to disk and update caches.
- Ensure rename/move updates record fields on disk (name and/or schema_id).
- Enforce UUID-based permissions storage (object_id + object_type) for GRANT/REVOKE.

### 1.4 Body Storage (SQL/SBLR) + Redaction Policy
- Store SQL or SBLR bodies in TOAST with stable OID references in catalog records.
- If body is intentionally removed, persist metadata and mark body as redacted.
- SHOW output behavior: always return input/output + comments, body as "Redacted" when missing.
- Decision: defer detailed metadata visibility/redaction rules to the security/visibility workstream; until then, only missing-body redaction is guaranteed.

### 1.5 Transactional Metadata Visibility
- Ensure catalog table reads honor transaction snapshot rules (older transactions see prior metadata).
- Avoid global cache pollution: update cache invalidation or versioning to respect MGA visibility.
- Use UUIDs as stable identity across renamed/moved objects.

### 1.6 Restart + Migration
- On startup, load all persisted objects into caches/resolver.
- If a catalog page is missing (older DB), create the page and treat as empty.
- Implemented: `CatalogManager::load` backfills missing catalog tables and rewrites the catalog root.

### 1.7 Tests
- Create object, restart, verify object exists and SHOW works (views, sequences, triggers, functions, procedures, synonyms, foreign tables).
- Rename/move persisted objects and verify resolution across restart.
- Verify permissions resolve by UUID even after rename.
- Verify SHOW redaction behavior for removed bodies.

## Track 2 - SBLR v2 Transaction Payloads + Autocommit + 2PC

### 2.0 Status (Current)
Completed / Implemented:
- ScratchBird parser v2 parses SQL-standard + Firebird legacy transaction options (WAIT/NO WAIT, LOCK TIMEOUT, RESERVING, AUTOCOMMIT) and ON CONFLICT.
- Bytecode generator v2 emits full START/SET TRANSACTION payloads (conflict action/error code, wait/lock/reservations/autocommit flags).
- SET AUTOCOMMIT and SET TRANSACTION AUTOCOMMIT are parsed in ScratchBird v2 and emit EXT_SET_AUTOCOMMIT.
- Executor decodes full flag set (isolation/access/deferrable/wait/lock timeout/reservations/autocommit/conflict action) and applies settings; autocommit commits after non-transaction control statements.
- EXT_SET_AUTOCOMMIT opcode is implemented and used by the MySQL parser.
- ScratchBird parser v2 now parses PREPARE/COMMIT/ROLLBACK PREPARED and emits 2PC opcodes (EXT_PREPARE/COMMIT/ROLLBACK PREPARED).
- Firebird parser implements SET TRANSACTION with Firebird options (READ ONLY/WRITE, READ COMMITTED, SNAPSHOT, WAIT/NO WAIT, LOCK TIMEOUT, RESERVING).
- v2 parser/bytecode tests cover transaction payload flags and 2PC opcodes.
- READ COMMITTED variants (READ CONSISTENCY / RECORD VERSION / NO RECORD VERSION) are parsed and encoded via READ_COMMITTED_MODE payload flag; executor maps READ CONSISTENCY to statement-level snapshots.
- MySQL parser now supports SET TRANSACTION isolation/access modes and emits the payload where the dialect allows it.
- Executor tests cover READ COMMITTED READ CONSISTENCY and NO RECORD VERSION payload handling.
- Table reservations are resolved to UUIDs via catalog resolver (search path/current schema aware) before locking.
- Prepared transactions are persisted (catalog table), tracked in CLOG/TIP as PREPARED, and pinned in OAT.
- Executor implements PREPARE/COMMIT/ROLLBACK PREPARED and integrates TransactionManager support.
- Executor tests cover autocommit transitions and 2PC prepare/commit/rollback flows.

Partial / Outstanding:
- Emulated parsers only map a subset of options (PostgreSQL: isolation/access/deferrable; MySQL: isolation/access via SET TRANSACTION and access via START TRANSACTION).
- 2PC prepared lock retention is not implemented (locks are released on prepare to avoid proc_id reuse conflicts).
- Lock ownership is proc_id-based only; no owner separation for prepared transactions yet.

### 2.1 Payload Contract (Authoritative Source = opcodes.h)
- Document full START/SET TRANSACTION payload mapping from `opcodes.h` flags:
  - HAS_ISOLATION, HAS_ACCESS_MODE, HAS_DEFERRABLE, HAS_WAIT_MODE, HAS_LOCK_TIMEOUT, HAS_RESERVATIONS, HAS_AUTOCOMMIT, HAS_CONFLICT_ERROR_CODE.
- Confirm encoding order and field sizes (U16 flags, conflict action byte, optional fields).

### 2.2 AST/Parser/Resolver Extensions
- Extend resolved AST for START/SET TRANSACTION to include:
  - WAIT/NO WAIT, LOCK TIMEOUT, RESERVING list, AUTOCOMMIT, conflict action, conflict error code.
  - READ COMMITTED variants (READ CONSISTENCY / RECORD VERSION / NO RECORD VERSION).
- Update parser_v2 and ScratchBird grammar to parse full SET/START TRANSACTION syntax.
- Keep emulated parsers dialect-accurate: only accept options supported by the emulated engine.

### 2.3 Bytecode Generation (v2)
- Emit full transaction payload fields based on flags and resolved AST.
- Add EXT_SET_AUTOCOMMIT emission for SET AUTOCOMMIT where appropriate.
- Align with `opcodes.h` ordering and ensure defaults are emitted only when flags set.

### 2.4 Executor + ConnectionContext Semantics
- Implement missing decode paths for all flags in executor transaction handling.
- Integrate lock timeout and table reservations (table lock manager).
- Implement autocommit behavior with "always-in-transaction" model:
  - AUTOCOMMIT ON: commit after each statement, start a new transaction automatically.
  - AUTOCOMMIT OFF: behave as explicit transaction.
- Ensure conflict action semantics are honored for START/SET TRANSACTION when a transaction is already active.

### 2.5 2PC (Prepare/Commit/Rollback Prepared)
- Extend transaction manager to support PREPARED state (durable storage).
- Implement SBLR extended opcodes:
  - EXT_PREPARE_TRANSACTION
  - EXT_COMMIT_PREPARED
  - EXT_ROLLBACK_PREPARED
- Store prepared transaction metadata (global transaction ID, owner, state) in catalog or CLOG extension.
- Enforce error handling for unknown or already-resolved prepared transactions.

### 2.6 Wire Protocol Alignment (Decision Gate)
- Decide whether BEGIN_TRANSACTION payload should carry isolation/access mode.
- If yes, update `TransactionPayload` and server_session handling to apply those settings.
- If no, document that SQL/SBLR is authoritative for transaction options.

### 2.7 Tests
- Unit tests for v2 bytecode generation (flags and field order).
- Executor tests for lock timeout, reservations, autocommit transitions.
- 2PC tests: prepare, commit prepared, rollback prepared, and error paths.

### 2.8 Prepared Lock Ownership Separation (Design + Implementation)
- Introduce lock-owner identity distinct from `proc_id` (e.g., owner type + UUID).
- Persist lock-owner identifier for prepared transactions and use it for lock release on COMMIT/ROLLBACK PREPARED.
- Update deadlock detector to operate on lock-owner identities (not just proc_id).

## Track 3 - Storage/MVCC/GC Alignment to Firebird MGA

### 3.0 Status (Current)
Completed / Implemented:
- Transaction markers (OIT/OAT/OST/NEXT) are tracked in `TransactionManager` and persisted to the DB header; markers update on begin/commit/rollback.
- Sweep manager exists with foreground/background paths; `SWEEP` opcode triggers foreground sweep; OIT advances using transaction state scans.
- Garbage collector exists with cooperative/background policies and config-driven tuning.
- Long transaction monitor exists with LOG/ROLLBACK/TERMINATE policies; started by `Database`; `ConnectionContext` checks termination requests.
- Dormant transaction catalog records, detach/reattach registry, and statement tracking are implemented (see Track 3.2 status).

Partial / Outstanding:
- Sweep trigger uses default interval constant and a simplified transaction-state scan (not TIP page scanning); space reclamation is still stubbed.
- Metadata MVCC and catalog cache invalidation/generation tracking are not implemented.
- READ COMMITTED RECORD_VERSION / NO_RECORD_VERSION behavior is not enforced yet; only READ CONSISTENCY maps to statement-level snapshots.
- MON$ / runtime view coverage for live marker values and long-transaction stats remains incomplete.
- Tests for isolation, sweep, GC reclaim, and long-transaction monitoring are missing or disabled; unit tests now default-disable long transaction monitoring via `SCRATCHBIRD_LONG_TRANSACTIONS_ENABLED=0`.

### 3.1 Cache Architecture Alignment (Firebird)
- Default to SuperServer-style shared page cache (ScratchBird is single-process), but treat metadata caches as per-attachment.
- Implement catalog/metadata cache invalidation via a catalog generation counter bumped on DDL commit.
- Honor transactional visibility: old transactions continue to read older catalog versions even if cache is refreshed for new transactions.
- Require cache refresh on new statement if generation mismatch; allow precompiled routines to keep cached metadata until recompiled or reloaded (Firebird-like behavior).

### 3.2 Dormant Transaction Data Model (Reattach + GC)
**Catalog table (persistent):**
```sql
CREATE TABLE sys.catalog.dormant_transactions (
  dormant_id UUID PRIMARY KEY,         -- reattach token (UUID v7)
  attachment_id UUID NOT NULL,
  proc_id INT NOT NULL,                -- ProcArray slot (required until attachment model is live)
  txn_id BIGINT NOT NULL,              -- MGA transaction ID
  session_id UUID NOT NULL,            -- protocol session UUID (v4 today)
  user_id UUID NOT NULL,
  session_user_id UUID NOT NULL,       -- original authenticated user
  role_id UUID,                        -- nullable/zero UUID if none
  isolation_level SMALLINT NOT NULL,
  access_mode SMALLINT NOT NULL,       -- READ WRITE / READ ONLY
  wait_mode SMALLINT NOT NULL,         -- WAIT / NO WAIT
  lock_timeout INT NOT NULL,
  autocommit_mode SMALLINT NOT NULL,
  current_schema_id UUID NOT NULL,
  session_settings_oid INT NOT NULL,   -- TOAST JSON: search_path, dialect_tag, sql_dialect, charset, parser_version, stmt_timeout
  start_time BIGINT NOT NULL,          -- epoch micros
  last_activity_time BIGINT NOT NULL,  -- epoch micros
  dormant_since BIGINT NOT NULL,       -- epoch micros
  lease_expires_at BIGINT NOT NULL,    -- epoch micros
  last_statement_oid INT NOT NULL,     -- TOAST ref (0 if unavailable)
  last_statement_hash BIGINT NOT NULL, -- 64-bit hash for quick compare
  last_statement_type SMALLINT NOT NULL,   -- DDL/DML/OTHER
  last_statement_status SMALLINT NOT NULL, -- IN_PROGRESS/COMPLETED/FAILED
  last_statement_time BIGINT NOT NULL,
  last_rows_affected BIGINT NOT NULL,
  last_error_code INT NOT NULL,        -- core::Status numeric code (0 = OK)
  last_sqlstate CHAR(5),               -- optional SQLSTATE
  state SMALLINT NOT NULL,             -- DORMANT/REATTACHED/ROLLED_BACK/EXPIRED
  server_instance_id UUID NOT NULL,    -- originating server instance only
  is_valid SMALLINT NOT NULL
);
```
**Indexes:**
- `(txn_id)`, `(attachment_id)`, `(proc_id)`, `(lease_expires_at)`, `(state, lease_expires_at)`.

**Storage + catalog wiring:**
- Add `dormant_transactions_page` to `CatalogRootPage`.
- Implement `DormantTransactionRecord` read/write/delete paths (heap page).
- Add a general catalog TOAST manager (separate from policy TOAST) for `last_statement_oid` and `session_settings_oid`.
- Store `last_statement_oid` in TOAST; if SQL text unavailable, store canonical summary string.

**Implementation status (code):**
- Added `dormant_transactions_page` to the catalog root and a matching `dormant_transactions_table_page_`.
- Implemented `DormantTransactionRecord` + CatalogManager CRUD using heap pages and TOAST OIDs.
- Using the policy TOAST manager for now; general catalog TOAST manager is still pending.
- Runtime wiring: ServerSession/ProtocolAdapter now track last statement status; Database can detach to dormant and reattach via in-memory registry.
- Server restart now purges dormant records from prior instances (no in-memory context to reattach).
- GC scheduling + lease enforcement is still pending.

**Runtime view (monitoring):**
```sql
CREATE VIEW sys.runtime.dormant_transactions AS (
  dormant_id UUID,
  attachment_id UUID,
  txn_id BIGINT,
  user_id UUID,
  isolation_level SMALLINT,
  read_only SMALLINT,
  dormant_since BIGINT,
  lease_expires_at BIGINT,
  state SMALLINT,
  last_statement_text TEXT
);
```

**Behavioral rules:**
- On disconnect with active txn: mark attachment dormant and persist record.
- No cursor state is retained; reattach is for inspection + continue/commit/rollback only.
- Reattach requires privileges; use `dormant_id` token and enforce `node_id` match.
- GC rolls back dormant transactions after lease expiry and marks state as EXPIRED/ROLLED_BACK.
- Keep `proc_id` reserved while dormant so OAT/OST calculations include the transaction.
- On server restart (new `server_instance_id`), purge or rollback stale dormant entries.

### 3.3 Transaction Markers (OIT/OAT/OST/NEXT)
- Verify header persistence and atomic updates of markers.
- Ensure OST is tracked for SNAPSHOT transactions only.
- Ensure OIT advances only after sweep and that OAT/OST updates are lock-safe.

### 3.4 Visibility Rules + Isolation Levels
- SNAPSHOT: create transaction snapshot at start; enforce visibility against xmin/xmax.
- READ COMMITTED variants:
  - READ CONSISTENCY: statement-level snapshot.
  - RECORD VERSION: read old versions when needed.
  - NO RECORD VERSION: wait or error on conflicts.
- SNAPSHOT TABLE STABILITY: enforce table reservations and conflicts.

### 3.5 Metadata MVCC
- Apply MVCC to catalog reads/writes so metadata respects transaction isolation.
- Ensure resolver and catalog caches are transaction-aware or invalidated safely.

### 3.6 Sweep Trigger + OIT Advancement
- Confirm sweep trigger uses (OST - OIT) > sweep_interval.
- Implement or validate sweep scanning of TIP to advance OIT.
- Expose sweep status/metrics via monitoring tables.

### 3.7 Garbage Collection (Cooperative + Background)
- Cooperative GC: remove dead versions during page reads based on OIT/visibility.
- Background GC: track dirty pages and clean asynchronously (configurable).
- Integrate index GC callbacks after heap cleanup.

### 3.8 Long-Running Transaction Management
- Implement threshold-based detection and logging (Firebird-style).
- Optional actions: rollback read-only or terminate connection (default log only).
- Expose MON$TRANSACTIONS and MON$DATABASE markers as live values.

### 3.9 Monitoring + Config
- Ensure MON$ views return real marker values, not placeholders.
- Implement configuration parameters for sweep interval, GC policy, long transaction thresholds.

### 3.10 Tests
- Isolation tests with overlapping transactions (snapshot vs read committed).
- OIT/OST updates and sweep trigger behavior.
- GC reclaim behavior and index cleanup.
- Long-transaction detection and monitoring queries.

## Suggested Order
1) Track 2 core payload/AST changes (needed for accurate transaction semantics).
2) Track 3 markers/visibility/sweep (foundation for MVCC correctness).
3) Track 1 persistence with MVCC-safe catalog reads (durability after isolation).

## Open Decisions / Clarifications
- Catalog cache granularity: per-attachment cache with generation checks is planned; confirm if a global cache with per-transaction views is preferred instead.
