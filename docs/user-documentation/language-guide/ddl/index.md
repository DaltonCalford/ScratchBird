# Data Definition Language (DDL)

SQL commands for defining database schema.

[Back to Language Guide](../index.md)

---

## DDL Commands

| Command | Description |
|---------|-------------|
| [CREATE DATABASE](create-database.md) | Create a new database |
| [CREATE TABLE](create-table.md) | Create a new table |
| [CREATE INDEX](create-index.md) | Create an index |
| [CREATE VIEW](create-view.md) | Create a view |
| [ALTER TABLE](alter-table.md) | Modify a table |
| [DROP](drop-statements.md) | Remove objects |

---

## Quick Reference

```sql
-- Create table
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL
);

-- Add column
ALTER TABLE users ADD COLUMN email VARCHAR(255);

-- Create index
CREATE INDEX idx_users_email ON users(email);

-- Create view
CREATE VIEW active_users AS
SELECT * FROM users WHERE active = TRUE;

-- Drop table
DROP TABLE users;
```
