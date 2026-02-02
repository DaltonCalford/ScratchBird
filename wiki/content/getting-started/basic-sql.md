# Basic SQL

**Last Updated:** 2026-01-30

---

## Overview

This guide teaches fundamental SQL operations in ScratchBird: creating tables, inserting data, querying, updating, and deleting records. These operations form the foundation of database interaction.

---

## Prerequisites

- ScratchBird server running
- Connected via any method (see [First Connection](first-connection.md))

All examples use the native SQL dialect. The same queries work through PostgreSQL and MySQL protocols with minor syntax variations.

---

## Creating a Database

Before creating tables, you may want to create a dedicated database:

```sql
-- Create a new database
CREATE DATABASE myapp;

-- List all databases
SELECT * FROM sb_catalog.databases;

-- Connect to the new database (in sb_isql)
\c myapp
```

---

## Creating Tables

### Basic Table Creation

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### With Auto-Incrementing ID

```sql
-- Using SERIAL (auto-increment)
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10, 2),
    in_stock BOOLEAN DEFAULT TRUE
);

-- Using GENERATED (SQL standard)
CREATE TABLE orders (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id INTEGER NOT NULL,
    total DECIMAL(10, 2),
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### With Foreign Keys

```sql
CREATE TABLE order_items (
    id SERIAL PRIMARY KEY,
    order_id INTEGER NOT NULL REFERENCES orders(id),
    product_id INTEGER NOT NULL REFERENCES products(id),
    quantity INTEGER NOT NULL DEFAULT 1,
    unit_price DECIMAL(10, 2) NOT NULL
);
```

### Verify Table Structure

```sql
-- List all tables
SELECT * FROM sb_catalog.tables;

-- Describe table (in sb_isql)
\d users

-- Or query columns directly
SELECT column_name, data_type, is_nullable
FROM sb_catalog.columns
WHERE table_name = 'users';
```

---

## Inserting Data

### Single Row Insert

```sql
INSERT INTO users (id, username, email)
VALUES (1, 'alice', 'alice@example.com');
```

### Multiple Rows

```sql
INSERT INTO users (id, username, email) VALUES
    (2, 'bob', 'bob@example.com'),
    (3, 'carol', 'carol@example.com'),
    (4, 'dave', 'dave@example.com');
```

### Insert with Auto-Generated ID

```sql
-- SERIAL columns auto-generate IDs
INSERT INTO products (name, price) VALUES ('Widget', 19.99);
INSERT INTO products (name, price) VALUES ('Gadget', 29.99);
INSERT INTO products (name, price) VALUES ('Gizmo', 39.99);

-- Get the last inserted ID
SELECT lastval();

-- Or use RETURNING
INSERT INTO products (name, price)
VALUES ('Thingamajig', 49.99)
RETURNING id;
```

### Insert with Default Values

```sql
-- Uses default for created_at
INSERT INTO users (id, username, email)
VALUES (5, 'eve', 'eve@example.com');

-- Explicitly use DEFAULT
INSERT INTO products (name, price, in_stock)
VALUES ('Doohickey', 9.99, DEFAULT);
```

---

## Querying Data (SELECT)

### Select All Columns

```sql
SELECT * FROM users;
```

### Select Specific Columns

```sql
SELECT username, email FROM users;
```

### With Aliases

```sql
SELECT
    username AS "User Name",
    email AS "Email Address"
FROM users;
```

### Filtering with WHERE

```sql
-- Equality
SELECT * FROM users WHERE username = 'alice';

-- Comparison
SELECT * FROM products WHERE price > 20.00;

-- LIKE pattern matching
SELECT * FROM users WHERE email LIKE '%@example.com';

-- IN list
SELECT * FROM users WHERE username IN ('alice', 'bob', 'carol');

-- BETWEEN range
SELECT * FROM products WHERE price BETWEEN 10.00 AND 30.00;

-- NULL check
SELECT * FROM users WHERE email IS NOT NULL;

-- Multiple conditions
SELECT * FROM products
WHERE price < 50.00
  AND in_stock = TRUE;
```

### Sorting Results

```sql
-- Ascending (default)
SELECT * FROM products ORDER BY price;

-- Descending
SELECT * FROM products ORDER BY price DESC;

-- Multiple columns
SELECT * FROM users ORDER BY username ASC, created_at DESC;
```

### Limiting Results

```sql
-- First 10 rows
SELECT * FROM users LIMIT 10;

-- Skip first 5, then get 10
SELECT * FROM users LIMIT 10 OFFSET 5;

-- Pagination example (page 3, 10 per page)
SELECT * FROM users LIMIT 10 OFFSET 20;
```

### Aggregate Functions

```sql
-- Count rows
SELECT COUNT(*) FROM users;

-- Count non-null values
SELECT COUNT(email) FROM users;

