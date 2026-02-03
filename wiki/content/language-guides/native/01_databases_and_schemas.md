# Databases and Schemas

**Last Updated:** 2026-02-03

---

ScratchBird separates physical **databases** from logical **schemas**.
Schemas are recursive and can contain sub‑schemas.

## Create a Database

```sql
CREATE DATABASE myapp;
```

## Create Schemas

```sql
CREATE SCHEMA app;
CREATE SCHEMA app.reporting;
```

## Drop

```sql
DROP SCHEMA app.reporting;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
