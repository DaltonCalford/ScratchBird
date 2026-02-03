# Indexes, Views, Sequences

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE INDEX | ScratchBird tracked | Catalog manager + index subsystem | Firebird-compatible syntax. |
| UNIQUE INDEX | ScratchBird tracked | Catalog manager + index subsystem | Enforced by the engine. |
| DESCENDING INDEX | Emulated | Index subsystem | Accepted when ScratchBird supports descending keys. |
| CREATE VIEW | ScratchBird tracked | Catalog manager + view executor | Views stored as catalog definitions. |
| GENERATOR/SEQUENCE | ScratchBird tracked | Sequence manager | Firebird generators map to ScratchBird sequences. |

## Examples

```sql
CREATE INDEX idx_users_email ON users (email);
CREATE UNIQUE INDEX idx_users_email_u ON users (email);

CREATE VIEW active_users AS SELECT * FROM users WHERE active = 1;

CREATE SEQUENCE order_seq;
SELECT NEXT VALUE FOR order_seq FROM RDB$DATABASE;
```

## Differences

- Firebird index expression options are supported when ScratchBird’s index
  subsystem implements the equivalent behavior.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
