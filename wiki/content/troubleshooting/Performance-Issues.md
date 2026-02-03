# Performance Issues

**Last Updated:** 2026-02-03

---

## Checklist

- Verify index usage with EXPLAIN.
- Check long‑running transactions.
- Monitor cache hit ratio and I/O.
- Identify large sequential scans.

## Example

```sql
EXPLAIN SELECT * FROM app.orders WHERE total > 100;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
