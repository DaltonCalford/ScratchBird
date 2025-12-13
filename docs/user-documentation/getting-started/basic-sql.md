# Basic SQL Operations

Learn essential SQL operations in ScratchBird.

[Back to Getting Started](index.md) | [Back to Documentation Index](../index.md)

---

## Prerequisites

- Connected to a ScratchBird database
- Basic understanding of SQL concepts

---

## Creating Tables

### Basic Table

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### With Auto-Increment

```sql
-- Using SERIAL (PostgreSQL style)
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10,2)
);

-- Using GENERATED (SQL standard)
CREATE TABLE orders (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    total DECIMAL(10,2)
);
```

### Common Data Types

| Type | Description | Example |
|------|-------------|---------|
| `INTEGER` | Whole numbers | `42` |
| `BIGINT` | Large integers | `9223372036854775807` |
| `DECIMAL(p,s)` | Exact numeric | `123.45` |
| `VARCHAR(n)` | Variable text | `'Hello'` |
| `TEXT` | Unlimited text | Long content |
| `BOOLEAN` | True/false | `TRUE`, `FALSE` |
| `DATE` | Date only | `'2024-01-15'` |
| `TIMESTAMP` | Date and time | `'2024-01-15 10:30:00'` |
| `UUID` | Unique identifier | `'550e8400-e29b-...'` |
| `JSON` | JSON data | `'{"key": "value"}'` |

---

## Inserting Data

### Single Row

```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com');
```

### Multiple Rows

```sql
INSERT INTO users (id, name, email) VALUES
    (2, 'Bob', 'bob@example.com'),
    (3, 'Carol', 'carol@example.com'),
    (4, 'Dave', 'dave@example.com');
```

### With Auto-Generated ID

```sql
-- For SERIAL columns, omit the id
INSERT INTO products (name, price)
VALUES ('Widget', 9.99);

-- Get the generated ID
INSERT INTO products (name, price)
VALUES ('Gadget', 19.99)
RETURNING id;
```

### Insert from SELECT

```sql
INSERT INTO archived_users (id, name, email)
SELECT id, name, email FROM users
WHERE created_at < '2023-01-01';
```

---

## Querying Data

### Basic SELECT

```sql
-- All columns
SELECT * FROM users;

-- Specific columns
SELECT name, email FROM users;

-- With alias
SELECT name AS user_name, email AS contact FROM users;
```

### Filtering with WHERE

```sql
-- Equality
SELECT * FROM users WHERE id = 1;

-- Comparison
SELECT * FROM products WHERE price > 10;

-- Pattern matching
SELECT * FROM users WHERE name LIKE 'A%';

-- NULL check
SELECT * FROM users WHERE email IS NOT NULL;

-- IN list
SELECT * FROM users WHERE id IN (1, 2, 3);

-- BETWEEN
SELECT * FROM products WHERE price BETWEEN 10 AND 50;

-- Multiple conditions
SELECT * FROM users
WHERE name LIKE 'A%' AND created_at > '2024-01-01';
```

### Sorting with ORDER BY

```sql
-- Ascending (default)
SELECT * FROM products ORDER BY price;

-- Descending
SELECT * FROM products ORDER BY price DESC;

-- Multiple columns
SELECT * FROM users ORDER BY name ASC, created_at DESC;
```

### Limiting Results

```sql
-- First 10 rows
SELECT * FROM users LIMIT 10;

-- With offset (pagination)
SELECT * FROM users LIMIT 10 OFFSET 20;
```

---

## Updating Data

### Basic UPDATE

```sql
UPDATE users
SET email = 'alice.new@example.com'
WHERE id = 1;
```

### Update Multiple Columns

```sql
UPDATE users
SET name = 'Alice Smith',
    email = 'alice.smith@example.com'
WHERE id = 1;
```

### Update with Expression

```sql
UPDATE products
SET price = price * 1.10  -- 10% increase
WHERE category = 'electronics';
```

### Update with RETURNING

```sql
UPDATE users
SET name = 'Alice Smith'
WHERE id = 1
RETURNING *;
```

---

## Deleting Data

### Basic DELETE

```sql
DELETE FROM users WHERE id = 1;
```

### Delete Multiple Rows

```sql
DELETE FROM users
WHERE created_at < '2020-01-01';
```

### Delete All Rows

```sql
-- Delete all (keeps table structure)
DELETE FROM temp_data;

-- Faster for large tables
TRUNCATE TABLE temp_data;
```

---

## Joins

### Inner Join

