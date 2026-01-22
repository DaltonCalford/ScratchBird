# Tablespace Online Migration (MGA)

**Status:** Beta specification (post-alpha)
**Last Updated:** 2026-01-20
**Scope:** Online table and index migration between tablespaces within a single database node

---

## 1) Purpose

Define an MGA-safe, online migration workflow for moving tables and indexes between
local tablespaces without long blocking locks.

Key goals:
- Preserve MGA snapshot semantics (no broken reads)
- Keep DML available during migration (brief cutover only)
- Support large tables with progress reporting and throttling

---

## 2) Scope and Non-Goals

**In scope:**
- Single-node migrations across local tablespaces
- Online `ALTER TABLE ... SET TABLESPACE ... ONLINE`
- Minimal blocking (short cutover)

**Out of scope:**
- Cross-node shard moves (see cluster sharding spec)
- Replication-driven moves
- Storage encryption or compression changes during migration

---

## 3) Design Overview

The online migration uses a **shadow table** in the target tablespace and a
**delta log** to capture concurrent changes. The base table remains authoritative
until cutover, so no duplicate visibility occurs during normal queries.

**High-level flow:**
1. Prepare: create shadow table + indexes in target tablespace
2. Copy: copy base table rows to shadow (snapshot)
3. Capture: log concurrent DML (delta log)
4. Catch-up: replay deltas to shadow until lag is small
5. Cutover: short exclusive lock to swap table roots
6. Cleanup: retire old storage after old snapshots finish

---

## 4) Metadata and Catalog Additions

### 4.1 Migration State

Add migration metadata to the table catalog (or a new migration catalog table):

```
struct TableMigrationState {
    UUID table_uuid;
    bool migrating;
    uint16_t source_tablespace_id;
    uint16_t target_tablespace_id;
    UUID shadow_table_uuid;
    UUID delta_log_table_uuid;
    uint64_t migration_start_xid;
    uint64_t cutover_xid;           // Set at cutover commit
    MigrationState state;           // NONE|PREPARE|COPY|CATCH_UP|CUTOVER|CLEANUP|DONE|FAILED
    uint64_t rows_copied;
    uint64_t rows_total_est;
    double rows_per_sec;            // Smoothed rate
    uint64_t eta_seconds;           // Estimated time remaining
    uint64_t last_lag_ms;           // Last observed catch-up lag
    uint64_t last_lag_sample_at;    // Timestamp (ms or epoch use consistent type)
    uint8_t throttle_state;         // NONE|LAG|IO|MANUAL
    uint32_t throttle_sleep_ms;     // Current throttle delay
    uint64_t last_progress_at;      // Timestamp of last progress update
    uint64_t last_error_code;
};
```

**State machine:**
- NONE -> PREPARE -> COPY -> CATCH_UP -> CUTOVER -> CLEANUP -> DONE
- ANY -> FAILED (recoverable)
- ANY -> CANCELED (if requested)

### 4.2 Delta Log

Delta log is a system-owned table (or internal queue) with ordered entries:

```
struct MigrationDelta {
    UUID table_uuid;
    uint64_t commit_xid;
    uint64_t sequence_id;   // Monotonic for ordering within a commit
    DeltaOp op;             // INSERT|UPDATE|DELETE
    RowKey key;             // Primary key or row locator
    RowImage new_row;       // For INSERT/UPDATE
};
```

---

## 5) DDL and API Surface

### 5.1 DDL

```
ALTER TABLE <table>
    SET TABLESPACE <tablespace>
    ONLINE
    WITH (
        batch_size = 10000,
        max_lag_ms = 5000,
        throttle_ms = 50,
        lock_timeout_ms = 5000
    );
```

### 5.2 Admin Controls

- `ALTER TABLE ... SET TABLESPACE ... ONLINE PAUSE`
- `ALTER TABLE ... SET TABLESPACE ... ONLINE RESUME`
- `ALTER TABLE ... SET TABLESPACE ... ONLINE CANCEL`

---

## 6) Migration Workflow (Detailed)

### 6.1 Prepare Phase

1. Acquire **MIGRATION LOCK** (blocks DDL, allows reads/writes)
2. Create shadow table in target tablespace
3. Create shadow indexes in target tablespace
4. Enable delta log capture for the base table
5. Release MIGRATION LOCK

### 6.2 Copy Phase

- Start a migration job with a stable snapshot at `migration_start_xid`
- Copy all visible rows from base table to shadow
- Track progress counters

### 6.3 Catch-Up Phase

- Repeatedly apply delta log entries in commit order
- Continue until lag <= `max_lag_ms`

### 6.4 Cutover Phase

1. Acquire **EXCLUSIVE LOCK** briefly
2. Stop new DML and flush remaining deltas
3. Swap table root pointers and index roots to shadow
4. Set `cutover_xid` in metadata
5. Release EXCLUSIVE LOCK

### 6.5 Cleanup Phase

- Mark old storage as retired
- Keep old storage readable for transactions with snapshot < `cutover_xid`
- GC old storage after all pre-cutover snapshots complete

---

## 7) Read/Write Routing Rules

### 7.1 During Migration (PREPARE/COPY/CATCH_UP)
- Queries use **base table only**
- Shadow table is hidden from normal reads
- DML targets base table and is mirrored into delta log

### 7.2 After Cutover
- If snapshot_xid < cutover_xid: read base table
- If snapshot_xid >= cutover_xid: read shadow (now live table)

This preserves MGA snapshot isolation without duplicate visibility.

---

## 8) Indexes and Constraints

- Build shadow indexes before cutover
- Replay deltas into shadow indexes during catch-up
- Validate unique constraints during catch-up and before cutover
- FK validation must run against shadow before cutover

---

## 9) Failure Handling

- **Crash during COPY/CATCH_UP**: resume from last checkpoint
- **Crash during CUTOVER**:
  - If cutover commit exists, new table is live
  - If not, base table remains live and migration can resume
- **Cancel**: drop shadow + delta log, reset migration state

---

## 10) Monitoring and Observability

Provide monitoring views:
- `sys.tablespace_migrations`
- `sys.tablespace_migration_progress`

Metrics:
- `tablespace_migration_rows_copied_total`
- `tablespace_migration_rows_per_sec`
- `tablespace_migration_eta_seconds`
- `tablespace_migration_lag_ms`
- `tablespace_migration_throttle_state`
- `tablespace_migration_state`

---

## 11) Security and Permissions

- Requires table owner or admin role
- Migration job runs as system-owned background task

---

## 12) Testing

- Online migration with concurrent DML
- Large table migration with throttling
- Cancel and resume
- Recovery mid-migration
- Cutover with active long-running snapshot

---

## 13) Related Specifications

- Tablespace core spec: `docs/specifications/storage/TABLESPACE_SPECIFICATION.md`
- MGA rules: `MGA_RULES.md`
- Storage engine: `docs/specifications/storage/STORAGE_ENGINE_MAIN.md`
- Cluster sharding (cross-node): `docs/specifications/Cluster%20Specification%20Work/SBCLUSTER-11-SHARD-MIGRATION-AND-REBALANCING.md`
- Optional Beta: Tablespace shrink/compaction (MGA-safe):
  `docs/specifications/beta_requirements/optional/TABLESPACE_SHRINK_COMPACTION.md`
