# DML: INSERT/UPDATE/DELETE

**Last Updated:** 2026-02-03

---

```sql
INSERT INTO users (email) VALUES ('a@x');
UPDATE users SET active = 0 WHERE id = 1;
DELETE FROM users WHERE id = 1;
```

ON DUPLICATE KEY UPDATE:

```sql
INSERT INTO users (id, email)
VALUES (1, 'a@x')
ON DUPLICATE KEY UPDATE email = VALUES(email);
```

## Differences

- LOAD DATA INFILE is restricted; bulk loading maps to ScratchBird COPY if
  enabled.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
