# DML: INSERT/UPDATE/DELETE

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| INSERT | ScratchBird tracked | Executor | Standard INSERT syntax supported. |
| UPDATE | ScratchBird tracked | Executor | Standard UPDATE syntax supported. |
| DELETE | ScratchBird tracked | Executor | Standard DELETE syntax supported. |
| RETURNING | ScratchBird tracked | Executor | PostgreSQL RETURNING supported where ScratchBird provides it. |
| ON CONFLICT | ScratchBird tracked | Executor + planner | Supported when conflict target and action are compatible. |
| MERGE | Emulated | Planner + executor | Mapped to ScratchBird MERGE behavior. |
| COPY DML paths | Emulated | COPY handler | Bulk loaders map to ScratchBird COPY semantics. |

## Example

```sql
INSERT INTO users (email) VALUES ('a@x') RETURNING id;

UPDATE users SET active = FALSE WHERE id = 1 RETURNING id;

DELETE FROM users WHERE id = 1 RETURNING id;
```

ON CONFLICT:

```sql
INSERT INTO users (id, email)
VALUES (1, 'a@x')
ON CONFLICT (id) DO UPDATE SET email = EXCLUDED.email;
```

## Differences

- COPY file-path operations and PROGRAM execution are restricted.
- Bulk loaders map to ScratchBird COPY semantics; options may differ.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
