# Databases and Schemas

**Last Updated:** 2026-02-03

---

Firebird uses a single database namespace with object name qualifiers. ScratchBird
maps Firebird databases into its database root and emulated schema paths.

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE DATABASE | Emulated (metadata-only) | Catalog manager database metadata | Creates catalog entry; no Firebird data file layout is created. |
| ALTER DATABASE | Emulated (metadata-only) | Catalog manager database metadata | Options stored when applicable; storage-specific options ignored. |
| DROP DATABASE | ScratchBird tracked | Catalog manager database drop | Drops ScratchBird database metadata and storage. |
| SET SCHEMA / object qualifiers | ScratchBird tracked | Parser + resolver | Name resolution follows Firebird rules with ScratchBird schema mapping. |

## Example

```sql
CREATE DATABASE 'scratchbird:db';
```

## Differences

- Firebird does not have separate schemas; ScratchBird uses a recursive schema
  tree internally and maps Firebird object names onto that tree.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