-- Sum, Average, Min, Max
SELECT
    COUNT(*) AS total_products,
    SUM(price) AS total_value,
    AVG(price) AS average_price,
    MIN(price) AS cheapest,
    MAX(price) AS most_expensive
FROM products;
```

### Grouping Data

```sql
-- Count products by in_stock status
SELECT in_stock, COUNT(*) AS count
FROM products
GROUP BY in_stock;

-- Filter groups with HAVING
SELECT in_stock, COUNT(*) AS count
FROM products
GROUP BY in_stock
HAVING COUNT(*) > 5;
```

### Distinct Values

```sql
-- Unique usernames
SELECT DISTINCT username FROM users;

-- Count unique values
SELECT COUNT(DISTINCT username) FROM users;
```

---

## Updating Data

### Update Single Row

```sql
UPDATE users
SET email = 'alice.smith@example.com'
WHERE username = 'alice';
```

### Update Multiple Columns

```sql
UPDATE products
SET price = 24.99, in_stock = FALSE
WHERE name = 'Widget';
```

### Update Multiple Rows

```sql
-- Apply 10% discount to all products
UPDATE products
SET price = price * 0.90;

-- Update based on condition
UPDATE products
SET in_stock = FALSE
WHERE price > 100.00;
```

### Update with RETURNING

```sql
UPDATE users
SET email = 'bob.jones@example.com'
WHERE username = 'bob'
RETURNING *;
```

---

## Deleting Data

### Delete Specific Rows

```sql
DELETE FROM users WHERE username = 'dave';
```

### Delete Multiple Rows

```sql
DELETE FROM products WHERE in_stock = FALSE;
```

### Delete All Rows

```sql
-- Deletes all rows, keeps table structure
DELETE FROM users;

