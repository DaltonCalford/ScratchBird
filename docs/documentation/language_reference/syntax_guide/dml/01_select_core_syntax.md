<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# SELECT - Core Syntax

[Prev](./README.md) | [Next](./02_select_projection_sources_and_aliasing.md) | [Topic README](./README.md) | [DML README](./README.md) | [Syntax Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1
- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

SELECT retrieves data from one or more tables, views, or other queryable objects.

## Syntax

```sql
[ WITH [ RECURSIVE ] with_query [, ...] ]
SELECT [ ALL | DISTINCT [ ON ( expression [, ...] ) ] ]
    [ * | expression [ [ AS ] output_name ] [, ...] ]
    [ FROM from_item [, ...] ]
    [ WHERE condition ]
    [ GROUP BY [ ALL | DISTINCT ] grouping_element [, ...] ]
    [ HAVING condition ]
    [ WINDOW window_name AS ( window_definition ) [, ...] ]
    [ { UNION | INTERSECT | EXCEPT } [ ALL | DISTINCT ] select ]
    [ ORDER BY expression [ ASC | DESC | USING operator ] [ NULLS { FIRST | LAST } ] [, ...] ]
    [ LIMIT { count | ALL } ]
    [ OFFSET start [ ROW | ROWS ] ]
    [ FETCH { FIRST | NEXT } [ count ] { ROW | ROWS } [ WITH TIES ] ]
    [ FOR { UPDATE | NO KEY UPDATE | SHARE | KEY SHARE } [ OF table_name [, ...] ] [ NOWAIT | SKIP LOCKED ] ]
```

## Core Clauses

### SELECT List

```sql
-- All columns
SELECT * FROM users;

-- Specific columns
SELECT id, name, email FROM users;

-- Expressions
SELECT id, name, salary * 1.1 AS new_salary FROM employees;

-- DISTINCT
SELECT DISTINCT department FROM employees;

-- DISTINCT ON (returns first row per distinct values)
SELECT DISTINCT ON (department) * FROM employees ORDER BY department, salary DESC;
```

### FROM Clause

```sql
-- Single table
SELECT * FROM users;

-- Multiple tables (cross join)
SELECT * FROM users, orders;

-- Table alias
SELECT u.id, u.name FROM users AS u;

-- Subquery
SELECT * FROM (SELECT * FROM users WHERE active = true) AS active_users;

-- Table function
SELECT * FROM generate_series(1, 10);
```

### WHERE Clause

```sql
-- Comparison operators
SELECT * FROM users WHERE age > 18;
SELECT * FROM products WHERE price <= 100;

-- Logical operators
SELECT * FROM users WHERE age > 18 AND status = 'active';
SELECT * FROM users WHERE status = 'inactive' OR last_login < '2024-01-01';

-- IN operator
SELECT * FROM users WHERE status IN ('active', 'pending');

-- BETWEEN
SELECT * FROM orders WHERE created_at BETWEEN '2024-01-01' AND '2024-01-31';

-- LIKE (pattern matching)
SELECT * FROM users WHERE email LIKE '%@company.com';
SELECT * FROM users WHERE name LIKE 'John%';  -- starts with John

-- IS NULL / IS NOT NULL
SELECT * FROM users WHERE phone IS NULL;

-- EXISTS
SELECT * FROM departments d WHERE EXISTS (
    SELECT 1 FROM employees e WHERE e.dept_id = d.id
);
```

### GROUP BY and Aggregates

```sql
-- Simple grouping
SELECT department, COUNT(*) AS employee_count
FROM employees
GROUP BY department;

-- Multiple aggregates
SELECT 
    department,
    COUNT(*) AS count,
    AVG(salary) AS avg_salary,
    MAX(salary) AS max_salary,
    MIN(salary) AS min_salary,
    SUM(bonus) AS total_bonus
FROM employees
GROUP BY department;

-- Multiple columns
SELECT department, job_title, COUNT(*)
FROM employees
GROUP BY department, job_title;

-- HAVING (filter groups)
SELECT department, AVG(salary) AS avg_salary
FROM employees
GROUP BY department
HAVING AVG(salary) > 50000;
```

### ORDER BY

```sql
-- Single column
SELECT * FROM users ORDER BY name;

-- Multiple columns
SELECT * FROM users ORDER BY department, name DESC;

-- Expression
SELECT * FROM users ORDER BY LENGTH(name);

-- NULL handling
SELECT * FROM users ORDER BY phone NULLS LAST;

-- Ordinal position
SELECT name, salary FROM users ORDER BY 2 DESC;  -- order by salary
```

### LIMIT / OFFSET

```sql
-- Top N
SELECT * FROM users ORDER BY created_at DESC LIMIT 10;

-- Pagination
SELECT * FROM users ORDER BY id LIMIT 20 OFFSET 40;  -- page 3, 20 per page

-- FETCH syntax (SQL standard)
SELECT * FROM users ORDER BY score DESC FETCH FIRST 10 ROWS ONLY;

-- WITH TIES
SELECT * FROM users ORDER BY score DESC FETCH FIRST 10 ROWS WITH TIES;
-- (includes all rows tied for 10th place)
```

## Complete Examples

### Basic Queries

```sql
-- All active users
SELECT id, name, email 
FROM users 
WHERE status = 'active' 
ORDER BY name;

-- Recent orders with totals
SELECT 
    o.id,
    o.created_at,
    u.name AS customer_name,
    SUM(oi.quantity * oi.price) AS total
FROM orders o
JOIN users u ON o.user_id = u.id
JOIN order_items oi ON o.id = oi.order_id
WHERE o.created_at > NOW() - INTERVAL '30 days'
GROUP BY o.id, o.created_at, u.name
HAVING SUM(oi.quantity * oi.price) > 100
ORDER BY total DESC
LIMIT 100;
```

### Analytics Queries

```sql
-- Monthly revenue
SELECT 
    DATE_TRUNC('month', created_at) AS month,
    COUNT(*) AS order_count,
    SUM(amount) AS revenue
FROM orders
WHERE created_at >= '2024-01-01'
GROUP BY DATE_TRUNC('month', created_at)
ORDER BY month;

-- Running totals (using window functions)
SELECT 
    date,
    amount,
    SUM(amount) OVER (ORDER BY date) AS running_total
FROM daily_sales;
```

## Parser Acceptance Cases

```sql
SELECT 1;
SELECT * FROM t1;
SELECT a, b FROM t1 WHERE c = 1;
SELECT a, COUNT(*) FROM t1 GROUP BY a;
SELECT * FROM t1 ORDER BY a LIMIT 10;
SELECT * FROM t1 UNION SELECT * FROM t2;
```

## Parser Rejection Cases

```sql
-- Column not in GROUP BY
SELECT a, b FROM t1 GROUP BY a;  -- Error: b not in GROUP BY

-- Ambiguous column
SELECT a FROM t1, t2;  -- Error: if both t1 and t2 have column a
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `undefined_column` | Column doesn't exist |
| `undefined_table` | Table doesn't exist |
| `group_by_violation` | Column not in GROUP BY |
| `ambiguous_column` | Column name matches multiple tables |

## See Also

- [Joins](03_select_joins_and_lateral_semantics.md)
- [Window Functions](05_select_window_order_limit_offset.md)
- [CTEs](12_cte_and_recursive_query_syntax.md)
- [INSERT](07_insert_syntax.md)
- [UPDATE](08_update_syntax.md)
- [DELETE](09_delete_syntax.md)
