# REST API Design

**Last Updated:** 2026-02-03

---

## Goals

Design REST endpoints that map cleanly to ScratchBird SQL and transaction
boundaries.

---

## Patterns

- Use transactions for multi‑statement operations.
- Prefer explicit pagination (limit/offset or keyset).
- Map SQLSTATE codes to HTTP errors.
- Validate inputs before SQL execution.
- Avoid long‑running transactions in request handlers.

### Example: Pagination

```sql
SELECT * FROM app.orders
ORDER BY id
LIMIT 50 OFFSET 100;
```

### Example: Transaction Boundary

```sql
BEGIN;
UPDATE app.accounts SET balance = balance - 10 WHERE id = 1;
UPDATE app.accounts SET balance = balance + 10 WHERE id = 2;
COMMIT;
```

### Example: Error Mapping

- 23505 (unique violation) -> HTTP 409
- 23503 (foreign key) -> HTTP 409
- 22001 (data too long) -> HTTP 400

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
