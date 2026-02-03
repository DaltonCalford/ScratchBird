# DML: SELECT

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| SELECT + projection | ScratchBird tracked | Planner + executor | Standard PostgreSQL syntax supported. |
| JOINs | ScratchBird tracked | Planner + executor | INNER/LEFT/RIGHT/FULL joins supported by ScratchBird. |
| WHERE / ORDER BY / LIMIT | ScratchBird tracked | Planner + executor | PostgreSQL-compatible semantics; planner differs. |
| CTEs (WITH) | ScratchBird tracked | Planner + executor | Recursive CTEs supported where ScratchBird supports recursion. |
| Window functions | ScratchBird tracked | Executor | Supported when ScratchBird implements the window function. |
| Subqueries | ScratchBird tracked | Planner + executor | Standard subquery forms supported. |

## Example

```sql
SELECT u.id, u.email, o.total
FROM users u
JOIN orders o ON o.user_id = u.id
WHERE o.total > 100
ORDER BY o.total DESC
LIMIT 50 OFFSET 0;
```

CTEs and window functions:

```sql
WITH ranked AS (
    SELECT id, total, ROW_NUMBER() OVER (ORDER BY total DESC) AS rn
    FROM orders
)
SELECT * FROM ranked WHERE rn <= 10;
```

## Differences

- Planner behavior and cost models differ from PostgreSQL.
- PostgreSQL-specific plan hints or extensions are not supported.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
