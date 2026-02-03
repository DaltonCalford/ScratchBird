# DML: INSERT/UPDATE/DELETE

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| INSERT | ScratchBird tracked | Executor | Firebird-compatible INSERT syntax supported. |
| UPDATE | ScratchBird tracked | Executor | Firebird-compatible UPDATE syntax supported. |
| DELETE | ScratchBird tracked | Executor | Firebird-compatible DELETE syntax supported. |
| RETURNING | ScratchBird tracked | Executor | Firebird RETURNING supported where ScratchBird provides it. |
| MERGE | Emulated | Planner + executor | Mapped to ScratchBird MERGE behavior. |
| EXECUTE STATEMENT | Emulated | PSQL/runtime | Supported when ScratchBird PSQL provides equivalent behavior. |

## Example

```sql
INSERT INTO users (email) VALUES ('a@x') RETURNING id;

UPDATE users SET active = 0 WHERE id = 1 RETURNING id;

DELETE FROM users WHERE id = 1 RETURNING id;
```

## Differences

- Bulk loaders map to ScratchBird COPY semantics; options may differ.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
