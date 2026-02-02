# SQL Syntax Reference

**Last Updated:** 2026-01-30

---

## Overview

ScratchBird provides a native SQL surface plus emulated Firebird, PostgreSQL, and MySQL dialects. This reference covers common SQL syntax that works across all dialects.

---

## SQL Dialects

ScratchBird supports multiple SQL dialects depending on the connection port:

| Port | Dialect | Documentation |
|------|---------|---------------|
| 3092 | Native ScratchBird | [Native Guide](../language-guides/native/README.md) |
| 5432 | PostgreSQL | [PostgreSQL Guide](../language-guides/postgresql/README.md) |
| 3306 | MySQL | [MySQL Guide](../language-guides/mysql/README.md) |
| 3050 | Firebird | [Firebird Guide](../language-guides/firebirdsql/README.md) |

---

## Data Definition (DDL)

### CREATE DATABASE

```sql
CREATE DATABASE database_name;
CREATE DATABASE IF NOT EXISTS database_name;
```

### CREATE SCHEMA

```sql
CREATE SCHEMA schema_name;
CREATE SCHEMA IF NOT EXISTS schema_name;
```

### CREATE TABLE

```sql
CREATE TABLE table_name (
    column_name data_type [constraints],
    ...
    [table_constraints]
);

-- Example
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### CREATE INDEX

```sql
CREATE INDEX index_name ON table_name (column_list);
CREATE UNIQUE INDEX index_name ON table_name (column_list);
CREATE INDEX index_name ON table_name USING method (column_list);

-- Index methods: BTREE, HASH, BITMAP, GIN, GIST, SPGIST
CREATE INDEX idx_name ON users USING BTREE (name);
CREATE INDEX idx_status ON orders USING BITMAP (status);
```

### ALTER TABLE

```sql
ALTER TABLE table_name ADD COLUMN column_name data_type;
ALTER TABLE table_name DROP COLUMN column_name;
ALTER TABLE table_name RENAME COLUMN old_name TO new_name;
ALTER TABLE table_name ALTER COLUMN column_name SET DEFAULT value;
ALTER TABLE table_name ADD CONSTRAINT constraint_name constraint_def;
```

### DROP

```sql
DROP TABLE table_name;
DROP TABLE IF EXISTS table_name;
DROP INDEX index_name;
DROP DATABASE database_name;
```

---

## Data Manipulation (DML)

### SELECT

```sql
SELECT [DISTINCT] column_list
FROM table_name
[WHERE condition]
[GROUP BY column_list]
[HAVING condition]
[ORDER BY column_list [ASC|DESC]]
[LIMIT count [OFFSET offset]];

-- Examples
SELECT * FROM users;
SELECT id, name FROM users WHERE active = true;
SELECT status, COUNT(*) FROM orders GROUP BY status;
SELECT * FROM products ORDER BY price DESC LIMIT 10;
```

### INSERT

```sql
INSERT INTO table_name (column_list) VALUES (value_list);
INSERT INTO table_name (column_list) VALUES (values1), (values2), ...;
INSERT INTO table_name SELECT ...;

-- Examples
INSERT INTO users (name, email) VALUES ('John', 'john@example.com');
INSERT INTO logs (message) SELECT error_msg FROM errors WHERE level = 'CRITICAL';
```

### UPDATE

```sql
UPDATE table_name SET column = value [, ...] [WHERE condition];

-- Examples
UPDATE users SET name = 'Jane' WHERE id = 1;
UPDATE products SET price = price * 1.1 WHERE category = 'electronics';
```

### DELETE

```sql
DELETE FROM table_name [WHERE condition];

-- Examples
DELETE FROM users WHERE id = 1;
DELETE FROM sessions WHERE expires_at < NOW();
```

---

## Joins

```sql
-- INNER JOIN
SELECT * FROM orders o
INNER JOIN users u ON o.user_id = u.id;

-- LEFT OUTER JOIN
SELECT * FROM users u
LEFT JOIN orders o ON u.id = o.user_id;

-- RIGHT OUTER JOIN
SELECT * FROM orders o
RIGHT JOIN users u ON o.user_id = u.id;

-- FULL OUTER JOIN
SELECT * FROM table1 t1
FULL OUTER JOIN table2 t2 ON t1.id = t2.id;

-- CROSS JOIN
SELECT * FROM colors CROSS JOIN sizes;
```

---

## Subqueries

```sql
-- Subquery in WHERE
SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE total > 100);

-- Subquery in FROM (derived table)
SELECT * FROM (SELECT id, name FROM users WHERE active = true) AS active_users;

-- Correlated subquery
SELECT * FROM products p
WHERE price > (SELECT AVG(price) FROM products WHERE category = p.category);

-- EXISTS
SELECT * FROM users u
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.user_id = u.id);
```

---

## Common Table Expressions (CTE)

```sql
WITH cte_name AS (
    SELECT ...
)
SELECT * FROM cte_name;

