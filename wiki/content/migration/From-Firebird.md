# Migration from Firebird

**Last Updated:** 2026-02-03

---

## Overview

ScratchBird supports live migration from Firebird using trigger‑based CDC.
This method mirrors changes into a change log that ScratchBird consumes.

---

## Prerequisites (Source)

- A Firebird user with privileges to create triggers and read system tables.
- Ability to install change‑capture triggers or shadow tables.

---

## ScratchBird Setup

```sql
CREATE MIGRATION SOURCE legacy_fb
    FROM SERVER legacy_fb_server
    OPTIONS (
        cdc_mode 'trigger',
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
START MIGRATION FOR TABLE public.customer
    FROM legacy_fb
    OPTIONS (order_by 'id', defer_indexes TRUE);
```

---

## Validation and Cutover

```sql
VALIDATE MIGRATION FOR TABLE public.customer SAMPLE 0.01;
CUTOVER TABLE public.customer;
```

---

## Rollback

```sql
ROLLBACK MIGRATION FOR TABLE public.customer PRESERVE LOCAL DATA;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