```sql
SELECT users.name, orders.total
FROM users
INNER JOIN orders ON users.id = orders.user_id;
```

### Left Join

```sql
-- All users, even without orders
SELECT users.name, orders.total
FROM users
LEFT JOIN orders ON users.id = orders.user_id;
```

### Multiple Joins

```sql
SELECT
    users.name,
    orders.id AS order_id,
    products.name AS product_name
FROM users
JOIN orders ON users.id = orders.user_id
JOIN order_items ON orders.id = order_items.order_id
JOIN products ON order_items.product_id = products.id;
```

---

## Aggregations

### Basic Aggregates

```sql
-- Count
SELECT COUNT(*) FROM users;

-- Sum
SELECT SUM(price) FROM products;

-- Average
SELECT AVG(price) FROM products;

-- Min/Max
SELECT MIN(price), MAX(price) FROM products;
```

### GROUP BY

```sql
-- Count per category
SELECT category, COUNT(*) as count
FROM products
GROUP BY category;

-- Total per user
SELECT user_id, SUM(total) as total_spent
FROM orders
GROUP BY user_id;
```

### HAVING (Filter Groups)

```sql
SELECT user_id, SUM(total) as total_spent
FROM orders
GROUP BY user_id
HAVING SUM(total) > 100;
```

---

## Indexes

### Create Index

```sql
-- Basic index
CREATE INDEX idx_users_email ON users(email);

-- Unique index
CREATE UNIQUE INDEX idx_users_email_unique ON users(email);

-- Composite index
CREATE INDEX idx_orders_user_date ON orders(user_id, created_at);
```

### ScratchBird Index Types

ScratchBird supports 11 index types:

| Type | Best For |
|------|----------|
| `BTREE` | General purpose, range queries |
| `HASH` | Equality lookups |
| `GIN` | Full-text search, arrays, JSON |
| `GIST` | Spatial data, complex types |
| `BRIN` | Large tables with natural ordering |

```sql
-- Specify index type
CREATE INDEX idx_products_name
ON products USING HASH (name);

CREATE INDEX idx_docs_content
ON documents USING GIN (content);
```

### View Indexes

```sql
-- In sb_isql
\di

-- Or query
SELECT * FROM pg_indexes WHERE tablename = 'users';
```

---

## Transactions

### Basic Transaction

```sql
BEGIN;

INSERT INTO orders (user_id, total) VALUES (1, 99.99);
INSERT INTO order_items (order_id, product_id, quantity)
VALUES (currval('orders_id_seq'), 1, 2);

COMMIT;
```

### Rollback on Error

```sql
BEGIN;

UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;

-- If something went wrong
ROLLBACK;
```

### Savepoints

```sql
BEGIN;

INSERT INTO users (id, name) VALUES (10, 'Test');
SAVEPOINT before_update;

UPDATE users SET name = 'Wrong' WHERE id = 10;

-- Undo just the update
ROLLBACK TO before_update;

COMMIT;  -- Insert is kept
```

---

## Views

### Create View

```sql
CREATE VIEW active_users AS
SELECT id, name, email
FROM users
WHERE active = TRUE;
```

### Use View

```sql
SELECT * FROM active_users;
```

### Drop View

```sql
DROP VIEW active_users;
```

---

## Common Patterns

### Pagination

```sql
-- Page 3 with 20 items per page
SELECT * FROM products
ORDER BY id
LIMIT 20 OFFSET 40;
```

### Upsert (Insert or Update)

```sql
INSERT INTO settings (key, value)
VALUES ('theme', 'dark')
ON CONFLICT (key) DO UPDATE SET value = EXCLUDED.value;
```

### Conditional Update

```sql
UPDATE products
SET status = CASE
    WHEN quantity = 0 THEN 'out_of_stock'
    WHEN quantity < 10 THEN 'low_stock'
    ELSE 'in_stock'
END;
```

### Subquery

```sql
SELECT * FROM users
WHERE id IN (
    SELECT user_id FROM orders
    WHERE total > 100
);
```

---

## Getting Help

### In sb_isql

```sql
-- List tables
\dt

-- Describe table
\d users

-- Show indexes
\di

-- Show all commands
\?
```

### SQL Reference

For complete SQL syntax, see the [Language Guide](../language-guide/index.md).

---

## Next Steps

Now you know basic SQL:

1. **Learn more SQL** - [Language Guide](../language-guide/index.md)
2. **Optimize queries** - [Performance Tuning](../admin/performance-tuning.md)
3. **Try tutorials** - [Tutorials](tutorials/web-app-backend.md)
