# Security (DCL)

**Last Updated:** 2026-02-03

---

## Compatibility Matrix

| Feature | Status | Source | Notes |
|---------|--------|--------|-------|
| CREATE USER | ScratchBird tracked | Catalog manager | Users stored in ScratchBird catalog. |
| ALTER USER | ScratchBird tracked | Catalog manager | User metadata updates supported. |
| DROP USER | ScratchBird tracked | Catalog manager | User removal supported. |
| CREATE ROLE | ScratchBird tracked | Catalog manager | Roles stored in ScratchBird catalog. |
| GRANT/REVOKE | ScratchBird tracked | Privilege manager | Enforced by ScratchBird privilege system. |
| RDB$ADMIN role | Emulated | Privilege manager | Mapped to ScratchBird admin privileges. |

## Example

```sql
CREATE ROLE analyst;
CREATE USER alice PASSWORD 'secret';
GRANT SELECT ON users TO analyst;
GRANT analyst TO alice;
```

## Differences

- Firebird security database integration is emulated; ScratchBird uses its own
  privilege model.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
