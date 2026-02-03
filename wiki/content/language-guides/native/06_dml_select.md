# DML: SELECT

**Last Updated:** 2026-02-03

---

```sql
SELECT u.id, u.email, o.total
FROM app.users u
JOIN app.orders o ON o.user_id = u.id
WHERE o.total > 100
ORDER BY o.total DESC
LIMIT 50;
```

Supports JOIN, GROUP BY, HAVING, window functions, and subqueries.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
