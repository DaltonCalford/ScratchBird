# DML: SELECT

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| SELECT + projection | ScratchBird tracked | Planner + executor | Firebird-compatible syntax supported. |
| JOINs | ScratchBird tracked | Planner + executor | INNER/LEFT/RIGHT joins supported. |
| WHERE / ORDER BY / ROWS | ScratchBird tracked | Planner + executor | Firebird ROWS/OFFSET syntax mapped to LIMIT/OFFSET. |
| CTEs (WITH) | ScratchBird tracked | Planner + executor | Recursive CTEs supported where ScratchBird supports recursion. |
| Window functions | ScratchBird tracked | Executor | Supported when ScratchBird implements the function. |

## Example

```sql
SELECT u.id, u.email, o.total
FROM users u
JOIN orders o ON o.user_id = u.id
WHERE o.total > 100
ORDER BY o.total DESC
ROWS 50;
```

## Differences

- Planner behavior and cost models differ from Firebird.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
