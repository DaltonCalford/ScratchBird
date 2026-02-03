# Utilities

**Last Updated:** 2026-02-03

---

```sql
EXPLAIN SELECT * FROM users WHERE id = 1;
ANALYZE TABLE users;
```

## LOAD DATA

MySQL `LOAD DATA INFILE` maps to ScratchBird bulk‑load when enabled, but
server‑side file access is restricted.

```sql
LOAD DATA INFILE 'data.csv'
INTO TABLE users
FIELDS TERMINATED BY ','
IGNORE 1 LINES;
```

## Differences

- Server‑side file paths may be rejected.
- `ANALYZE TABLE` maps to ScratchBird statistics update.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
