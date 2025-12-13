# Data Manipulation Language (DML)

SQL commands for querying and modifying data.

[Back to Language Guide](../index.md)

---

## DML Commands

| Command | Description |
|---------|-------------|
| [SELECT](select.md) | Query data |
| [INSERT](insert.md) | Add rows |
| [UPDATE](update.md) | Modify rows |
| [DELETE](delete.md) | Remove rows |
| [MERGE](merge.md) | Upsert operations |

---

## Quick Reference

```sql
-- Select
SELECT name, email FROM users WHERE active = TRUE;

-- Insert
INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com');

-- Update
UPDATE users SET email = 'new@example.com' WHERE id = 1;

-- Delete
DELETE FROM users WHERE id = 1;

-- Upsert
INSERT INTO users (id, name) VALUES (1, 'Bob')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;
```
