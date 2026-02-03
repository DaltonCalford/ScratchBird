# Tables and Constraints

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE TABLE | ScratchBird tracked | Catalog manager + DDL executor | Creates a ScratchBird table with PostgreSQL-compatible syntax. |
| ALTER TABLE | ScratchBird tracked | DDL executor | Standard column and constraint operations are supported; storage-specific options may be ignored. |
| DROP TABLE | ScratchBird tracked | Catalog manager + DDL executor | Drops ScratchBird table metadata and storage. |
| PRIMARY KEY | ScratchBird tracked | Constraint metadata + index subsystem | Enforced by the engine. |
| UNIQUE | ScratchBird tracked | Constraint metadata + index subsystem | Enforced by the engine. |
| FOREIGN KEY | ScratchBird tracked | Constraint metadata + executor | Enforced by the engine. |
| CHECK | ScratchBird tracked | Expression validator + executor | Enforced at DML time. |
| NOT NULL | ScratchBird tracked | Column metadata + executor | Enforced at DML time. |
| UNLOGGED | Emulated (metadata-only) | Table metadata | Treated as a regular table in ScratchBird Alpha. |
| TABLESPACE | ScratchBird tracked | Tablespace metadata | Maps to ScratchBird tablespaces. |
| Storage parameters (WITH ...) | Emulated (metadata-only) | Table metadata | Options are stored but not guaranteed to affect storage. |

## Example

```sql
CREATE TABLE users (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Differences

- PostgreSQL storage parameters (fillfactor, toast options, reloptions) are not
  guaranteed to affect ScratchBird storage.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