-- Multiple CTEs
WITH
    active_users AS (SELECT * FROM users WHERE active = true),
    recent_orders AS (SELECT * FROM orders WHERE created_at > NOW() - INTERVAL '7 days')
SELECT * FROM active_users u
JOIN recent_orders o ON u.id = o.user_id;

-- Recursive CTE
WITH RECURSIVE hierarchy AS (
    SELECT id, parent_id, name, 1 AS level
    FROM categories WHERE parent_id IS NULL
    UNION ALL
    SELECT c.id, c.parent_id, c.name, h.level + 1
    FROM categories c
    JOIN hierarchy h ON c.parent_id = h.id
)
SELECT * FROM hierarchy;
```

---

## Transactions

```sql
BEGIN;
-- or
START TRANSACTION;

-- SQL statements here

COMMIT;
-- or
ROLLBACK;

-- Savepoints
SAVEPOINT savepoint_name;
ROLLBACK TO SAVEPOINT savepoint_name;
RELEASE SAVEPOINT savepoint_name;
```

---

## Constraints

### Column Constraints

```sql
column_name data_type PRIMARY KEY
column_name data_type NOT NULL
column_name data_type UNIQUE
column_name data_type DEFAULT value
column_name data_type CHECK (condition)
column_name data_type REFERENCES other_table(column)
```

### Table Constraints

```sql
PRIMARY KEY (column_list)
UNIQUE (column_list)
CHECK (condition)
FOREIGN KEY (column) REFERENCES other_table(column)
    [ON DELETE action]
    [ON UPDATE action]
```

### Foreign Key Actions

```sql
ON DELETE CASCADE    -- Delete referencing rows
ON DELETE SET NULL   -- Set to NULL
ON DELETE RESTRICT   -- Prevent deletion
ON DELETE NO ACTION  -- Same as RESTRICT
ON UPDATE CASCADE    -- Update referencing rows
```

---

## Data Types Summary

| Type | Description |
|------|-------------|
| `INT`, `INTEGER` | 32-bit integer |
| `BIGINT` | 64-bit integer |
| `SMALLINT` | 16-bit integer |
| `DECIMAL(p,s)`, `NUMERIC(p,s)` | Exact decimal |
| `REAL`, `FLOAT` | Floating point |
| `DOUBLE PRECISION` | Double precision float |
| `BOOLEAN` | True/false |
| `CHAR(n)` | Fixed-length string |
| `VARCHAR(n)` | Variable-length string |
| `TEXT` | Unlimited text |
| `DATE` | Date only |
| `TIME` | Time only |
| `TIMESTAMP` | Date and time |
| `INTERVAL` | Time interval |
| `UUID` | UUID type |
| `JSON`, `JSONB` | JSON data |
| `BYTEA`, `BLOB` | Binary data |
| `ARRAY` | Array type |

See [Data Types Reference](Data-Types.md) for complete documentation.

---

## Operators Summary

| Category | Operators |
|----------|-----------|
| Comparison | `=`, `<>`, `!=`, `<`, `>`, `<=`, `>=` |
| Logical | `AND`, `OR`, `NOT` |
| Arithmetic | `+`, `-`, `*`, `/`, `%` |
| String | `\|\|` (concatenation), `LIKE`, `ILIKE` |
| NULL | `IS NULL`, `IS NOT NULL` |
| Range | `BETWEEN`, `IN` |
| Pattern | `LIKE`, `SIMILAR TO`, `~` (regex) |

See [Operators Reference](Operators.md) for complete documentation.

---

## Dialect-Specific Syntax

### PostgreSQL-Style

```sql
-- Type casting
SELECT '123'::INT;
SELECT CAST('123' AS INT);

-- Dollar-quoted strings
SELECT $$This is a string$$;

-- Array syntax
SELECT ARRAY[1, 2, 3];
SELECT array_column[1];
```

### MySQL-Style

```sql
-- Backtick identifiers
SELECT `column` FROM `table`;

-- SHOW commands
SHOW TABLES;
SHOW DATABASES;
SHOW COLUMNS FROM table_name;

-- AUTO_INCREMENT
CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY);
```

### Firebird-Style

```sql
-- Generators
CREATE GENERATOR gen_id;
SELECT GEN_ID(gen_id, 1) FROM RDB$DATABASE;

-- EXECUTE BLOCK
EXECUTE BLOCK AS
BEGIN
    -- statements
END;
```

---

## Related Documents

- [Native SQL Guide](../language-guides/native/README.md)
- [PostgreSQL Emulation](../language-guides/postgresql/README.md)
- [MySQL Emulation](../language-guides/mysql/README.md)
- [Firebird Emulation](../language-guides/firebirdsql/README.md)
- [Functions Reference](Functions.md)
- [Operators Reference](Operators.md)
- [Data Types Reference](Data-Types.md)
