# Migration from PostgreSQL

**Last Updated:** 2026-02-03

---

## Overview

ScratchBird supports live migration from PostgreSQL using logical replication.
This page describes the end‑to‑end flow and required prerequisites.

---

## Prerequisites (Source)

- `wal_level = logical`
- `max_replication_slots` and `max_wal_senders` sized for migration
- A replication user with privileges:
  - `REPLICATION`
  - Access to schemas/tables being migrated

---

## ScratchBird Setup

1. Create a foreign server definition (connection details to the source).
2. Define the migration source using logical replication.

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

---

## Start Migration

```sql
START MIGRATION FOR TABLE public.users
    FROM legacy_pg
    OPTIONS (order_by 'id', defer_indexes TRUE);
```

Monitor progress:

```sql
SHOW MIGRATION STATUS FOR TABLE public.users;
```

---

## Validation and Cutover

```sql
VALIDATE MIGRATION FOR TABLE public.users SAMPLE 0.01;
CUTOVER TABLE public.users;
```

---

## Rollback

```sql
ROLLBACK MIGRATION FOR TABLE public.users PRESERVE LOCAL DATA;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
