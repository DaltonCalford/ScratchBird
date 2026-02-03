# Indexes, Views, Sequences

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE INDEX | ScratchBird tracked | Catalog manager + index subsystem | Standard index creation with PostgreSQL syntax. |
| UNIQUE INDEX | ScratchBird tracked | Catalog manager + index subsystem | Enforced by the engine. |
| Expression indexes | ScratchBird tracked | Expression compiler + index subsystem | Supported when expressions are compatible with ScratchBird. |
| Partial indexes | Emulated | Planner compatibility layer | Accepted when predicate is supported; may be planned differently. |
| Index methods | ScratchBird tracked | Index subsystem | BTREE/HASH/GIN/GIST/BRIN/SPGIST/HNSW/IVF/BITMAP/COLUMNSTORE/FULLTEXT/INVERTED as implemented by ScratchBird. |
| CREATE VIEW | ScratchBird tracked | Catalog manager + view executor | Views stored as catalog definitions. |
| CREATE SEQUENCE | ScratchBird tracked | Sequence manager | Sequences stored as ScratchBird objects. |

## Examples

```sql
CREATE INDEX idx_users_email ON users (email);
CREATE UNIQUE INDEX idx_users_email_u ON users (email);
CREATE INDEX idx_users_lower_email ON users ((lower(email)));

CREATE VIEW active_users AS SELECT * FROM users WHERE active = TRUE;

CREATE SEQUENCE order_seq START 1;
SELECT nextval('order_seq');
```

## Differences

- PostgreSQL extension-provided access methods are not available unless
  ScratchBird implements them directly.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
