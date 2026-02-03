# Transactions

**Last Updated:** 2026-02-03

---

ScratchBird uses Firebird‑style MGA (multi‑generational architecture) for
transaction isolation. Each statement runs inside a transaction, and snapshots
are stable within the transaction boundary.

---

## Basic Commands

```sql
BEGIN;
COMMIT;
ROLLBACK;
```

### Savepoints

```sql
SAVEPOINT sp1;
ROLLBACK TO sp1;
RELEASE SAVEPOINT sp1;
```

---

## Isolation Levels

ScratchBird supports standard isolation levels:

- READ COMMITTED
- REPEATABLE READ
- SERIALIZABLE

Use `SET TRANSACTION` or dialect‑specific syntax to select isolation level.

---

## Notes

- Readers do not block writers under MGA.
- Old versions are reclaimed by sweep/GC once no active transaction can see them.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
