# Delphi Desktop App

**Last Updated:** 2026-02-03

---

## Goal

Connect a Delphi application to ScratchBird and perform CRUD operations.

---

## Step 1: Schema

```sql
CREATE SCHEMA app;
CREATE TABLE app.contacts (
    id INTEGER PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    email VARCHAR(255)
);
```

---

## Step 2: Connection Details

- Host: localhost
- Port: 3092 (native) or 5432 (PostgreSQL)
- Database: app
- User: SYSARCH
- Password: ScratchBirdBeta1!

---

## Step 3: UI Workflow

1. Open a connection.
2. Query `app.contacts` into a dataset.
3. Bind the dataset to grid and form controls.
4. Use INSERT/UPDATE/DELETE for edits.
5. Wrap multi‑step updates in a transaction.

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
