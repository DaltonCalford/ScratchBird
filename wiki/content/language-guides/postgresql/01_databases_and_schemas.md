# Databases and Schemas

**Last Updated:** 2026-02-03

---

PostgreSQL uses databases and schemas. ScratchBird maps these into its database
root and recursive schema tree.

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE DATABASE | Emulated (metadata-only) | Catalog manager database metadata | Creates a catalog entry; no PostgreSQL data directory is created. |
| ALTER DATABASE | Emulated (metadata-only) | Catalog manager database metadata | Options are stored when applicable; storage-specific options are ignored. |
| DROP DATABASE | ScratchBird tracked | Catalog manager database drop | Drops ScratchBird database metadata and storage; no PostgreSQL data directory. |
| CREATE SCHEMA | ScratchBird tracked | Catalog manager schema tree | Creates a schema node in the ScratchBird recursive schema tree. |
| ALTER SCHEMA | Emulated (metadata-only) | Catalog manager schema metadata | Renames and owner changes are stored when applicable. |
| DROP SCHEMA | ScratchBird tracked | Catalog manager schema drop | Removes schema metadata and contained objects per DROP rules. |
| SET search_path | ScratchBird tracked | Session context | Name resolution uses search path; recursive schemas are still allowed. |

## Examples

```sql
CREATE DATABASE app;
\c app

CREATE SCHEMA app;
SET search_path = app, public;
```

## Differences

- ScratchBird schemas are recursive (schemas can contain sub-schemas). PostgreSQL
  does not provide recursive schemas.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
