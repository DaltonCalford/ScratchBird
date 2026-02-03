# Migration from MySQL

**Last Updated:** 2026-02-03

---

## Overview

ScratchBird supports live migration from MySQL/MariaDB using binlog‑based CDC.

---

## Prerequisites (Source)

- Binary logging enabled (`log_bin = ON`)
- Row‑based binlog format (`binlog_format = ROW`)
- Unique `server_id`
- A replication user with privileges:
  - `REPLICATION SLAVE`
  - `REPLICATION CLIENT`
  - Access to schemas/tables being migrated

---

## ScratchBird Setup

```sql
CREATE MIGRATION SOURCE legacy_mysql
    FROM SERVER legacy_mysql_server
    OPTIONS (
        cdc_mode 'binlog',
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
START MIGRATION FOR TABLE app.users
    FROM legacy_mysql
    OPTIONS (order_by 'id', defer_indexes TRUE);
```

---

## Validation and Cutover

```sql
VALIDATE MIGRATION FOR TABLE app.users SAMPLE 0.01;
CUTOVER TABLE app.users;
```

---

## Rollback

```sql
ROLLBACK MIGRATION FOR TABLE app.users PRESERVE LOCAL DATA;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
