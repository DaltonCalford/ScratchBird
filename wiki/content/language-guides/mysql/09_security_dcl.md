# Security (DCL)

**Last Updated:** 2026-02-03

---

```sql
CREATE USER 'app' IDENTIFIED BY 'secret';
GRANT SELECT ON app.users TO 'app';
REVOKE SELECT ON app.users FROM 'app';
```

## Differences

- MySQL‑specific authentication plugins are mapped to ScratchBird auth methods
  where possible.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
