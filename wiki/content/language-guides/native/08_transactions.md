# Transactions

**Last Updated:** 2026-02-03

---

```sql
BEGIN;
UPDATE app.users SET active = FALSE WHERE id = 1;
COMMIT;
```

Savepoints:

```sql
SAVEPOINT sp1;
ROLLBACK TO sp1;
RELEASE SAVEPOINT sp1;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
