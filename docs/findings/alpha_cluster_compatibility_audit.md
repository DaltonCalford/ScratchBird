# Alpha Cluster Compatibility Audit

## Scope
Identify conflicts between the current single-node implementation and the planned cluster design. Provide alpha-safe guardrails so local/standalone remains correct while not blocking beta cluster support (shared-nothing now, shared-storage multi-writer later).

## Design Assumptions
- Alpha: standalone/local only, shared-nothing allowed.
- Beta: cluster support, shared-storage multi-writer is the eventual goal.
- Cluster time enforcement provides a trusted time source for conflict resolution and UUIDv7 ordering.

## High-Risk Conflicts (Code vs Cluster Goals)
1) Exclusive file lock prevents multi-writer and multi-process access.
   - `src/core/database.cpp:432` and `src/core/database.cpp:537` use `flock(..., LOCK_EX)`.
   - Conflict: shared-storage multi-writer and some cluster topologies require non-exclusive local file locking or a distributed lock manager.

2) Transaction IDs and commit state are local-only.
   - `src/core/transaction_manager.cpp` uses local `next_xid_` and local CLOG/TIP.
   - Conflict: cluster MGA requires a global ordering or composite IDs (node_id + local_xid), plus global visibility horizons.

3) Lock manager is local-only.
   - `src/core/lock_manager.cpp` uses in-memory locks keyed by local proc_id.
   - Conflict: multi-writer cluster needs distributed lock coordination or strict shard ownership rules.

4) Domains are schema-scoped and local.
   - `DomainRecord` includes `schema_id` and APIs require schema_id (example: `include/scratchbird/core/catalog_manager.h:1605`).
   - Conflict: cluster domains must be global with dialect tags and conflict resolution.

5) Domain/type definitions reference local TOAST OIDs.
   - `DomainRecord` uses `base_type_oid`, `check_expr_oid` in `src/core/catalog_manager.cpp:664`.
   - Conflict: OIDs are local, so replicating definitions across nodes will diverge without stable content-addressing.

6) UUIDv7 uses local wall clock only.
   - `src/core/uuidv7.cpp:10` uses `system_clock::now()`.
   - Conflict: conflict resolution by UUID ordering needs cluster time enforcement to avoid skew.

7) Sequences are local and cached in-memory.
   - `SequenceState` and `sequence_cache_` are local (`include/scratchbird/core/catalog_manager.h:436`, `:3118`).
   - Conflict: global sequences require leasing or centralized allocation to avoid duplicates.

8) Page LSNs are never advanced.
   - LSNs are initialized to 0 in multiple page types (e.g., `src/core/page_manager.cpp:998`).
   - Conflict: replication and conflict resolution need monotonically advancing LSN or equivalent ordering metadata.

9) Cluster identity is absent in code/config.
   - No `cluster_id` or `node_id` stored in database header or config.
   - Conflict: cluster trust and domain origin metadata require persistent cluster and node identifiers.

10) ShardingSphere-style routing is not implemented.
   - Catalog and query planner assume local catalog only; no shard routing metadata.
   - Conflict: horizontal scaling requires routing rules, shard ownership, and cross-shard execution policy.

## Alpha Guardrails (Required Now to Avoid Beta Rework)
- **Locking abstraction**: replace direct `flock` calls with a `StorageLockProvider` interface.
  - Alpha default: local exclusive lock.
  - Beta: distributed lock manager (DLM) or shared lock mode.

- **Cluster time provider**: inject a `TimeSource` for UUIDv7 and security timestamps.
  - Alpha default: local system clock.
  - Beta: cluster time service (majority agreement).

- **Transaction ID extensibility**:
  - Reserve fields or extend TIP/CLOG record format to allow composite IDs (node_id + local_xid).
  - Alpha can keep local_xid only but must keep forward-compatible storage.

- **Global horizons and epochs**:
  - Add hooks for a global horizon provider (cluster-wide OIT/OAT) to avoid premature GC in beta.

- **Sequence scope and leasing**:
  - Add sequence scope (LOCAL/GLOBAL) in catalog.
  - Alpha: allow only LOCAL sequences, enforce GLOBAL as not supported.
  - Beta: add lease allocator for GLOBAL sequences.

- **Domain metadata for replication**:
  - Add dialect_tag, compat_name, definition_hash, storage_hash, origin_node_id, origin_cluster_id.
  - Store full domain definitions in a stable format (TOAST + hash), not only local OIDs.

- **Cluster identity persistence**:
  - Add cluster_id and node_id to the database header (or catalog root).
  - Alpha: default cluster_id = local-only cluster; node_id = database UUID.

- **LSN or ordering metadata**:
  - Introduce a monotonic page sequence counter for replication ordering (even if unused in alpha).

- **Sharding metadata placeholders**:
  - Reserve catalog tables for shard routing rules and node roles.
  - Alpha: no routing, but schema is in place to avoid data migration later.

## Plan Allocation
- `docs/planning/plan_01_core_storage_gc.md`: add lock provider abstraction, LSN progression, and cluster_id/node_id persistence.
- `docs/planning/plan_03_security_context_auth_audit_quorum.md`: add TimeSource/cluster time integration and domain DDL privileges.
- `docs/planning/plan_06_metadata_show_and_catalog.md`: global domain tables and collision tracking.
- `docs/planning/plan_10_cluster_domains_and_conflict_resolution.md`: domain conflict algorithm and rebind support.
- New: `docs/planning/plan_11_alpha_cluster_compatibility_guardrails.md` (alpha-specific guardrails and tests).

## Verification (Alpha)
- Ensure local-only behavior remains correct with guardrail abstractions enabled.
- Add tests that validate:
  - UUIDv7 uses TimeSource abstraction.
  - Sequence scope enforcement (LOCAL allowed, GLOBAL rejected).
  - Domain metadata includes dialect_tag/compat_name/hashes.
  - Database header stores cluster_id and node_id.
  - LSN advances on page writes (monotonic).

