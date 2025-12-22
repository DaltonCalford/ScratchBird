# Plan 11 - Alpha Cluster Compatibility Guardrails

## Scope
Add alpha-safe guardrails so standalone mode remains correct while not blocking beta cluster support. Shared-nothing is allowed for alpha; shared-storage multi-writer is the eventual goal.

## Priority
P0 (prevents incompatible on-disk or API assumptions).

## References
- `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `docs/specifications/TRANSACTION_DISTRIBUTED.md`
- `docs/specifications/REPLICATION_AND_SHADOW_PROTOCOLS.md`
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
- `docs/planning/plan_01_core_storage_gc.md`
- `docs/planning/plan_03_security_context_auth_audit_quorum.md`
- `docs/planning/plan_10_cluster_domains_and_conflict_resolution.md`
- `docs/findings/alpha_cluster_compatibility_audit.md`

## Order of Implementation
1) Cluster identity and time abstraction.
2) Storage lock provider abstraction.
3) Transaction ID extensibility + global horizon hooks.
4) Sequence scope and lease placeholders.
5) LSN/order metadata.
6) Shard routing placeholders.

## Concrete Code Touchpoints (Exact Files + Functions)
- Cluster identity:
  - `include/scratchbird/core/database.h` (`DatabaseHeader` fields)
  - `src/core/database.cpp` (`init_header_page`, `validate_header`)
- Time source:
  - `include/scratchbird/core/uuidv7.h` / `src/core/uuidv7.cpp` (injectable time source)
- Storage lock provider:
  - `src/core/database.cpp` (flock usage)
  - `src/server/daemon.cpp` (flock usage)
- Transaction ID extensibility:
  - `src/core/transaction_manager.cpp` (XID allocation, comparisons)
  - `src/core/clog.cpp` / TIP format
- Sequence scope:
  - `src/core/catalog_manager.cpp` (SequenceRecord)
  - `include/scratchbird/core/catalog_manager.h` (SequenceInfo)
- LSN/order:
  - `src/core/page_manager.cpp` / `src/core/buffer_pool.cpp`
- Shard placeholders:
  - `src/core/catalog_manager.cpp` / `include/scratchbird/core/catalog_manager.h`

## Implementation Tasks
- Add `cluster_id` and `node_id` to database header or catalog root (persisted).
- Introduce `TimeSource` interface for UUIDv7 and security timestamps.
- Replace direct `flock` calls with `StorageLockProvider` interface (local-only implementation for alpha).
- Extend transaction ID record formats to allow composite IDs (node_id + local_xid).
- Add global horizon provider interface for GC (local provider in alpha).
- Add sequence scope (LOCAL/GLOBAL) in catalog; block GLOBAL in alpha.
- Add monotonic page LSN or order counter (even if replication is disabled).
- Add placeholder catalog tables for shard routing metadata and node roles.

## Required Data/Schema Changes
- Database header: `cluster_id`, `node_id`, `cluster_epoch` (reserved fields).
- Sequence catalog: `sequence_scope` and optional lease metadata.
- Shard routing catalog placeholders: `sys.cluster.configuration.shards`, `sys.cluster.configuration.shard_mapping`,
  `sys.cluster.configuration.cluster_nodes`.

## Completion Checklist (Developer)
- [ ] `cluster_id` and `node_id` are persisted and retrievable.
- [ ] UUIDv7 uses `TimeSource` abstraction (local default).
- [ ] Storage lock uses `StorageLockProvider` abstraction (local exclusive default).
- [ ] Transaction ID format is forward-compatible with composite IDs.
- [ ] Global horizon hooks exist (local provider used in alpha).
- [ ] Sequence scope is enforced (GLOBAL blocked in alpha).
- [ ] LSN/order counter advances on page writes.
- [ ] Shard routing placeholder tables exist.

## Completion Checklist (Auditor)
- [ ] No on-disk format blocks future composite XIDs.
- [ ] Domain and security metadata include cluster identity fields.
- [ ] Standalone mode remains correct with guardrails enabled.

## Testing Requirements
- UUIDv7 uses injected time source (unit test).
- Sequence scope enforcement: GLOBAL rejected, LOCAL allowed.
- Database header stores cluster_id/node_id (restart persistence test).
- LSN monotonicity test for page writes.
- Storage lock provider can be swapped (mock test).

## Acceptance Criteria
- Alpha remains stable and local-only.
- No schema or storage decisions require incompatible migrations for beta cluster support.

## Full Implementation Detail (No Ambiguity)
### 1) Database Header Fields
- Update `DatabaseHeader` in `include/scratchbird/core/database.h`:
  - Add `cluster_id` (UUID v7 bytes)
  - Add `node_id` (UUID v7 bytes)
  - Add `cluster_epoch` (uint64_t)
- Update `src/core/database.cpp`:
  - Set these fields in `init_header_page()` for new DBs.
  - Validate them in `validate_header()`.

### 2) TimeSource Abstraction
- Add `TimeSource` interface in `include/scratchbird/core/time_source.h`.
- Update `generateUuidV7()` to call `TimeSource::nowMillis()`.
- Default implementation uses system clock; inject cluster time in beta.

### 3) StorageLockProvider Abstraction
- Add `StorageLockProvider` interface (`acquireExclusive(path)`, `release(path)`).
- Replace `flock()` usage in `src/core/database.cpp` and `src/server/daemon.cpp` with provider calls.
- Default provider uses `flock` for alpha.

### 4) Transaction ID Extensibility
- Reserve bits or fields for `origin_node_id` in transaction metadata (TIP/CL0G).
- Add helper for composite XID: `{origin_node_id, local_xid}`.
- Ensure comparisons use composite ordering (origin node first, then local xid).

### 5) Sequence Scope
- Extend `SequenceRecord` and `SequenceInfo`:
  - `sequence_scope` (0=LOCAL, 1=GLOBAL)
  - `lease_owner_id` (UUID) and `lease_expiry` (timestamp) placeholders.
- Enforce GLOBAL scope as unsupported in alpha (return clear error).

### 6) LSN / Order Counter
- Add `lsn_counter` to `DatabaseHeader` (monotonic increment).
- Update page write path (buffer flush) to set `PageHeader.lsn = ++lsn_counter`.

### 7) Shard Metadata Placeholders
- Add tables and minimal DDL in `CatalogManager`:
  - `sys.cluster.configuration.shards(shard_id, shard_name, shard_key_spec)`
  - `sys.cluster.configuration.shard_mapping(object_id, shard_id, rule_spec)`
  - `sys.cluster.configuration.cluster_nodes(node_id, cluster_id, role, status)`
- Populate with minimal defaults in standalone mode (single node).

## Common Failure Patterns
- Direct `flock` use scattered in code (bypasses abstraction).
- UUIDv7 uses system clock directly (cluster time not applied).
- On-disk formats lack reserved fields for node_id/epoch.
- Sequence APIs ignore scope field.
- LSN remains zero on all pages, breaking future replication order.
