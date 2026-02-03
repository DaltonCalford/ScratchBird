# Security (DCL)

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE ROLE | ScratchBird tracked | Catalog manager | Role metadata stored in ScratchBird catalog. |
| CREATE USER | ScratchBird tracked | Catalog manager | User metadata stored in ScratchBird catalog. |
| GRANT/REVOKE on objects | ScratchBird tracked | Privilege manager | Enforced by ScratchBird privilege system. |
| GRANT role membership | ScratchBird tracked | Privilege manager | Role membership stored in ScratchBird catalog. |
| Role attributes (SUPERUSER, CREATEDB, CREATEROLE) | Emulated | Privilege manager | Mapped to ScratchBird privilege model; not identical to PostgreSQL. |
| DEFAULT PRIVILEGES | Emulated | Privilege manager | Applied when implemented; may differ from PostgreSQL. |

## Example

```sql
CREATE ROLE analyst;
CREATE USER alice;
GRANT SELECT ON users TO analyst;
GRANT analyst TO alice;
```

## Differences

- PostgreSQL role attributes map to ScratchBird’s privilege model and may not
  have identical semantics.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
