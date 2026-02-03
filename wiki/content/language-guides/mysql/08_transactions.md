# Transactions

**Last Updated:** 2026-02-03

---

```sql
START TRANSACTION;
UPDATE users SET active = 1;
COMMIT;
```

Savepoints:

```sql
SAVEPOINT sp1;
ROLLBACK TO sp1;
RELEASE SAVEPOINT sp1;
```

## Differences

- ScratchBird uses MGA; InnoDB undo/redo and autocommit semantics may differ in
  edge cases.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
