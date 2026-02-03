# Basic SQL

**Last Updated:** 2026-02-03

---

## Overview

This guide shows basic SQL operations using the native V2 dialect: create tables,
insert rows, query, update, and delete.

---

## Create a Database

```sql
CREATE DATABASE myapp;
```

---

## Create a Table

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

---

## Insert Data

```sql
INSERT INTO users (id, username, email)
VALUES (1, 'alice', 'alice@example.com');

INSERT INTO users (id, username, email) VALUES
    (2, 'bob', 'bob@example.com'),
    (3, 'carol', 'carol@example.com');
```

---

## Query Data

```sql
SELECT * FROM users;

SELECT username, email
FROM users
WHERE email LIKE '%@example.com'
ORDER BY username;
```

---

## Update Data

```sql
UPDATE users
SET email = 'alice@newdomain.com'
WHERE id = 1;
```

---

## Delete Data

```sql
DELETE FROM users
WHERE id = 3;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
