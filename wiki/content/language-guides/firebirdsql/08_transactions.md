# Transactions

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| START TRANSACTION | ScratchBird tracked | Transaction manager | Firebird-compatible syntax supported. |
| COMMIT/ROLLBACK | ScratchBird tracked | Transaction manager | Standard semantics supported. |
| SAVEPOINT | ScratchBird tracked | Transaction manager | SAVEPOINT/ROLLBACK TO/RELEASE supported. |
| Isolation levels | ScratchBird tracked | Transaction manager | Read committed/consistency semantics mapped to ScratchBird MGA. |
| Two-phase (PREPARE) | Emulated | Transaction manager | Supported only if ScratchBird implements 2PC. |

## Example

```sql
SET TRANSACTION READ COMMITTED;
UPDATE users SET active = 1;
COMMIT;
```

## Differences

- Firebird uses record versioning with garbage collection; ScratchBird uses MGA
  with TIP and sweep/GC. Results are compatible, internal cleanup differs.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
