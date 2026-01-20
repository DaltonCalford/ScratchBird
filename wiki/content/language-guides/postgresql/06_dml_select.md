[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - DML SELECT

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers the SELECT statement and Common Table Expressions (CTEs) in PostgreSQL emulation mode. The SELECT statement retrieves data from tables with support for PostgreSQL-specific features like DISTINCT ON, window functions, and advanced set operations.

**Spec refs:**
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`

---

## SELECT Statement

### Basic Syntax

```sql
SELECT [ALL | DISTINCT | DISTINCT ON (expression [, ...])]
    select_list
FROM table_reference [, ...]
[WHERE condition]
[GROUP BY grouping_element [, ...]]
[HAVING condition]
[WINDOW window_name AS (window_definition) [, ...]]
[{ UNION | INTERSECT | EXCEPT } [ALL | DISTINCT] select]
[ORDER BY expression [ASC | DESC | USING operator] [NULLS {FIRST | LAST}] [, ...]]
[LIMIT {count | ALL}]
[OFFSET start [ROW | ROWS]]
[FETCH {FIRST | NEXT} [count] {ROW | ROWS} {ONLY | WITH TIES}]
[FOR {UPDATE | NO KEY UPDATE | SHARE | KEY SHARE} [OF table_name [, ...]] [NOWAIT | SKIP LOCKED]]
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

**Column aliases:**
```sql
SELECT
    id AS user_id,
    first_name || ' ' || last_name AS full_name,
    email AS contact
FROM users;
```

**Expressions and literals:**
```sql
SELECT
    id,
    name,
    'active'::text AS status,
    NOW() AS queried_at,
    price * quantity AS total
FROM orders;
```

**Table aliases:**
```sql
SELECT u.id, u.name, o.total
FROM users AS u
JOIN orders AS o ON u.id = o.user_id;
```

---

## DISTINCT and DISTINCT ON

### DISTINCT

Eliminates duplicate rows from the result set.

```sql
-- Get unique countries
SELECT DISTINCT country FROM customers;

-- Unique combinations
SELECT DISTINCT country, city FROM customers;

-- With expressions
SELECT DISTINCT UPPER(category) FROM products;
```

### DISTINCT ON (PostgreSQL-specific)

Returns the first row for each set of duplicates. Must be used with ORDER BY.

```sql
-- Get the most recent order per customer
SELECT DISTINCT ON (customer_id)
    customer_id,
    order_id,
    order_date,
    total
FROM orders
ORDER BY customer_id, order_date DESC;

-- Get the highest-priced product per category
SELECT DISTINCT ON (category)
    category,
    product_name,
    price
FROM products
ORDER BY category, price DESC;

-- Multiple DISTINCT ON columns
SELECT DISTINCT ON (region, category)
    region,
    category,
    product_name,
    sales
FROM regional_sales
ORDER BY region, category, sales DESC;
```

**Important:** The DISTINCT ON expression(s) must match the leftmost ORDER BY expression(s).

---

## WHERE Clause

### Comparison Operators

```sql
SELECT * FROM products WHERE price > 100;
SELECT * FROM products WHERE stock = 0;
SELECT * FROM products WHERE name != 'Widget';
SELECT * FROM products WHERE name <> 'Widget';  -- Same as !=
SELECT * FROM users WHERE age >= 18;
SELECT * FROM orders WHERE total <= 1000;
```

### Logical Operators

```sql
-- AND
SELECT * FROM products
WHERE category = 'Electronics' AND price < 500;

-- OR
SELECT * FROM users
WHERE country = 'USA' OR country = 'Canada';

-- NOT
SELECT * FROM products
WHERE NOT discontinued;

-- Combining operators (use parentheses for clarity)
SELECT * FROM products
WHERE (category = 'Electronics' OR category = 'Computers')
AND price BETWEEN 100 AND 500
AND NOT discontinued;
```

### BETWEEN

```sql
SELECT * FROM orders
WHERE order_date BETWEEN '2026-01-01' AND '2026-12-31';

-- BETWEEN is inclusive on both ends
SELECT * FROM products
WHERE price BETWEEN 50 AND 200;  -- 50 <= price <= 200

-- NOT BETWEEN
SELECT * FROM products
WHERE price NOT BETWEEN 50 AND 200;
```

### IN

```sql
SELECT * FROM users
WHERE country IN ('USA', 'Canada', 'Mexico');

-- With subquery
SELECT * FROM products
WHERE category_id IN (SELECT id FROM categories WHERE active = true);

-- NOT IN
SELECT * FROM users
WHERE status NOT IN ('banned', 'suspended');
```

### LIKE and ILIKE

```sql
-- Case-sensitive pattern matching
SELECT * FROM users WHERE name LIKE 'John%';      -- Starts with 'John'
SELECT * FROM users WHERE name LIKE '%son';       -- Ends with 'son'
SELECT * FROM users WHERE name LIKE '%tech%';     -- Contains 'tech'
SELECT * FROM codes WHERE code LIKE '___-____';   -- Pattern: 3 chars, dash, 4 chars

-- Case-insensitive (PostgreSQL-specific)
SELECT * FROM users WHERE email ILIKE '%@GMAIL.COM';

-- Escape character
SELECT * FROM files WHERE name LIKE '%\_%' ESCAPE '\';  -- Contains underscore
```

### SIMILAR TO and Regular Expressions

```sql
-- SIMILAR TO (SQL standard regex-like)
SELECT * FROM users WHERE name SIMILAR TO '(John|Jane)%';

-- PostgreSQL regex operators
SELECT * FROM users WHERE email ~ '^[a-z]+@example\.com$';    -- Matches
SELECT * FROM users WHERE email !~ '^test';                    -- Doesn't match
SELECT * FROM users WHERE email ~* 'gmail\.com$';             -- Case-insensitive match
SELECT * FROM users WHERE email !~* 'yahoo';                   -- Case-insensitive not match
```

### NULL Handling

```sql
SELECT * FROM users WHERE middle_name IS NULL;
SELECT * FROM orders WHERE shipped_date IS NOT NULL;

-- IS DISTINCT FROM (treats NULL as a comparable value)
SELECT * FROM products WHERE discount IS DISTINCT FROM 0;
SELECT * FROM users WHERE status IS NOT DISTINCT FROM 'active';
```

### Array and JSON Operators

```sql
-- Array contains
SELECT * FROM posts WHERE tags @> ARRAY['postgresql'];

-- Array overlap
SELECT * FROM posts WHERE tags && ARRAY['sql', 'database'];

-- JSON containment
SELECT * FROM events WHERE data @> '{"type": "click"}';

-- JSON key exists
SELECT * FROM events WHERE data ? 'user_id';
```

---

## Joins

### INNER JOIN

```sql
SELECT
    u.name,
    o.order_id,
    o.total
FROM users u
INNER JOIN orders o ON u.id = o.user_id;

-- Shorthand
SELECT u.name, o.order_id
FROM users u
JOIN orders o ON u.id = o.user_id;
```

### LEFT OUTER JOIN

```sql
SELECT
    u.name,
    COALESCE(COUNT(o.id), 0) AS order_count
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
GROUP BY u.id, u.name;

-- LEFT OUTER JOIN is same as LEFT JOIN
SELECT u.*, o.order_id
FROM users u
LEFT OUTER JOIN orders o ON u.id = o.user_id;
```

### RIGHT OUTER JOIN

```sql
SELECT
    p.name AS product,
    c.name AS category
FROM products p
RIGHT JOIN categories c ON p.category_id = c.id;
```

### FULL OUTER JOIN

```sql
SELECT
    e.name AS employee,
    d.name AS department
FROM employees e
FULL OUTER JOIN departments d ON e.department_id = d.id;
```

### CROSS JOIN

```sql
SELECT
    colors.name AS color,
    sizes.name AS size
FROM colors
CROSS JOIN sizes;

-- Equivalent comma syntax
SELECT c.name, s.name
FROM colors c, sizes s;
```

### Multiple Joins

```sql
SELECT
    c.name AS customer,
    o.order_date,
    p.name AS product,
    oi.quantity,
    oi.unit_price
FROM customers c
JOIN orders o ON c.id = o.customer_id
JOIN order_items oi ON o.id = oi.order_id
JOIN products p ON oi.product_id = p.id
WHERE o.status = 'completed'
ORDER BY o.order_date DESC;
```

### Self Join

```sql
SELECT
    e.name AS employee,
    m.name AS manager
FROM employees e
LEFT JOIN employees m ON e.manager_id = m.id;
```

### USING Clause

```sql
-- When join columns have the same name
SELECT *
FROM orders
JOIN customers USING (customer_id);

-- Multiple columns
SELECT *
FROM order_items
JOIN inventory USING (product_id, warehouse_id);
```

### NATURAL JOIN

```sql
-- Automatically joins on columns with matching names
SELECT *
FROM users
NATURAL JOIN user_profiles;
```

### LATERAL Join (PostgreSQL-specific)

```sql
-- LATERAL allows subquery to reference earlier FROM items
SELECT
    u.name,
    recent_orders.*
FROM users u
CROSS JOIN LATERAL (
    SELECT order_id, total, order_date
    FROM orders
    WHERE user_id = u.id
    ORDER BY order_date DESC
    LIMIT 3
) AS recent_orders;

-- With LEFT JOIN LATERAL
SELECT
    d.name AS department,
    top_salary.max_salary
FROM departments d
LEFT JOIN LATERAL (
    SELECT MAX(salary) AS max_salary
    FROM employees
    WHERE department_id = d.id
) AS top_salary ON true;
```

---

## GROUP BY

### Basic Grouping

```sql
SELECT
    category,
    COUNT(*) AS product_count
FROM products
GROUP BY category;
```

### Multiple Columns

```sql
SELECT
    country,
    city,
    COUNT(*) AS user_count
FROM users
GROUP BY country, city
ORDER BY country, city;
```

### Grouping by Expression

```sql
SELECT
    DATE_TRUNC('month', order_date) AS month,
    COUNT(*) AS order_count,
    SUM(total) AS revenue
FROM orders
GROUP BY DATE_TRUNC('month', order_date)
ORDER BY month;
```

### ROLLUP

```sql
-- Generates subtotals and grand total
SELECT
    region,
    category,
    SUM(sales) AS total_sales
FROM regional_sales
GROUP BY ROLLUP (region, category);

-- Result includes:
-- 1. Each region + category combination
-- 2. Subtotal for each region (category = NULL)
-- 3. Grand total (region = NULL, category = NULL)
```

### CUBE

```sql
-- Generates all possible subtotals
SELECT
    region,
    category,
    SUM(sales) AS total_sales
FROM regional_sales
GROUP BY CUBE (region, category);

-- Result includes all 2^n combinations
```

### GROUPING SETS

```sql
-- Specify exact grouping combinations
SELECT
    region,
    category,
    product,
    SUM(sales) AS total_sales
FROM sales
GROUP BY GROUPING SETS (
    (region, category, product),  -- Detail level
    (region, category),           -- By region and category
    (region),                     -- By region only
    ()                            -- Grand total
);
```

### GROUPING Function

```sql
-- Identify which columns are aggregated
SELECT
    region,
    category,
    SUM(sales) AS total_sales,
    GROUPING(region) AS region_is_total,
    GROUPING(category) AS category_is_total
FROM regional_sales
GROUP BY ROLLUP (region, category);
```

---

## Aggregate Functions

### Common Aggregates

```sql
SELECT
    category,
    COUNT(*) AS count,
    COUNT(discount) AS discounted_count,  -- Non-null count
    SUM(price) AS total_price,
    AVG(price) AS avg_price,
    MIN(price) AS min_price,
    MAX(price) AS max_price,
    SUM(stock * price) AS inventory_value
FROM products
GROUP BY category;
```

### FILTER Clause (PostgreSQL-specific)

```sql
SELECT
    COUNT(*) AS total_orders,
    COUNT(*) FILTER (WHERE status = 'completed') AS completed_orders,
    COUNT(*) FILTER (WHERE status = 'pending') AS pending_orders,
    SUM(total) FILTER (WHERE status = 'completed') AS completed_revenue
FROM orders;
```

### Array and String Aggregates

```sql
-- Aggregate into array
SELECT
    customer_id,
    ARRAY_AGG(product_name ORDER BY order_date) AS purchased_products
FROM orders o
JOIN order_items oi ON o.id = oi.order_id
JOIN products p ON oi.product_id = p.id
GROUP BY customer_id;

-- String aggregation
SELECT
    department_id,
    STRING_AGG(name, ', ' ORDER BY name) AS employee_names
FROM employees
GROUP BY department_id;
```

### Statistical Aggregates

```sql
SELECT
    category,
    STDDEV(price) AS price_stddev,
    VARIANCE(price) AS price_variance,
    PERCENTILE_CONT(0.5) WITHIN GROUP (ORDER BY price) AS median_price,
    PERCENTILE_DISC(0.5) WITHIN GROUP (ORDER BY price) AS median_price_discrete,
    MODE() WITHIN GROUP (ORDER BY price) AS mode_price
FROM products
GROUP BY category;
```

---

## HAVING Clause

Filters groups after aggregation.

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
    DATE_TRUNC('month', order_date) AS month,
    COUNT(*) AS order_count,
    SUM(total) AS revenue
FROM orders
GROUP BY DATE_TRUNC('month', order_date)
HAVING COUNT(*) >= 100 AND SUM(total) > 10000;
```

---

## ORDER BY

### Basic Sorting

```sql
-- Ascending (default)
SELECT * FROM products ORDER BY price;
SELECT * FROM users ORDER BY name ASC;

-- Descending
SELECT * FROM products ORDER BY price DESC;
```

### Multiple Columns

```sql
SELECT * FROM products
ORDER BY category ASC, price DESC;
```

### NULLS FIRST / NULLS LAST

```sql
-- NULLs at the end (default for ASC)
SELECT * FROM users ORDER BY last_login DESC NULLS LAST;

-- NULLs at the beginning
SELECT * FROM products ORDER BY discount_rate ASC NULLS FIRST;
```

### By Column Position

```sql
SELECT name, price FROM products
ORDER BY 2 DESC, 1 ASC;  -- Order by price DESC, then name ASC
```

### By Expression

```sql
SELECT first_name, last_name
FROM users
ORDER BY last_name || ', ' || first_name;

SELECT product_name, price, stock
FROM products
ORDER BY price * stock DESC;
```

### USING Operator

```sql
-- Use custom operator for ordering
SELECT * FROM points
ORDER BY location USING <->;  -- Distance operator
```

---

## LIMIT, OFFSET, and FETCH

### LIMIT

```sql
SELECT * FROM products LIMIT 10;
SELECT * FROM products LIMIT ALL;  -- No limit (same as omitting)
```

### OFFSET

```sql
SELECT * FROM products LIMIT 10 OFFSET 20;  -- Skip 20, return 10
SELECT * FROM products OFFSET 10;           -- Skip 10, return rest
```

### FETCH (SQL Standard)

```sql
-- Equivalent to LIMIT
SELECT * FROM products
FETCH FIRST 10 ROWS ONLY;

-- With offset
SELECT * FROM products
OFFSET 20 ROWS
FETCH NEXT 10 ROWS ONLY;

-- WITH TIES (includes ties in ORDER BY)
SELECT * FROM products
ORDER BY price DESC
FETCH FIRST 5 ROWS WITH TIES;  -- May return more if tied prices
```

### Pagination Pattern

```sql
-- Page 1
SELECT * FROM products ORDER BY id LIMIT 10 OFFSET 0;

-- Page 2
SELECT * FROM products ORDER BY id LIMIT 10 OFFSET 10;

-- Page N (page_size = 10)
SELECT * FROM products ORDER BY id LIMIT 10 OFFSET ((N - 1) * 10);
```

---

## Subqueries

### Scalar Subquery

```sql
SELECT *
FROM products
WHERE price > (SELECT AVG(price) FROM products);

SELECT
    name,
    salary,
    salary - (SELECT AVG(salary) FROM employees) AS diff_from_avg
FROM employees;
```

### IN Subquery

```sql
SELECT *
FROM users
WHERE id IN (SELECT DISTINCT customer_id FROM orders);

SELECT *
FROM products
WHERE category_id NOT IN (
    SELECT id FROM categories WHERE discontinued = true
);
```

### EXISTS Subquery

```sql
SELECT *
FROM products p
WHERE EXISTS (
    SELECT 1 FROM order_items oi WHERE oi.product_id = p.id
);

SELECT *
FROM customers c
WHERE NOT EXISTS (
    SELECT 1 FROM orders WHERE customer_id = c.id
);
```

### Correlated Subquery

```sql
SELECT
    p.name,
    p.price,
    (SELECT AVG(price) FROM products WHERE category_id = p.category_id) AS category_avg
FROM products p;

SELECT *
FROM employees e
WHERE salary > (
    SELECT AVG(salary)
    FROM employees
    WHERE department_id = e.department_id
);
```

### Subquery in FROM (Derived Table)

```sql
SELECT category, avg_price
FROM (
    SELECT category, AVG(price) AS avg_price
    FROM products
    GROUP BY category
) AS category_stats
WHERE avg_price > 100;
```

### ANY / SOME / ALL

```sql
-- ANY (matches if any value satisfies)
SELECT * FROM products
WHERE price > ANY (SELECT price FROM products WHERE category = 'Budget');

-- ALL (matches if all values satisfy)
SELECT * FROM products
WHERE price > ALL (SELECT price FROM products WHERE category = 'Budget');

-- SOME is synonym for ANY
SELECT * FROM users
WHERE age >= SOME (SELECT min_age FROM access_levels);
```

---

## Set Operations

### UNION

```sql
-- Removes duplicates
SELECT name FROM customers
UNION
SELECT name FROM suppliers;

-- Keeps duplicates
SELECT product_id FROM order_items
UNION ALL
SELECT product_id FROM wishlist_items;
```

### INTERSECT

```sql
-- Rows in both queries
SELECT customer_id FROM orders_2024
INTERSECT
SELECT customer_id FROM orders_2025;

SELECT id FROM premium_users
INTERSECT ALL
SELECT id FROM active_users;
```

### EXCEPT

```sql
-- Rows in first but not second
SELECT id FROM all_users
EXCEPT
SELECT user_id FROM banned_users;

SELECT product_id FROM inventory
EXCEPT ALL
SELECT product_id FROM discontinued;
```

### Combining Set Operations

```sql
(SELECT name, 'customer' AS type FROM customers)
UNION
(SELECT name, 'supplier' AS type FROM suppliers)
EXCEPT
(SELECT name, type FROM blacklist)
ORDER BY name;
```

---

## Common Table Expressions (WITH)

### Basic CTE

```sql
WITH recent_orders AS (
    SELECT *
    FROM orders
    WHERE order_date > CURRENT_DATE - INTERVAL '30 days'
)
SELECT
    customer_id,
    COUNT(*) AS order_count,
    SUM(total) AS total_spent
FROM recent_orders
GROUP BY customer_id;
```

### Multiple CTEs

```sql
WITH
high_value_customers AS (
    SELECT customer_id
    FROM orders
    GROUP BY customer_id
    HAVING SUM(total) > 10000
),
recent_orders AS (
    SELECT *
    FROM orders
    WHERE order_date > CURRENT_DATE - INTERVAL '90 days'
)
SELECT
    c.name,
    COUNT(ro.id) AS recent_order_count
FROM customers c
JOIN high_value_customers hvc ON c.id = hvc.customer_id
LEFT JOIN recent_orders ro ON c.id = ro.customer_id
GROUP BY c.id, c.name;
```

### Recursive CTE

```sql
-- Organizational hierarchy
WITH RECURSIVE org_chart AS (
    -- Base case: top-level employees (no manager)
    SELECT id, name, manager_id, 1 AS level
    FROM employees
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive case: employees with managers
    SELECT e.id, e.name, e.manager_id, oc.level + 1
    FROM employees e
    JOIN org_chart oc ON e.manager_id = oc.id
)
SELECT * FROM org_chart ORDER BY level, name;

-- Tree traversal with path
WITH RECURSIVE category_tree AS (
    SELECT id, name, parent_id, name::text AS path, 1 AS depth
    FROM categories
    WHERE parent_id IS NULL

    UNION ALL

    SELECT c.id, c.name, c.parent_id,
           ct.path || ' > ' || c.name,
           ct.depth + 1
    FROM categories c
    JOIN category_tree ct ON c.parent_id = ct.id
)
SELECT * FROM category_tree ORDER BY path;

-- Numeric sequence
WITH RECURSIVE numbers AS (
    SELECT 1 AS n
    UNION ALL
    SELECT n + 1 FROM numbers WHERE n < 100
)
SELECT * FROM numbers;
```

### CTE with Data Modification

```sql
-- Use CTE result in INSERT
WITH new_data AS (
    SELECT id, calculated_value
    FROM source_table
    WHERE condition
)
INSERT INTO target_table (source_id, value)
SELECT id, calculated_value FROM new_data;

-- CTE that modifies data (PostgreSQL-specific)
WITH deleted_orders AS (
    DELETE FROM orders
    WHERE status = 'cancelled'
    AND order_date < CURRENT_DATE - INTERVAL '1 year'
    RETURNING *
)
INSERT INTO archived_orders
SELECT * FROM deleted_orders;
```

---

## Window Functions

### Basic Window Functions

```sql
SELECT
    name,
    department,
    salary,
    ROW_NUMBER() OVER (ORDER BY salary DESC) AS rank,
    RANK() OVER (ORDER BY salary DESC) AS rank_with_gaps,
    DENSE_RANK() OVER (ORDER BY salary DESC) AS rank_no_gaps
FROM employees;
```

### PARTITION BY

```sql
SELECT
    name,
    department,
    salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank,
    salary - AVG(salary) OVER (PARTITION BY department) AS diff_from_dept_avg
FROM employees;
```

### Running Totals and Aggregates

```sql
SELECT
    order_date,
    total,
    SUM(total) OVER (ORDER BY order_date) AS running_total,
    AVG(total) OVER (ORDER BY order_date ROWS BETWEEN 6 PRECEDING AND CURRENT ROW) AS weekly_avg
FROM orders;
```

### Frame Specification

```sql
SELECT
    order_date,
    total,
    -- Last 7 days
    SUM(total) OVER (
        ORDER BY order_date
        RANGE BETWEEN INTERVAL '7 days' PRECEDING AND CURRENT ROW
    ) AS weekly_total,
    -- Moving average (5 rows)
    AVG(total) OVER (
        ORDER BY order_date
        ROWS BETWEEN 2 PRECEDING AND 2 FOLLOWING
    ) AS moving_avg
FROM orders;
```

### Named Windows

```sql
SELECT
    name,
    department,
    salary,
    ROW_NUMBER() OVER w AS row_num,
    RANK() OVER w AS rank,
    SUM(salary) OVER w AS dept_total
FROM employees
WINDOW w AS (PARTITION BY department ORDER BY salary DESC);
```

### LAG and LEAD

```sql
SELECT
    order_date,
    total,
    LAG(total) OVER (ORDER BY order_date) AS prev_total,
    LEAD(total) OVER (ORDER BY order_date) AS next_total,
    total - LAG(total) OVER (ORDER BY order_date) AS change
FROM orders;
```

### FIRST_VALUE and LAST_VALUE

```sql
SELECT
    name,
    department,
    salary,
    FIRST_VALUE(name) OVER (
        PARTITION BY department
        ORDER BY salary DESC
    ) AS highest_paid,
    LAST_VALUE(name) OVER (
        PARTITION BY department
        ORDER BY salary DESC
        RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS lowest_paid
FROM employees;
```

### NTH_VALUE and NTILE

```sql
SELECT
    name,
    salary,
    NTH_VALUE(name, 2) OVER (ORDER BY salary DESC) AS second_highest,
    NTILE(4) OVER (ORDER BY salary DESC) AS quartile
FROM employees;
```

---

## FOR UPDATE / FOR SHARE

### Row-Level Locking

```sql
-- Lock selected rows for update
SELECT * FROM accounts
WHERE id = 123
FOR UPDATE;

-- Lock for update, but don't block others reading
SELECT * FROM accounts
WHERE id = 123
FOR NO KEY UPDATE;

-- Shared lock (allow others to read, prevent updates)
SELECT * FROM accounts
WHERE id = 123
FOR SHARE;

-- Key share (allow non-key updates)
SELECT * FROM accounts
WHERE id = 123
FOR KEY SHARE;
```

### Lock Options

```sql
-- Skip locked rows
SELECT * FROM tasks
WHERE status = 'pending'
ORDER BY priority DESC
LIMIT 1
FOR UPDATE SKIP LOCKED;

-- Fail immediately if locked
SELECT * FROM accounts
WHERE id = 123
FOR UPDATE NOWAIT;

-- Lock specific tables in join
SELECT * FROM orders o
JOIN order_items oi ON o.id = oi.order_id
WHERE o.id = 456
FOR UPDATE OF orders;
```

---

## Known Limitations

### Current Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Basic SELECT | Partial | Simple queries work, complex may fail |
| DISTINCT | Partial | Flag encoding mismatch |
| DISTINCT ON | Stubbed | Parser accepts, executor may fail |
| JOINs | Partial | Basic joins work, LATERAL stubbed |
| GROUP BY | Partial | Basic works, ROLLUP/CUBE stubbed |
| HAVING | Implemented | Works with limitations |
| ORDER BY | Implemented | Basic sorting works |
| LIMIT/OFFSET | Implemented | Works correctly |
| FETCH | Partial | Basic works, WITH TIES stubbed |
| Subqueries | Partial | Simple works, correlated may fail |
| Set operations | Partial | UNION works, INTERSECT/EXCEPT stubbed |
| CTEs | Stubbed | Parser accepts, executor mismatch |
| Recursive CTEs | Stubbed | Parser accepts, executor mismatch |
| Window functions | Stubbed | Parser accepts, executor mismatch |
| FOR UPDATE | Partial | Basic locking works |

### Specific Issues

**Bytecode Format Mismatches:**
- SELECT bytecode layout differs from executor expectations
- DISTINCT flag encoding issues
- Alias string encoding differs
- CTE clause (EXT_WITH_CLAUSE) payload mismatch

**Unsupported Features:**
- LATERAL joins
- Window frame specifications
- GROUPING SETS / CUBE / ROLLUP
- FETCH WITH TIES
- Complex recursive CTEs

### Workarounds

**For CTEs:** Use derived tables (subqueries in FROM) instead:
```sql
-- Instead of CTE:
-- WITH recent AS (SELECT * FROM orders WHERE date > '2026-01-01')
-- SELECT * FROM recent;

-- Use derived table:
SELECT * FROM (
    SELECT * FROM orders WHERE date > '2026-01-01'
) AS recent;
```

**For Window Functions:** Use application-level processing or correlated subqueries:
```sql
-- Instead of ROW_NUMBER():
SELECT
    a.*,
    (SELECT COUNT(*) FROM products b
     WHERE b.category = a.category AND b.price >= a.price) AS rank
FROM products a;
```

---

## See Also

- [DML Modification](07_dml_modification.md) - INSERT, UPDATE, DELETE
- [Transaction Control](08_transactions.md) - BEGIN, COMMIT, ROLLBACK
- [Functions](14_functions.md) - Aggregate and window functions
- [Operators](12_operators.md) - Comparison and logical operators

