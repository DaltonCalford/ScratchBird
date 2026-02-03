# Transactions

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| BEGIN/COMMIT/ROLLBACK | ScratchBird tracked | Transaction manager | Standard semantics supported. |
| Isolation levels | ScratchBird tracked | Transaction manager | READ COMMITTED/REPEATABLE READ/SERIALIZABLE mapped to ScratchBird MGA. |
| Savepoints | ScratchBird tracked | Transaction manager | SAVEPOINT/ROLLBACK TO/RELEASE supported. |
| Two-phase (PREPARE TRANSACTION) | Emulated | Transaction manager | Supported only if ScratchBird implements 2PC. |
| Deferrable constraints | Emulated | Constraint manager | Enforced only if ScratchBird supports deferral. |

## Example

```sql
BEGIN;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
UPDATE users SET active = TRUE;
COMMIT;
```

Savepoints:

```sql
SAVEPOINT sp1;
ROLLBACK TO sp1;
RELEASE SAVEPOINT sp1;
```

## Differences

- PostgreSQL uses WAL and VACUUM. ScratchBird uses MGA with TIP and sweep/GC.
- Long-running transactions delay sweep instead of VACUUM cleanup.
- WAL is optional in ScratchBird Alpha and not required for correctness.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
