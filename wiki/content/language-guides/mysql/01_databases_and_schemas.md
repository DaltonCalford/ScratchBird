# Databases and Schemas

**Last Updated:** 2026-02-03

---

MySQL uses databases as the primary namespace. ScratchBird maps them into its
catalog tree.

## Identical Behavior

```sql
CREATE DATABASE app;
USE app;
```

## Emulated / Mapped

- CREATE DATABASE is metadata in ScratchBird; no separate MySQL data directory.
- CHARACTER SET/COLLATE options are stored when supported by ScratchBird’s
  charset/collation system.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
