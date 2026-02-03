# First Application

**Last Updated:** 2026-02-03

---

## Goal

Build a small CRUD workflow using ScratchBird SQL and understand the basic
schema and query patterns.

---

## Step 1: Create Database and Schema

```sql
CREATE DATABASE app;
CREATE SCHEMA app;
```

---

## Step 2: Create Tables

```sql
CREATE TABLE app.users (
    id INTEGER PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE app.orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES app.users(id),
    total DECIMAL(10,2) CHECK (total >= 0),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

---

## Step 3: Insert Sample Data

```sql
INSERT INTO app.users (id, email) VALUES
    (1, 'a@x'),
    (2, 'b@x');

INSERT INTO app.orders (id, user_id, total) VALUES
    (1, 1, 12.50),
    (2, 2, 99.99);
```

---

## Step 4: Query Data

```sql
SELECT u.email, o.total
FROM app.users u
JOIN app.orders o ON o.user_id = u.id
ORDER BY o.total DESC;
```

---

## Step 5: Update and Delete

```sql
UPDATE app.users SET active = FALSE WHERE id = 2;
DELETE FROM app.orders WHERE id = 1;
```

---

## Step 6: Add Indexes

```sql
CREATE INDEX idx_orders_user_id ON app.orders (user_id);
```

---

## Step 7: Add a Simple View

```sql
CREATE VIEW app.active_users AS
SELECT * FROM app.users WHERE active = TRUE;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
