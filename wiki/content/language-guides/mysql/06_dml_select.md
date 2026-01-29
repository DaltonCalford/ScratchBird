[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Data Query Language (SELECT)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers the SELECT statement and related query operations in MySQL emulation mode. The SELECT statement retrieves data from one or more tables with support for filtering, joining, grouping, sorting, and limiting results.

**Important:** Basic SELECT statements (single-table, WHERE, GROUP BY, HAVING, ORDER BY, LIMIT) are implemented and emit correct bytecode. Complex multi-table joins are parsed but skipped during bytecode emission. Window functions and CTEs are not yet supported.

---

## SELECT Statement

Retrieves rows from one or more tables.

### Syntax

```sql
SELECT [ALL | DISTINCT | DISTINCTROW]
    select_list
FROM table_references
[WHERE where_condition]
[GROUP BY {col_name | expr | position} [ASC | DESC], ... [WITH ROLLUP]]
[HAVING where_condition]
[ORDER BY {col_name | expr | position} [ASC | DESC], ...]
[LIMIT {[offset,] row_count | row_count OFFSET offset}]
```

### Basic SELECT

**Select all columns:**
```sql
SELECT * FROM users;
```

**Select specific columns:**
```sql
SELECT id, name, email FROM users;
```

**Select with column aliases:**
```sql
SELECT
    id AS user_id,
    CONCAT(first_name, ' ', last_name) AS full_name,
    email AS contact_email
FROM users;
```

**Select with literal values:**
```sql
SELECT
    id,
    name,
    'active' AS status,
    100 AS default_credit
FROM users;
```

### DISTINCT

Eliminates duplicate rows from results.

**Examples:**
```sql
-- Get unique countries:
SELECT DISTINCT country FROM users;

-- Get unique combinations:
SELECT DISTINCT country, city FROM users;

-- DISTINCTROW is synonym for DISTINCT:
SELECT DISTINCTROW category FROM products;
```

**Notes:**
- DISTINCT applies to entire row, not individual columns
- Can impact performance on large datasets
- NULL values are considered equal for DISTINCT purposes

### WHERE Clause

Filters rows based on conditions.

**Comparison operators:**
```sql
SELECT * FROM products WHERE price > 100;
SELECT * FROM products WHERE stock = 0;
SELECT * FROM products WHERE name <> 'Widget';
SELECT * FROM users WHERE age >= 18;
```

**Logical operators:**
```sql
SELECT * FROM products
WHERE category = 'Electronics' AND price < 500;

SELECT * FROM users
WHERE country = 'USA' OR country = 'Canada';

SELECT * FROM products
WHERE NOT (category = 'Discontinued');
```

**BETWEEN:**
```sql
SELECT * FROM orders
WHERE order_date BETWEEN '2024-01-01' AND '2024-12-31';

SELECT * FROM products
WHERE price BETWEEN 50 AND 200;
```

**IN:**
```sql
SELECT * FROM users
WHERE country IN ('USA', 'Canada', 'Mexico');

SELECT * FROM orders
WHERE status IN ('pending', 'processing');
```

**LIKE pattern matching:**
```sql
-- Starts with 'John':
SELECT * FROM users WHERE name LIKE 'John%';

-- Ends with 'son':
SELECT * FROM users WHERE name LIKE '%son';

-- Contains 'tech':
SELECT * FROM products WHERE description LIKE '%tech%';

-- Exactly 5 characters:
SELECT * FROM codes WHERE code LIKE '_____';
```

**Regular expressions:**
```sql
SELECT * FROM users WHERE email REGEXP '^[a-z]+@example\.com$';
SELECT * FROM products WHERE sku RLIKE '^[A-Z]{3}-[0-9]{4}$';
```

**NULL checks:**
```sql
SELECT * FROM users WHERE middle_name IS NULL;
SELECT * FROM orders WHERE shipped_date IS NOT NULL;
```

### Joins

Combine rows from multiple tables.

**INNER JOIN:**
```sql
SELECT
    users.name,
    orders.order_id,
    orders.total
FROM users
INNER JOIN orders ON users.id = orders.customer_id;
```

**LEFT JOIN:**
```sql
SELECT
    users.name,
    COALESCE(COUNT(orders.id), 0) AS order_count
FROM users
LEFT JOIN orders ON users.id = orders.customer_id
GROUP BY users.id, users.name;
```

**RIGHT JOIN:**
```sql
SELECT
    products.name,
    categories.category_name
FROM products
RIGHT JOIN categories ON products.category_id = categories.id;
```

**CROSS JOIN:**
```sql
SELECT
    colors.name AS color,
    sizes.name AS size
FROM colors
CROSS JOIN sizes;
```

**Multiple joins:**
```sql
SELECT
    customers.name AS customer,
    orders.order_date,
    products.name AS product,
    order_items.quantity
FROM customers
JOIN orders ON customers.id = orders.customer_id
JOIN order_items ON orders.id = order_items.order_id
JOIN products ON order_items.product_id = products.id
WHERE orders.status = 'completed';
```

**Self join:**
```sql
SELECT
    e1.name AS employee,
    e2.name AS manager
FROM employees e1
LEFT JOIN employees e2 ON e1.manager_id = e2.id;
```

**USING clause:**
```sql
SELECT *
FROM users
JOIN orders USING (user_id);
```

**NATURAL JOIN:**
```sql
SELECT * FROM users NATURAL JOIN user_profiles;
```

**STRAIGHT_JOIN (MySQL specific):**
```sql
SELECT * FROM t1 STRAIGHT_JOIN t2 ON t1.id = t2.id;
```

### GROUP BY

Groups rows sharing a common value.

**Basic grouping:**
```sql
SELECT
    category,
    COUNT(*) AS product_count
FROM products
GROUP BY category;
```

**Multiple columns:**
```sql
SELECT
    country,
    city,
    COUNT(*) AS user_count
FROM users
GROUP BY country, city;
```

**WITH ROLLUP:**
```sql
SELECT
    category,
    COUNT(*) AS total,
    SUM(price) AS total_value
FROM products
GROUP BY category WITH ROLLUP;
```

**Aggregate functions:**
```sql
SELECT
    category,
    COUNT(*) AS count,
    AVG(price) AS avg_price,
    MIN(price) AS min_price,
    MAX(price) AS max_price,
    SUM(stock * price) AS inventory_value
FROM products
GROUP BY category;
```

### HAVING Clause

Filters groups after aggregation.

**Examples:**
```sql
SELECT
    category,
    COUNT(*) AS product_count
FROM products
GROUP BY category
HAVING COUNT(*) > 5;

SELECT
    customer_id,
    SUM(total) AS total_spent
FROM orders
GROUP BY customer_id
HAVING SUM(total) > 1000;

SELECT
    YEAR(order_date) AS year,
    MONTH(order_date) AS month,
    COUNT(*) AS order_count
FROM orders
GROUP BY YEAR(order_date), MONTH(order_date)
HAVING COUNT(*) >= 100;
```

### ORDER BY

Sorts result rows.

**Ascending order (default):**
```sql
SELECT * FROM products ORDER BY price;
SELECT * FROM users ORDER BY name ASC;
```

**Descending order:**
```sql
SELECT * FROM products ORDER BY price DESC;
SELECT * FROM orders ORDER BY order_date DESC;
```

**Multiple columns:**
```sql
SELECT * FROM products
ORDER BY category ASC, price DESC;
```

**By column position:**
```sql
SELECT name, price FROM products
ORDER BY 2 DESC, 1 ASC;
```

**By expression:**
```sql
SELECT first_name, last_name
FROM users
ORDER BY CONCAT(last_name, ', ', first_name);

SELECT product_name, price, stock
FROM products
ORDER BY price * stock DESC;
```

**NULL handling:**
```sql
SELECT * FROM users ORDER BY last_login DESC NULLS LAST;
SELECT * FROM products ORDER BY discount_rate ASC NULLS FIRST;
```

### LIMIT

Restricts number of rows returned.

**Limit rows:**
```sql
SELECT * FROM products LIMIT 10;
```

**Limit with offset:**
```sql
SELECT * FROM products LIMIT 20, 10;  -- Skip 20, return 10
SELECT * FROM products LIMIT 10 OFFSET 20;  -- Same as above
```

**Pagination:**
```sql
-- Page 1 (rows 1-10):
SELECT * FROM products ORDER BY id LIMIT 10 OFFSET 0;

-- Page 2 (rows 11-20):
SELECT * FROM products ORDER BY id LIMIT 10 OFFSET 10;

-- Page 3 (rows 21-30):
SELECT * FROM products ORDER BY id LIMIT 10 OFFSET 20;
```

### Subqueries

Queries nested within other queries.

**Scalar subquery:**
```sql
SELECT *
FROM products
WHERE price > (SELECT AVG(price) FROM products);
```

**IN subquery:**
```sql
SELECT *
FROM users
WHERE id IN (SELECT DISTINCT customer_id FROM orders);
```

**EXISTS subquery:**
```sql
SELECT *
FROM products p
WHERE EXISTS (
    SELECT 1 FROM order_items oi WHERE oi.product_id = p.id
);
```

**Correlated subquery:**
```sql
SELECT
    p.name,
    (SELECT COUNT(*) FROM order_items WHERE product_id = p.id) AS times_ordered
FROM products p;
```

**Subquery in FROM:**
```sql
SELECT category, avg_price
FROM (
    SELECT category, AVG(price) AS avg_price
    FROM products
    GROUP BY category
) AS category_averages
WHERE avg_price > 100;
```

### UNION

Combines results from multiple SELECT statements.

**UNION (removes duplicates):**
```sql
SELECT name FROM customers
UNION
SELECT name FROM suppliers;
```

**UNION ALL (keeps duplicates):**
```sql
SELECT product_id FROM order_items
UNION ALL
SELECT product_id FROM wishlist_items;
```

**With ORDER BY:**
```sql
(SELECT name, 'customer' AS type FROM customers)
UNION
(SELECT name, 'supplier' AS type FROM suppliers)
ORDER BY name;
```

---

## Known Limitations

### What Works

- **Single-table SELECT**: Basic SELECT with WHERE, GROUP BY, HAVING, ORDER BY, LIMIT emits correct bytecode (SELECT opcode, DISTINCT flag, WHERE_CLAUSE, GROUP_BY, HAVING, ORDER_BY, LIMIT opcodes)
- **DISTINCT / DISTINCTROW**: Properly emitted as flag byte
- **Subqueries**: Scalar, IN, EXISTS, and correlated subqueries are parsed
- **UNION / UNION ALL**: Parsed and emitted

### Partial Implementation

- **Multi-table joins**: Join tokens (JOIN, LEFT, RIGHT, INNER, CROSS, NATURAL, STRAIGHT_JOIN) are recognized, but complex multi-join FROM clauses are skipped during bytecode emission (the parser advances past join tokens without emitting join opcodes)
- **WITH ROLLUP**: Parsed but not emitted in bytecode

### Missing Features

- **Window functions**: Not supported in MySQL parser
- **Common Table Expressions (WITH)**: Not supported
- **SELECT INTO OUTFILE**: Not supported
- **FOR UPDATE / LOCK IN SHARE MODE**: Not fully implemented

### Spec Deltas

- **Join algorithm hints**: MySQL join hints (STRAIGHT_JOIN priority) may not affect execution
- **Query optimizer hints**: Optimizer hints are not supported
- **Full-text search**: MATCH ... AGAINST not implemented
