# Migration Overview

**Last Updated:** 2026-02-03

---

## Purpose

ScratchBird live migration is designed for **zero‑downtime** movement from a
legacy database into ScratchBird while applications continue to run. The engine
routes queries per table based on migration state, supports CDC, and provides
validation, cutover, and rollback controls.

---

## Supported Sources and CDC

| Source | Typical CDC Method |
|--------|---------------------|
| PostgreSQL 9.6+ | Logical replication |
| MySQL 5.7+ / MariaDB 10+ | Binlog (row format) |
| Firebird 2.5+ | Trigger‑based capture |
| SQL Server 2016+ | Change Tracking / CDC |
| Oracle 12c+ | LogMiner / GoldenGate |

---

## Architecture (Simplified)

```
Client → ScratchBird listener → parser → query router
           ├─ local engine execution
           └─ remote execution + CDC + dual‑write
```

The query router intercepts statements **after semantic analysis** and routes
reads/writes based on the table state.

---

## Table Migration State Machine

States are tracked per table:

- NOT_STARTED
- BULK_LOADING
- SYNCHRONIZING
- DUAL_WRITE
- CUTOVER_READY
- LOCAL_ONLY
- ROLLBACK
- PAUSED
- ERROR

### Routing Behavior (Simplified)

| Query | NOT_STARTED | BULK_LOADING | SYNCHRONIZING | DUAL_WRITE | LOCAL_ONLY |
|------|-------------|--------------|---------------|------------|-----------|
| SELECT | Remote | Remote | Remote | Local | Local |
| INSERT/UPDATE/DELETE | Remote | Blocked | Remote + Queue | Both | Local |
| DDL | Remote | Blocked | Blocked | Blocked | Local |

---

## Core Migration Commands

### Define a Migration Source

```sql
CREATE MIGRATION SOURCE legacy_pg
    FROM SERVER legacy_pg_server
    OPTIONS (
        cdc_mode 'logical_replication',
        batch_size 10000,
        parallel_workers 4,
        rate_limit 50000,
        conflict_strategy 'source_wins',
        validation_sample 0.01
    );
```

### Start Migration

```sql
START MIGRATION FOR TABLE public.users
    FROM legacy_pg
    OPTIONS (
        order_by 'id',
        defer_indexes TRUE
    );

START MIGRATION FOR SCHEMA public
    FROM legacy_pg
    EXCLUDING (audit_log);
```

### Pause / Resume / Abort

```sql
PAUSE MIGRATION FOR TABLE public.users;
RESUME MIGRATION FOR TABLE public.users;
ABORT MIGRATION FOR TABLE public.users;
```

### Cutover / Rollback

```sql
CUTOVER TABLE public.users;
ROLLBACK MIGRATION FOR TABLE public.users PRESERVE LOCAL DATA;
```

---

## Status and Validation

### Status

```sql
SHOW MIGRATION STATUS;
SHOW MIGRATION STATUS FOR TABLE public.users;
```

### Validation

```sql
VALIDATE MIGRATION FOR TABLE public.users SAMPLE 0.01;
VALIDATE MIGRATION FOR TABLE public.users FULL;
```

---

## Typical Migration Phases

1. **Connect and map** legacy schemas to ScratchBird schemas.
2. **Bulk load** tables into ScratchBird.
3. **Synchronize** with CDC until lag is acceptable.
4. **Dual‑write** for stability and validation.
5. **Cutover** to local‑only execution.
6. **Rollback** if validation fails or issues arise.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
