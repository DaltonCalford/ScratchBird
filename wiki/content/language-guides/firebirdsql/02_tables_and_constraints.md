# Tables and Constraints

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE TABLE | ScratchBird tracked | Catalog manager + DDL executor | Firebird-compatible syntax. |
| ALTER TABLE | ScratchBird tracked | DDL executor | Standard column/constraint operations supported. |
| DROP TABLE | ScratchBird tracked | Catalog manager + DDL executor | Drops metadata and storage. |
| PRIMARY KEY | ScratchBird tracked | Constraint metadata + index subsystem | Enforced by the engine. |
| UNIQUE | ScratchBird tracked | Constraint metadata + index subsystem | Enforced by the engine. |
| FOREIGN KEY | ScratchBird tracked | Constraint metadata + executor | Enforced by the engine. |
| CHECK | ScratchBird tracked | Expression validator + executor | Enforced at DML time. |
| NOT NULL | ScratchBird tracked | Column metadata + executor | Enforced at DML time. |

## Example

```sql
CREATE TABLE users (
    id BIGINT PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Differences

- Firebird storage page size and file layout options are stored but do not
  affect ScratchBird storage.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
