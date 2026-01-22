# SBCLUSTER-11: Shard Migration and Rebalancing

**Status:** Beta specification
**Last Updated:** 2026-01-20
**Scope:** Cross-node movement of shards, shard splits/merges, and rebalancing

---

## 1) Purpose

Define a safe, online data movement protocol for moving tables and partitions
between servers in a ScratchBird cluster. This includes shard rebalancing,
resharding (split/merge), and capacity-driven migration.

---

## 2) Scope and Non-Goals

**In scope:**
- Moving shard data between nodes
- Rebalancing on topology changes
- Shard split/merge workflows
- Cutover and rollback semantics

**Out of scope:**
- Cross-shard transactions (not supported)
- Multi-master conflict resolution details (see replication spec)
- Logical replication format (see replication spec)

---

## 3) Related Specifications

- `SBCLUSTER-01-CLUSTER-CONFIG-EPOCH.md` (map versioning)
- `SBCLUSTER-05-SHARDING.md` (shard map and routing)
- `SBCLUSTER-06-DISTRIBUTED-QUERY.md` (coordinator model)
- `SBCLUSTER-07-REPLICATION.md` (write-after log streaming)
- `SBCLUSTER-09-SCHEDULER.md` (cluster job orchestration)

---

## 4) Design Principles

1. **Online by default**: allow reads/writes during migration with brief cutover.
2. **Shard-local MVCC**: MGA remains shard-local; no cross-shard transactions.
3. **Versioned routing**: sessions pin to a shard map version.
4. **Idempotent migration**: resume after failure without data loss.
5. **Explicit cutover**: shard map update is the sole authority for routing.

---

## 5) Terminology

- **Shard Map Version**: Immutable mapping used for routing.
- **Migration Epoch**: Unique identifier for a migration attempt.
- **Source Shard**: Current owner of data.
- **Target Shard**: Destination owner of data.
- **Delta Stream**: Ordered stream of DML changes applied after bulk copy.

---

## 6) Migration State Machine

```
PLANNED -> PREPARE -> COPY -> CATCH_UP -> CUTOVER -> CLEANUP -> DONE
         \-> FAILED
         \-> CANCELED
```

State is recorded in `sys.shard_migrations` and is tied to a migration epoch.

---

## 7) Data Movement Protocol

### 7.1 Prepare

- Allocate target shard and storage
- Create shard metadata and catalog entries
- Start migration epoch and register in cluster catalog

### 7.2 Bulk Copy

- Take a consistent snapshot on source shard
- Stream data in chunks to target shard
- Verify checksums per chunk

### 7.3 Delta Stream (Catch-Up)

- Capture DML on source shard (logical change stream)
- Apply changes to target in commit order
- Monitor lag; repeat until lag <= threshold

### 7.4 Cutover

- Freeze writes briefly on source shard
- Apply final deltas
- Publish new shard map version via CCE
- New sessions route to target shard

### 7.5 Cleanup

- Mark source shard as retired
- Keep source readable for pre-cutover sessions
- Garbage collect old storage after old sessions exit

---

## 8) Routing Rules

- Each session pins a shard map version at start
- Queries use that version for routing throughout the session
- New map version applies only to new sessions

This avoids mid-session routing changes and preserves snapshot consistency.

---

## 9) Shard Split and Merge

### 9.1 Split

- Create N target shards for a source shard
- Recompute shard key ranges or hash ring vnodes
- Copy rows based on key range
- Cutover to new shard map

### 9.2 Merge

- Create a single target shard
- Copy rows from source shards
- Cutover and retire sources

---

## 10) DDL and Control Plane APIs

**DDL (Beta):**
```
ALTER TABLE <table> SPLIT SHARD;
ALTER TABLE <table> MERGE SHARDS;
ALTER TABLE <table> MOVE SHARD <shard_id> TO <node_id>;
ALTER TABLE <table> REBALANCE;
```

**Control Plane:**
- `sb_cluster rebalance --dry-run`
- `sb_cluster migrate-shard --plan`
- `sb_cluster migrate-shard --execute`

---

## 11) Observability

Views:
- `sys.shard_migrations`
- `sys.shard_map_versions`
- `sys.shard_stats`

Metrics:
- `cluster_shard_migration_bytes_total`
- `cluster_shard_migration_bytes_per_sec`
- `cluster_shard_migration_rows_per_sec`
- `cluster_shard_migration_eta_seconds`
- `cluster_shard_migration_lag_ms`
- `cluster_shard_migration_throttle_state`
- `cluster_shard_migration_state`

---

## 11.1 Catalog Placeholders (Planned)

These catalog tables back the monitoring views and migration orchestration.

**Planned system tables:**
- `pg_shard_migrations` (authoritative state)
- `pg_shard_map_versions` (immutable map versions)

**Proposed fields (`pg_shard_migrations`):**
- `migration_uuid` (UUID)
- `source_shard_uuid` (UUID)
- `target_shard_uuid` (UUID)
- `shard_key_spec` (text)
- `state` (PREPARE|COPY|CATCH_UP|CUTOVER|CLEANUP|DONE|FAILED|CANCELED)
- `migration_start_xid` (uint64)
- `cutover_xid` (uint64)
- `bytes_copied` (uint64)
- `rows_copied` (uint64)
- `bytes_per_sec` (double)
- `rows_per_sec` (double)
- `eta_seconds` (uint64)
- `last_lag_ms` (uint64)
- `last_lag_sample_at` (timestamp)
- `throttle_state` (text)
- `throttle_sleep_ms` (uint32)
- `last_progress_at` (timestamp)
- `last_error_code` (int32)
- `created_at` (timestamp)
- `updated_at` (timestamp)

**Proposed fields (`pg_shard_map_versions`):**
- `map_version` (uint64)
- `map_uuid` (UUID)
- `created_at` (timestamp)
- `created_by` (UUID)
- `description` (text)
- `signature` (bytes)

---

## 12) Failure Handling

- **Source failure**: migration pauses; resume after source recovery
- **Target failure**: migration fails; target cleaned up
- **Cutover failure**: if map update not committed, source remains live

All steps are designed to be idempotent.

---

## 13) Security

- All migration traffic uses cluster TLS identities
- Only cluster admins can initiate migrations
- All migration actions are auditable events

---

## 14) Testing

- Live migration under write load
- Split/merge correctness
- Resume after failure
- Cutover correctness with pinned sessions

---

## 15) Related Documents

- `SBCLUSTER-05-SHARDING.md`
- `SBCLUSTER-06-DISTRIBUTED-QUERY.md`
- `SBCLUSTER-07-REPLICATION.md`
- `docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`
