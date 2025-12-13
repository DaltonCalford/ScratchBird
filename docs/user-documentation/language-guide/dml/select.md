# SELECT

Query data from tables.

[Back to DML Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Syntax

```sql
SELECT [ DISTINCT ] columns
FROM table
[ WHERE condition ]
[ GROUP BY columns ]
[ HAVING condition ]
[ ORDER BY columns ]
[ LIMIT count [ OFFSET offset ] ];
```

---

## Select Columns

```sql
-- All columns
SELECT * FROM users;

-- Specific columns
SELECT name, email FROM users;

-- With alias
SELECT name AS user_name, email AS contact FROM users;

-- Expressions
SELECT name, price * quantity AS total FROM order_items;
```

---

## WHERE Clause

### Comparison

```sql
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE age > 18;
SELECT * FROM users WHERE name <> 'Admin';
```

### Pattern Matching

```sql
-- LIKE
SELECT * FROM users WHERE name LIKE 'A%';     -- Starts with A
SELECT * FROM users WHERE name LIKE '%son';   -- Ends with son
SELECT * FROM users WHERE name LIKE '%john%'; -- Contains john

-- ILIKE (case-insensitive)
SELECT * FROM users WHERE name ILIKE '%john%';

-- Regular expressions
SELECT * FROM users WHERE email ~ '^[a-z]+@';
```

### NULL Checks

```sql
SELECT * FROM users WHERE email IS NULL;
SELECT * FROM users WHERE email IS NOT NULL;
```

### IN and BETWEEN

```sql
SELECT * FROM users WHERE status IN ('active', 'pending');
SELECT * FROM products WHERE price BETWEEN 10 AND 100;
```

### Logical Operators

```sql
SELECT * FROM users WHERE active = TRUE AND verified = TRUE;
SELECT * FROM users WHERE role = 'admin' OR role = 'superuser';
SELECT * FROM users WHERE NOT deleted;
```

---

## ORDER BY

```sql
-- Ascending (default)
SELECT * FROM users ORDER BY name;

-- Descending
SELECT * FROM users ORDER BY created_at DESC;

-- Multiple columns
SELECT * FROM users ORDER BY last_name, first_name;

-- With NULLS placement
SELECT * FROM users ORDER BY email NULLS LAST;
```

---

## LIMIT and OFFSET

```sql
-- First 10 rows
SELECT * FROM users LIMIT 10;

-- Pagination (page 3, 20 per page)
SELECT * FROM users LIMIT 20 OFFSET 40;

-- Firebird style
SELECT FIRST 10 SKIP 40 * FROM users;
```

---

## DISTINCT

```sql
-- Unique values
SELECT DISTINCT country FROM users;

-- Multiple columns
SELECT DISTINCT city, country FROM users;

-- With ON
SELECT DISTINCT ON (user_id) * FROM orders ORDER BY user_id, created_at DESC;
```

---

## Joins

### INNER JOIN

```sql
SELECT u.name, o.total
FROM users u
INNER JOIN orders o ON u.id = o.user_id;
```

### LEFT JOIN

```sql
-- All users, even without orders
SELECT u.name, o.total
FROM users u
LEFT JOIN orders o ON u.id = o.user_id;
```

### RIGHT JOIN

```sql
SELECT u.name, o.total
FROM users u
RIGHT JOIN orders o ON u.id = o.user_id;
```

### FULL OUTER JOIN

```sql
SELECT u.name, o.total
FROM users u
FULL OUTER JOIN orders o ON u.id = o.user_id;
```

### CROSS JOIN

```sql
SELECT * FROM colors CROSS JOIN sizes;
```

### Self Join

```sql
SELECT e.name, m.name AS manager
FROM employees e
LEFT JOIN employees m ON e.manager_id = m.id;
```

### Multiple Joins

```sql
SELECT
    o.id,
    u.name AS customer,
    p.name AS product,
    oi.quantity
FROM orders o
JOIN users u ON o.user_id = u.id
JOIN order_items oi ON o.id = oi.order_id
JOIN products p ON oi.product_id = p.id;
```

---

## Aggregates

```sql
SELECT COUNT(*) FROM users;
SELECT SUM(total) FROM orders;
SELECT AVG(price) FROM products;
SELECT MIN(created_at), MAX(created_at) FROM users;
```

### GROUP BY

```sql
SELECT country, COUNT(*) AS user_count
FROM users
GROUP BY country;

-- Multiple columns
SELECT country, city, COUNT(*)
FROM users
GROUP BY country, city;
```

### HAVING

```sql
SELECT country, COUNT(*) AS user_count
FROM users
GROUP BY country
HAVING COUNT(*) > 100;
```

---

## Subqueries

### In WHERE

```sql
SELECT * FROM users
WHERE id IN (SELECT user_id FROM orders WHERE total > 1000);
```

### In FROM

```sql
SELECT avg_total
FROM (
    SELECT user_id, AVG(total) AS avg_total
    FROM orders
    GROUP BY user_id
) AS user_avgs
WHERE avg_total > 500;
```

### Correlated Subquery

```sql
SELECT *
FROM users u
WHERE EXISTS (
    SELECT 1 FROM orders o
    WHERE o.user_id = u.id AND o.total > 1000
);
```

---

## Common Table Expressions (CTE)

```sql
WITH high_value_customers AS (
    SELECT user_id, SUM(total) AS total_spent
    FROM orders
    GROUP BY user_id
    HAVING SUM(total) > 10000
)
SELECT u.name, hvc.total_spent
FROM users u
JOIN high_value_customers hvc ON u.id = hvc.user_id;
```

### Recursive CTE

```sql
WITH RECURSIVE subordinates AS (
    -- Base case
    SELECT id, name, manager_id, 1 AS level
    FROM employees
    WHERE id = 1

    UNION ALL

    -- Recursive case
    SELECT e.id, e.name, e.manager_id, s.level + 1
    FROM employees e
    JOIN subordinates s ON e.manager_id = s.id
)
SELECT * FROM subordinates;
```

---

## Set Operations

### UNION

```sql
SELECT name FROM customers
UNION
SELECT name FROM suppliers;

-- Keep duplicates
SELECT name FROM customers
UNION ALL
SELECT name FROM suppliers;
```

### INTERSECT

```sql
SELECT product_id FROM orders_2023
INTERSECT
SELECT product_id FROM orders_2024;
```

### EXCEPT

```sql
SELECT product_id FROM orders_2023
EXCEPT
SELECT product_id FROM orders_2024;
```

---

## CASE Expression

```sql
SELECT
    name,
    CASE status
        WHEN 'A' THEN 'Active'
        WHEN 'I' THEN 'Inactive'
        ELSE 'Unknown'
    END AS status_label
FROM users;

-- Searched CASE
SELECT
    name,
    CASE
        WHEN age < 18 THEN 'Minor'
        WHEN age < 65 THEN 'Adult'
        ELSE 'Senior'
    END AS age_group
FROM users;
```

---

## EXPLAIN

Analyze query execution:

```sql
EXPLAIN SELECT * FROM users WHERE email = 'test@example.com';

EXPLAIN ANALYZE SELECT * FROM large_table WHERE id = 1;

EXPLAIN (ANALYZE, BUFFERS, FORMAT TEXT)
SELECT * FROM orders WHERE user_id = 123;
```

---

## Locking

```sql
-- Share lock (read)
SELECT * FROM users FOR SHARE;

-- Exclusive lock (write)
SELECT * FROM users WHERE id = 1 FOR UPDATE;

-- Skip locked rows
SELECT * FROM tasks WHERE status = 'pending'
FOR UPDATE SKIP LOCKED
LIMIT 1;
```

---

## See Also

- [Aggregate Functions](../functions/aggregate-functions.md)
- [Window Functions](../functions/window-functions.md)
- [Performance Tuning](../../admin/performance-tuning.md)
