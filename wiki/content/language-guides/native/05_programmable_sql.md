# Programmable SQL

**Last Updated:** 2026-02-03

---

ScratchBird supports procedures, functions, and triggers compiled to SBLR.

```sql
CREATE PROCEDURE app.recalc_stats()
BEGIN
    -- procedure body
END;
```

Trigger example:

```sql
CREATE TRIGGER app.users_bi
BEFORE INSERT ON app.users
FOR EACH ROW
BEGIN
    -- set defaults
END;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
