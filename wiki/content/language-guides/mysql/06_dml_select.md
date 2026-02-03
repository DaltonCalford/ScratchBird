# DML: SELECT

**Last Updated:** 2026-02-03

---

```sql
SELECT u.id, u.email, o.total
FROM users u
JOIN orders o ON o.user_id = u.id
WHERE o.total > 100
ORDER BY o.total DESC
LIMIT 50;
```

Limit with offset:

```sql
SELECT * FROM users LIMIT 20, 10;
```

## Differences

- Optimizer hints are accepted but may be ignored.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
