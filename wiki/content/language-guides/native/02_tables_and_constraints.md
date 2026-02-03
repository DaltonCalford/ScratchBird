# Tables and Constraints

**Last Updated:** 2026-02-03

---

## Create Table

```sql
CREATE TABLE app.users (
    id INTEGER PRIMARY KEY,
    email VARCHAR(255) UNIQUE NOT NULL,
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Constraints

- PRIMARY KEY
- UNIQUE
- FOREIGN KEY
- CHECK
- NOT NULL

```sql
CREATE TABLE app.orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES app.users(id),
    total DECIMAL(10,2) CHECK (total >= 0)
);
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