-- Faster for large tables, resets sequences
TRUNCATE TABLE users;
```

### Delete with RETURNING

```sql
DELETE FROM users
WHERE username = 'eve'
RETURNING *;
```

---

## Joins

### Sample Data Setup

```sql
-- Create related tables
CREATE TABLE departments (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

CREATE TABLE employees (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    department_id INTEGER REFERENCES departments(id),
    salary DECIMAL(10, 2)
);

-- Insert sample data
INSERT INTO departments (name) VALUES ('Engineering'), ('Sales'), ('Marketing');

INSERT INTO employees (name, department_id, salary) VALUES
    ('Alice', 1, 75000),
    ('Bob', 1, 80000),
    ('Carol', 2, 65000),
    ('Dave', 2, 70000),
    ('Eve', NULL, 55000);  -- No department
```

### Inner Join

Returns only matching rows from both tables:

```sql
SELECT e.name AS employee, d.name AS department
FROM employees e
INNER JOIN departments d ON e.department_id = d.id;
```

Result:
```
 employee | department
----------+-------------
 Alice    | Engineering
 Bob      | Engineering
 Carol    | Sales
 Dave     | Sales
```

### Left Join

Returns all rows from left table, matching rows from right:

```sql
SELECT e.name AS employee, d.name AS department
FROM employees e
LEFT JOIN departments d ON e.department_id = d.id;
```

Result:
```
 employee | department
----------+-------------
 Alice    | Engineering
 Bob      | Engineering
 Carol    | Sales
 Dave     | Sales
 Eve      | NULL
```

### Right Join

Returns all rows from right table, matching rows from left:

```sql
SELECT e.name AS employee, d.name AS department
FROM employees e
RIGHT JOIN departments d ON e.department_id = d.id;
```

### Full Outer Join

Returns all rows from both tables:

```sql
SELECT e.name AS employee, d.name AS department
FROM employees e
FULL OUTER JOIN departments d ON e.department_id = d.id;
```

---

## Subqueries

### Subquery in WHERE

```sql
-- Employees in Engineering
SELECT * FROM employees
WHERE department_id = (
    SELECT id FROM departments WHERE name = 'Engineering'
);

-- Products more expensive than average
SELECT * FROM products
WHERE price > (SELECT AVG(price) FROM products);
```

### Subquery in FROM

```sql
-- Average salary by department
SELECT d.name, avg_sal.avg_salary
FROM departments d
JOIN (
    SELECT department_id, AVG(salary) AS avg_salary
    FROM employees
    GROUP BY department_id
) avg_sal ON d.id = avg_sal.department_id;
```

### Subquery with IN

```sql
-- Departments with employees
SELECT * FROM departments
WHERE id IN (SELECT DISTINCT department_id FROM employees WHERE department_id IS NOT NULL);
```

### Subquery with EXISTS

```sql
-- Departments that have employees
SELECT * FROM departments d
WHERE EXISTS (
    SELECT 1 FROM employees e WHERE e.department_id = d.id
);
```

---

## Common Table Expressions (CTEs)

### Basic CTE

```sql
WITH high_earners AS (
    SELECT * FROM employees WHERE salary > 70000
)
SELECT * FROM high_earners;
```

### Multiple CTEs

```sql
WITH
    dept_stats AS (
        SELECT department_id, AVG(salary) AS avg_salary
        FROM employees
        GROUP BY department_id
    ),
    high_avg AS (
        SELECT * FROM dept_stats WHERE avg_salary > 70000
    )
SELECT d.name, ha.avg_salary
FROM high_avg ha
JOIN departments d ON d.id = ha.department_id;
```

---

## Transactions

### Basic Transaction

```sql
-- Start transaction
BEGIN;

-- Perform operations
INSERT INTO users (id, username, email) VALUES (10, 'frank', 'frank@example.com');
UPDATE users SET email = 'frank.updated@example.com' WHERE id = 10;

-- Commit changes
COMMIT;
```

### Rollback

```sql
BEGIN;

DELETE FROM users WHERE id = 10;

-- Oops, wrong user! Undo changes
ROLLBACK;
```

### Savepoints

```sql
BEGIN;

INSERT INTO users (id, username, email) VALUES (11, 'grace', 'grace@example.com');
SAVEPOINT sp1;

INSERT INTO users (id, username, email) VALUES (12, 'henry', 'henry@example.com');

-- Undo henry but keep grace
ROLLBACK TO SAVEPOINT sp1;

COMMIT;
```

---

## Indexes

### Create Index

```sql
-- Single column index
CREATE INDEX idx_users_email ON users(email);

-- Composite index
CREATE INDEX idx_employees_dept_salary ON employees(department_id, salary);

-- Unique index
CREATE UNIQUE INDEX idx_users_username ON users(username);
```

### List Indexes

```sql
SELECT * FROM sb_catalog.indexes WHERE table_name = 'users';
```

### Drop Index

```sql
DROP INDEX idx_users_email;
```

---

## Constraints

### Primary Key

```sql
-- At creation
CREATE TABLE example (
    id INTEGER PRIMARY KEY
);

-- Or named
CREATE TABLE example2 (
    id INTEGER,
    CONSTRAINT pk_example2 PRIMARY KEY (id)
);
```

### Foreign Key

```sql
CREATE TABLE child (
    id INTEGER PRIMARY KEY,
    parent_id INTEGER,
    CONSTRAINT fk_parent FOREIGN KEY (parent_id)
        REFERENCES parent(id)
        ON DELETE CASCADE
        ON UPDATE CASCADE
);
```

### Unique Constraint

```sql
ALTER TABLE users ADD CONSTRAINT unique_email UNIQUE (email);
```

### Check Constraint

```sql
ALTER TABLE products ADD CONSTRAINT positive_price CHECK (price > 0);
```

### Not Null

```sql
ALTER TABLE users ALTER COLUMN username SET NOT NULL;
```

---

## Altering Tables

### Add Column

```sql
ALTER TABLE users ADD COLUMN phone VARCHAR(20);
```

### Drop Column

```sql
ALTER TABLE users DROP COLUMN phone;
```

### Rename Column

```sql
ALTER TABLE users RENAME COLUMN username TO user_name;
```

### Change Column Type

```sql
ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(200);
```

### Rename Table

```sql
ALTER TABLE users RENAME TO app_users;
```

---

## Dropping Objects

```sql
-- Drop table
DROP TABLE users;

-- Drop table if exists (no error if missing)
DROP TABLE IF EXISTS users;

-- Drop table and dependent objects
DROP TABLE users CASCADE;

-- Drop database
DROP DATABASE myapp;
```

---

## Quick Reference

| Operation | SQL |
|-----------|-----|
| Create table | `CREATE TABLE name (columns)` |
| Insert row | `INSERT INTO table (cols) VALUES (vals)` |
| Select all | `SELECT * FROM table` |
| Select with filter | `SELECT * FROM table WHERE condition` |
| Update rows | `UPDATE table SET col=val WHERE condition` |
| Delete rows | `DELETE FROM table WHERE condition` |
| Join tables | `SELECT * FROM a JOIN b ON a.id = b.a_id` |
| Count rows | `SELECT COUNT(*) FROM table` |
| Group by | `SELECT col, COUNT(*) FROM table GROUP BY col` |
| Order by | `SELECT * FROM table ORDER BY col DESC` |
| Limit results | `SELECT * FROM table LIMIT 10` |

---

## Next Steps

- [Native SQL Guide](../language-guides/native/README.md) - Full SQL reference
- [Data Types](../reference/Data-Types.md) - Complete data type reference
- [Functions](../reference/Functions.md) - Built-in functions
- [Tutorials](../tutorials/README.md) - Build real applications

