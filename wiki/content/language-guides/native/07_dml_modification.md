# DML: INSERT/UPDATE/DELETE

**Last Updated:** 2026-02-03

---

```sql
INSERT INTO app.users (id, email) VALUES (1, 'a@x');
UPDATE app.users SET active = FALSE WHERE id = 1;
DELETE FROM app.users WHERE id = 1;
```

MERGE and COPY are supported for bulk operations.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
