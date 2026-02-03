# Utilities

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| SET STATISTICS | ScratchBird tracked | Statistics manager | Updates ScratchBird stats. |
| EXECUTE STATEMENT | Emulated | PSQL/runtime | Supported when ScratchBird PSQL provides equivalent behavior. |
| BACKUP/RESTORE utilities | Out of scope | External tools | ScratchBird uses its own tooling. |
| SET PLAN / EXPLAIN | Emulated | Planner | Firebird plan output is mapped to ScratchBird plan details. |

## Example

```sql
SET STATISTICS INDEX idx_users_email;
```

## Differences

- Firebird-specific backup/restore utilities are not available; use ScratchBird
  tools and SQL commands.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
