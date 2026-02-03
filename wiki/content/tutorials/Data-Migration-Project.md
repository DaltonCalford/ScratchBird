# Data Migration Project

**Last Updated:** 2026-02-03

---

## Goal

Execute a staged live migration from a legacy database into ScratchBird.

---

## Step 1: Define Migration Source

```sql
CREATE MIGRATION SOURCE legacy
    FROM SERVER legacy_server
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

## Step 2: Start Migration

```sql
START MIGRATION FOR TABLE public.users
    FROM legacy
    OPTIONS (order_by 'id', defer_indexes TRUE);
```

---

## Step 3: Monitor Progress

```sql
SHOW MIGRATION STATUS FOR TABLE public.users;
```

---

## Step 4: Validate

```sql
VALIDATE MIGRATION FOR TABLE public.users SAMPLE 0.01;
```

---

## Step 5: Cutover

```sql
CUTOVER TABLE public.users;
```

---

## Step 6: Rollback (If Needed)

```sql
ROLLBACK MIGRATION FOR TABLE public.users PRESERVE LOCAL DATA;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
