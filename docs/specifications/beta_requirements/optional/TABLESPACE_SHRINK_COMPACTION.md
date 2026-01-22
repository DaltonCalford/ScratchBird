# Optional Beta Specification: Tablespace Shrink and Compaction

Status: Optional Beta  
Last Updated: January 2026

## 1. Purpose
Provide a safe way to reclaim tablespace file size by compacting live data
into a smaller footprint. This is equivalent to SQL Server "shrink", but
implemented in an MGA-safe way and designed to avoid data loss.

## 2. Scope and Non-Goals

**In scope**
- Reclaim free space in a tablespace by moving live pages into lower ranges.
- Online compaction using MGA-safe migration mechanics.
- Optional offline compaction (admin-only) for faster, simpler operation.

**Out of scope**
- Cross-node movement (sharding); see cluster specs.
- Compression/encryption changes during shrink (handled elsewhere).

## 3. Safety Model (MGA)

Shrink must preserve snapshot isolation:
- Old versions visible to older snapshots must remain readable.
- Cutover only occurs once the migration snapshot is safe.
- Old storage is only released after pre-cutover snapshots finish.

## 4. Modes

### 4.1 Online (default) - Copy/Swap
Recommended and safest. Uses the online migration engine:
1. Create new target datafiles (same tablespace ID).
2. Copy table data into new files (shadow storage).
3. Capture and replay deltas (per-table migration).
4. Cutover with a brief exclusive lock per table.
5. Retire old files after MGA safety check.

### 4.2 Offline (optional)
Requires an exclusive tablespace lock and no active connections.
All data is rewritten into a compacted file layout, then the original file
is truncated or replaced. This is faster but blocks all access.

## 5. DDL / CLI Surface (V2 parser only)

**DDL (ScratchBird native only):**
```
ALTER TABLESPACE <name>
    SHRINK
    WITH (
        mode = online,                 -- online | offline
        target_free_percent = 5,       -- desired free space at end of file
        throttle_ms = 50,              -- online only
        max_lag_ms = 5000,             -- online only
        lock_timeout_ms = 5000         -- online only
    );
```

**CLI (sb_admin):**
```
sb_admin tablespace shrink <name> [--mode online|offline] [--target-free 5]
```

Emulated parsers must reject tablespace DDL (parity policy).

## 6. Workflow (Online)

1. **Prepare**: create target datafiles within the same tablespace.
2. **Copy**: migrate tables one-by-one using the online tablespace migration
   workflow (shadow + delta log).
3. **Catch-up**: replay deltas until lag <= max_lag_ms.
4. **Cutover**: swap roots for each table; record cutover xid.
5. **Cleanup**: retire old files only after pre-cutover snapshots finish.

## 7. Monitoring and Progress

Use existing migration views and metrics:
- `sys.tablespace_migrations` (per-table state)
- `sys.tablespace_migration_progress` (if defined)
- Prometheus tablespace migration metrics in
  `docs/specifications/operations/PROMETHEUS_METRICS_REFERENCE.md`

**Optional aggregate:** provide a tablespace-level summary by aggregating
per-table rows for a given tablespace ID.

## 8. Permissions

Requires admin or tablespace owner privileges.

## 9. Failure and Recovery

- Resume is supported (online) using migration state.
- Cancel drops shadow storage and resets state.
- Offline mode requires manual retry if interrupted.

## 10. Related Specifications

- `docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`
- `docs/specifications/storage/TABLESPACE_SPECIFICATION.md`
- `docs/specifications/operations/MONITORING_SQL_VIEWS.md`
- `docs/specifications/operations/PROMETHEUS_METRICS_REFERENCE.md`
