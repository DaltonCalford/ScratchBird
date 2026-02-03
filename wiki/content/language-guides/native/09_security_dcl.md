# Security (DCL)

**Last Updated:** 2026-02-03

---

```sql
CREATE USER alice IDENTIFIED BY 'secret';
CREATE ROLE analyst;
GRANT SELECT ON app.users TO analyst;
GRANT analyst TO alice;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
