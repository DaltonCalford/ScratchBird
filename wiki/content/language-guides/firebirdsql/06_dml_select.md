[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - DML SELECT

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`

---

## Overview

The SELECT statement retrieves data from one or more tables. Firebird SQL uses some unique syntax elements compared to other SQL dialects:

- **FIRST/SKIP** instead of LIMIT/OFFSET for result limiting
- **ROWS** clause as alternative row limiting syntax
- **FROM RDB$DATABASE** for single-row queries without a real table
- **NULLS FIRST/LAST** for explicit NULL ordering

---

## Basic SELECT

### Syntax

```sql
SELECT [ALL | DISTINCT]
    [FIRST n] [SKIP n]
    select_list
FROM table_reference [, ...]
[WHERE search_condition]
[GROUP BY grouping_expression [, ...]]
[HAVING search_condition]
[UNION [ALL | DISTINCT] select_statement]
[ORDER BY order_expression [ASC | DESC] [NULLS FIRST | NULLS LAST] [, ...]]
[ROWS m [TO n]]
[FOR UPDATE [OF column_list] [WITH LOCK]]
```

### Examples

#### Select All Columns

```sql
SELECT * FROM employees;
```

#### Select Specific Columns

```sql
SELECT employee_id, first_name, last_name, salary
FROM employees;
```

#### Column Aliases

```sql
SELECT
    employee_id AS id,
    first_name || ' ' || last_name AS full_name,
    salary * 12 AS annual_salary
FROM employees;
```

Note: The `||` operator is for string concatenation in Firebird.

#### Expressions and Literals

```sql
SELECT
    'Employee: ' || first_name AS label,
    salary + bonus AS total_compensation,
    hire_date + 365 AS anniversary_date
FROM employees;
```

#### Single-Row Query Without Table

```sql
-- Use RDB$DATABASE for queries that don't need a table
SELECT CURRENT_TIMESTAMP FROM RDB$DATABASE;

SELECT 1 + 1 AS result FROM RDB$DATABASE;

SELECT UPPER('hello world') FROM RDB$DATABASE;
```

---

## DISTINCT

### Description

Removes duplicate rows from the result set.

### Examples

```sql
-- Unique departments
SELECT DISTINCT department_id FROM employees;

-- Unique combinations
SELECT DISTINCT department_id, job_title FROM employees;
```

---

## FIRST and SKIP (Row Limiting)

### Description

Firebird uses FIRST and SKIP for result limiting, similar to LIMIT and OFFSET in other databases.

- **FIRST n**: Return only the first n rows
- **SKIP n**: Skip the first n rows before returning results

### Syntax

```sql
SELECT FIRST n [SKIP m] column_list
FROM table_reference
[WHERE ...]
[ORDER BY ...];
```

### Examples

#### First N Rows

```sql
-- Get first 10 employees
SELECT FIRST 10 * FROM employees ORDER BY hire_date;
```

#### Skip N Rows

```sql
-- Skip first 20, return next 10 (pagination)
SELECT FIRST 10 SKIP 20 * FROM employees ORDER BY employee_id;
```

#### Variables in FIRST/SKIP

```sql
-- Using parameters (in prepared statements)
SELECT FIRST ? SKIP ? employee_id, first_name
FROM employees
ORDER BY employee_id;
```

### Pagination Pattern

```sql
-- Page 1 (rows 1-10)
SELECT FIRST 10 SKIP 0 * FROM products ORDER BY product_id;

-- Page 2 (rows 11-20)
SELECT FIRST 10 SKIP 10 * FROM products ORDER BY product_id;

-- Page 3 (rows 21-30)
SELECT FIRST 10 SKIP 20 * FROM products ORDER BY product_id;

-- General formula: SKIP (page_number - 1) * page_size
```

---

## ROWS Clause

### Description

Alternative syntax for row limiting, placed at the end of the query. Can specify a range of rows to return.

### Syntax

```sql
SELECT column_list
FROM table_reference
[WHERE ...]
[ORDER BY ...]
ROWS m [TO n];
```

- `ROWS m`: Return first m rows (same as FIRST m)
- `ROWS m TO n`: Return rows from position m to n (1-based)

### Examples

```sql
-- First 10 rows
SELECT * FROM employees ORDER BY salary DESC ROWS 10;

-- Rows 11 through 20
SELECT * FROM employees ORDER BY salary DESC ROWS 11 TO 20;

-- First 5 rows (same as FIRST 5)
SELECT * FROM products ORDER BY price ROWS 5;
```

### FIRST/SKIP vs ROWS

| FIRST/SKIP | ROWS Equivalent |
|------------|-----------------|
| `FIRST 10` | `ROWS 10` |
| `FIRST 10 SKIP 20` | `ROWS 21 TO 30` |
| `FIRST 5 SKIP 0` | `ROWS 5` or `ROWS 1 TO 5` |

---

## WHERE Clause

### Description

Filters rows based on a condition.

### Comparison Operators

```sql
-- Equality
SELECT * FROM employees WHERE department_id = 10;

-- Inequality
SELECT * FROM employees WHERE status <> 'INACTIVE';

-- Range comparisons
SELECT * FROM products WHERE price > 100;
SELECT * FROM products WHERE price >= 100;
SELECT * FROM products WHERE price < 50;
SELECT * FROM products WHERE price <= 50;
```

### BETWEEN

```sql
SELECT * FROM orders
WHERE order_date BETWEEN '2024-01-01' AND '2024-12-31';

SELECT * FROM products
WHERE price BETWEEN 10 AND 100;
```

### IN List

```sql
SELECT * FROM employees
WHERE department_id IN (10, 20, 30);

SELECT * FROM products
WHERE category IN ('Electronics', 'Books', 'Clothing');
```

### LIKE Pattern Matching

```sql
-- Starts with 'A'
SELECT * FROM employees WHERE last_name LIKE 'A%';

-- Contains 'smith'
SELECT * FROM employees WHERE last_name LIKE '%smith%';

-- Second character is 'a'
SELECT * FROM employees WHERE last_name LIKE '_a%';

-- Escape special characters
SELECT * FROM products WHERE name LIKE '%10\%%' ESCAPE '\';
```

### SIMILAR TO (Regular Expressions)

```sql
-- Matches email pattern
SELECT * FROM users
WHERE email SIMILAR TO '[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}';

-- Matches product codes
SELECT * FROM products
WHERE product_code SIMILAR TO '[A-Z]{3}-[0-9]{4}';
```

### CONTAINING and STARTING WITH

Firebird-specific string matching operators:

```sql
-- Contains substring (case-insensitive by default with some collations)
SELECT * FROM products WHERE description CONTAINING 'wireless';

-- Starts with prefix
SELECT * FROM customers WHERE company_name STARTING WITH 'Acme';
```

**Note**: These are parsed but may not be fully encoded in the V2 generator.

### IS NULL / IS NOT NULL

```sql
SELECT * FROM employees WHERE manager_id IS NULL;

SELECT * FROM orders WHERE shipped_date IS NOT NULL;
```

### Boolean Expressions

```sql
-- AND
SELECT * FROM products
WHERE category = 'Electronics' AND price < 500;

-- OR
SELECT * FROM employees
WHERE department_id = 10 OR department_id = 20;

-- NOT
SELECT * FROM products
WHERE NOT discontinued;

-- Combined
SELECT * FROM orders
WHERE (status = 'PENDING' OR status = 'PROCESSING')
AND order_date > '2024-01-01';
```

### EXISTS Subquery

```sql
SELECT * FROM customers c
WHERE EXISTS (
    SELECT 1 FROM orders o
    WHERE o.customer_id = c.customer_id
    AND o.total_amount > 1000
);
```

### IN Subquery

```sql
SELECT * FROM products
WHERE category_id IN (
    SELECT category_id FROM categories
    WHERE active = 1
);
```

---

## JOIN Operations

### Description

Combines rows from multiple tables based on related columns.

### INNER JOIN

```sql
-- Explicit JOIN syntax
SELECT e.employee_id, e.first_name, d.department_name
FROM employees e
INNER JOIN departments d ON e.department_id = d.department_id;

-- Implicit JOIN (comma syntax)
SELECT e.employee_id, e.first_name, d.department_name
FROM employees e, departments d
WHERE e.department_id = d.department_id;
```

### LEFT JOIN (LEFT OUTER JOIN)

```sql
-- All employees, even those without departments
SELECT e.employee_id, e.first_name, d.department_name
FROM employees e
LEFT JOIN departments d ON e.department_id = d.department_id;
```

### RIGHT JOIN (RIGHT OUTER JOIN)

```sql
-- All departments, even those without employees
SELECT e.employee_id, e.first_name, d.department_name
FROM employees e
RIGHT JOIN departments d ON e.department_id = d.department_id;
```

### FULL OUTER JOIN

```sql
-- All employees and all departments
SELECT e.employee_id, e.first_name, d.department_name
FROM employees e
FULL OUTER JOIN departments d ON e.department_id = d.department_id;
```

### CROSS JOIN

```sql
-- Cartesian product
SELECT p.product_name, c.color_name
FROM products p
CROSS JOIN colors c;
```

### Self Join

```sql
-- Employees with their managers
SELECT
    e.employee_id,
    e.first_name AS employee_name,
    m.first_name AS manager_name
FROM employees e
LEFT JOIN employees m ON e.manager_id = m.employee_id;
```

### Multiple Joins

```sql
SELECT
    o.order_id,
    c.company_name,
    p.product_name,
    oi.quantity,
    oi.unit_price
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
JOIN order_items oi ON o.order_id = oi.order_id
JOIN products p ON oi.product_id = p.product_id
WHERE o.order_date >= '2024-01-01';
```

### Join with Additional Conditions

```sql
SELECT e.*, d.department_name
FROM employees e
JOIN departments d
    ON e.department_id = d.department_id
    AND d.active = 1;
```

### Natural Join

```sql
-- Joins on columns with same names (use with caution)
SELECT *
FROM orders
NATURAL JOIN customers;
```

---

## GROUP BY and Aggregation

### Aggregate Functions

```sql
-- Count
SELECT COUNT(*) FROM employees;
SELECT COUNT(DISTINCT department_id) FROM employees;

-- Sum
SELECT SUM(salary) FROM employees;

-- Average
SELECT AVG(salary) FROM employees;

-- Min/Max
SELECT MIN(hire_date), MAX(hire_date) FROM employees;
```

### GROUP BY

```sql
-- Count employees per department
SELECT department_id, COUNT(*) AS employee_count
FROM employees
GROUP BY department_id;

-- Multiple grouping columns
SELECT department_id, job_title, COUNT(*) AS count
FROM employees
GROUP BY department_id, job_title;
```

### GROUP BY with Aggregates

```sql
SELECT
    department_id,
    COUNT(*) AS emp_count,
    SUM(salary) AS total_salary,
    AVG(salary) AS avg_salary,
    MIN(salary) AS min_salary,
    MAX(salary) AS max_salary
FROM employees
GROUP BY department_id;
```

### HAVING Clause

Filters groups after aggregation:

```sql
-- Departments with more than 5 employees
SELECT department_id, COUNT(*) AS emp_count
FROM employees
GROUP BY department_id
HAVING COUNT(*) > 5;

-- Departments with average salary over 50000
SELECT department_id, AVG(salary) AS avg_salary
FROM employees
GROUP BY department_id
HAVING AVG(salary) > 50000;
```

### GROUP BY with Expressions

```sql
-- Group by year
SELECT
    EXTRACT(YEAR FROM order_date) AS order_year,
    COUNT(*) AS order_count,
    SUM(total_amount) AS total_sales
FROM orders
GROUP BY EXTRACT(YEAR FROM order_date)
ORDER BY order_year;
```

---

## ORDER BY

### Basic Ordering

```sql
-- Ascending (default)
SELECT * FROM employees ORDER BY last_name;

-- Descending
SELECT * FROM employees ORDER BY salary DESC;

-- Multiple columns
SELECT * FROM employees ORDER BY department_id ASC, salary DESC;
```

### Order by Column Position

```sql
-- Order by first column
SELECT first_name, last_name, salary
FROM employees
ORDER BY 1;

-- Order by third column descending
SELECT first_name, last_name, salary
FROM employees
ORDER BY 3 DESC;
```

### Order by Expression

```sql
SELECT first_name, last_name, salary + bonus AS total_comp
FROM employees
ORDER BY salary + bonus DESC;
```

### NULLS FIRST / NULLS LAST

Control where NULL values appear in sorted results:

```sql
-- NULLs at the beginning
SELECT * FROM employees ORDER BY manager_id NULLS FIRST;

-- NULLs at the end
SELECT * FROM employees ORDER BY manager_id NULLS LAST;

-- Combined with ASC/DESC
SELECT * FROM employees ORDER BY commission DESC NULLS LAST;
```

### Collation-Sensitive Ordering

```sql
SELECT * FROM customers
ORDER BY company_name COLLATE UNICODE;
```

---

## UNION and Set Operations

### UNION

Combines results from multiple queries, removing duplicates:

```sql
SELECT employee_id, first_name, last_name FROM employees
UNION
SELECT contractor_id, first_name, last_name FROM contractors;
```

### UNION ALL

Combines results keeping duplicates (faster):

```sql
SELECT product_id, product_name FROM products_us
UNION ALL
SELECT product_id, product_name FROM products_eu;
```

### Multiple UNIONs

```sql
SELECT 'Q1' AS quarter, SUM(amount) FROM sales WHERE month IN (1,2,3)
UNION ALL
SELECT 'Q2', SUM(amount) FROM sales WHERE month IN (4,5,6)
UNION ALL
SELECT 'Q3', SUM(amount) FROM sales WHERE month IN (7,8,9)
UNION ALL
SELECT 'Q4', SUM(amount) FROM sales WHERE month IN (10,11,12);
```

### ORDER BY with UNION

```sql
-- ORDER BY applies to the entire result
SELECT employee_id, first_name FROM active_employees
UNION
SELECT employee_id, first_name FROM inactive_employees
ORDER BY first_name;
```

---

## Subqueries

### Scalar Subqueries

Returns a single value:

```sql
SELECT
    employee_id,
    first_name,
    salary,
    (SELECT AVG(salary) FROM employees) AS company_avg
FROM employees;
```

### Correlated Subqueries

References the outer query:

```sql
SELECT e.employee_id, e.first_name, e.salary
FROM employees e
WHERE e.salary > (
    SELECT AVG(salary)
    FROM employees
    WHERE department_id = e.department_id
);
```

### Subquery in FROM (Derived Tables)

```sql
SELECT dept_summary.department_id, dept_summary.total_salary
FROM (
    SELECT department_id, SUM(salary) AS total_salary
    FROM employees
    GROUP BY department_id
) AS dept_summary
WHERE dept_summary.total_salary > 100000;
```

### Subquery with IN

```sql
SELECT * FROM products
WHERE supplier_id IN (
    SELECT supplier_id FROM suppliers
    WHERE country = 'USA'
);
```

### Subquery with NOT IN

```sql
SELECT * FROM customers
WHERE customer_id NOT IN (
    SELECT DISTINCT customer_id FROM orders
    WHERE order_date >= '2024-01-01'
);
```

### Subquery with EXISTS

```sql
SELECT * FROM departments d
WHERE EXISTS (
    SELECT 1 FROM employees e
    WHERE e.department_id = d.department_id
);
```

### Subquery with NOT EXISTS

```sql
SELECT * FROM products p
WHERE NOT EXISTS (
    SELECT 1 FROM order_items oi
    WHERE oi.product_id = p.product_id
);
```

---

## FOR UPDATE (Locking)

### Description

Locks selected rows for update within a transaction.

### Syntax

```sql
SELECT column_list
FROM table_reference
WHERE condition
FOR UPDATE [OF column_list] [WITH LOCK]
```

### Examples

```sql
-- Lock rows for update
SELECT * FROM accounts
WHERE account_id = 12345
FOR UPDATE;

-- Lock specific columns
SELECT account_id, balance FROM accounts
WHERE customer_id = 100
FOR UPDATE OF balance;

-- WITH LOCK for pessimistic locking
SELECT * FROM inventory
WHERE product_id = 500
FOR UPDATE WITH LOCK;
```

### Usage Notes

- Must be within a transaction
- Other transactions will wait or fail when trying to access locked rows
- Locks are released at COMMIT or ROLLBACK

---

## Common Table Expressions (CTEs)

### WITH Clause

```sql
WITH department_totals AS (
    SELECT department_id, SUM(salary) AS total_salary
    FROM employees
    GROUP BY department_id
)
SELECT d.department_name, dt.total_salary
FROM departments d
JOIN department_totals dt ON d.department_id = dt.department_id
ORDER BY dt.total_salary DESC;
```

### Multiple CTEs

```sql
WITH
    active_customers AS (
        SELECT customer_id, company_name
        FROM customers
        WHERE status = 'ACTIVE'
    ),
    recent_orders AS (
        SELECT customer_id, COUNT(*) AS order_count
        FROM orders
        WHERE order_date >= '2024-01-01'
        GROUP BY customer_id
    )
SELECT ac.company_name, COALESCE(ro.order_count, 0) AS orders_2024
FROM active_customers ac
LEFT JOIN recent_orders ro ON ac.customer_id = ro.customer_id;
```

### Recursive CTE

```sql
-- Organizational hierarchy
WITH RECURSIVE org_chart AS (
    -- Anchor: top-level employees (no manager)
    SELECT employee_id, first_name, manager_id, 1 AS level
    FROM employees
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive: employees with managers
    SELECT e.employee_id, e.first_name, e.manager_id, oc.level + 1
    FROM employees e
    JOIN org_chart oc ON e.manager_id = oc.employee_id
)
SELECT * FROM org_chart ORDER BY level, first_name;
```

---

## CASE Expressions

### Simple CASE

```sql
SELECT
    employee_id,
    first_name,
    CASE department_id
        WHEN 10 THEN 'Sales'
        WHEN 20 THEN 'Marketing'
        WHEN 30 THEN 'Engineering'
        ELSE 'Other'
    END AS department_name
FROM employees;
```

### Searched CASE

```sql
SELECT
    product_id,
    product_name,
    price,
    CASE
        WHEN price < 10 THEN 'Budget'
        WHEN price < 50 THEN 'Standard'
        WHEN price < 100 THEN 'Premium'
        ELSE 'Luxury'
    END AS price_category
FROM products;
```

### CASE in ORDER BY

```sql
SELECT * FROM employees
ORDER BY
    CASE status
        WHEN 'ACTIVE' THEN 1
        WHEN 'ON_LEAVE' THEN 2
        WHEN 'INACTIVE' THEN 3
        ELSE 4
    END;
```

---

## Conditional Functions

### COALESCE

Returns first non-NULL value:

```sql
SELECT
    employee_id,
    COALESCE(phone_work, phone_mobile, phone_home, 'No phone') AS contact_phone
FROM employees;
```

### NULLIF

Returns NULL if values are equal:

```sql
SELECT
    product_id,
    -- Avoid division by zero
    total_revenue / NULLIF(units_sold, 0) AS avg_price
FROM products;
```

### IIF (Firebird 3.0+)

Inline IF-THEN-ELSE:

```sql
SELECT
    employee_id,
    IIF(salary > 50000, 'High', 'Normal') AS salary_level
FROM employees;
```

---

## Known Limitations

### Partial Implementation

**SELECT with FIRST/SKIP**
- Basic FIRST and SKIP work correctly
- Status: Implemented

**JOINs**
- V2 pipeline has limitations on multi-table joins
- Complex join conditions may not execute correctly
- Status: Partial

**SIMILAR TO**
- Parsed as regex pattern matching
- Status: Implemented

**CONTAINING / STARTING WITH**
- Parsed but not encoded in V2 generator
- Status: Stubbed

**Schema-qualified identifiers**
- `schema.table.column` syntax is rejected in Firebird parser
- Firebird doesn't support schemas (by design)
- Status: Not applicable

### V2 Pipeline Limitations

**Select List Encoding**
- Some complex expressions may not be properly encoded
- Computed columns may have issues

**Multi-Table Queries**
- Complex joins may encounter bytecode issues
- Consider using simpler join patterns

### Specification Deltas

**ROWS Clause**
- Parser accepts ROWS syntax
- May have execution issues in complex queries

**FOR UPDATE WITH LOCK**
- Parsed correctly
- Lock behavior depends on transaction configuration

### Workarounds

**For complex joins**:
```sql
-- Instead of complex multi-way join
-- Consider using subqueries or CTEs
WITH dept_data AS (
    SELECT department_id, department_name FROM departments
)
SELECT e.*, d.department_name
FROM employees e
JOIN dept_data d ON e.department_id = d.department_id;
```

**For schema qualification**:
```sql
-- Firebird doesn't use schemas
-- Use database aliases or naming conventions instead
SELECT * FROM employees;  -- No schema prefix
```

---

## Quick Reference

### Row Limiting

| Syntax | Description |
|--------|-------------|
| `FIRST 10` | First 10 rows |
| `FIRST 10 SKIP 20` | 10 rows after skipping 20 |
| `ROWS 10` | First 10 rows |
| `ROWS 11 TO 20` | Rows 11 through 20 |

### Join Types

| Type | Description |
|------|-------------|
| `INNER JOIN` | Matching rows only |
| `LEFT JOIN` | All left rows + matching right |
| `RIGHT JOIN` | All right rows + matching left |
| `FULL OUTER JOIN` | All rows from both tables |
| `CROSS JOIN` | Cartesian product |

### Pattern Matching

| Operator | Description |
|----------|-------------|
| `LIKE` | Simple patterns (% and _) |
| `SIMILAR TO` | Regular expressions |
| `CONTAINING` | Contains substring |
| `STARTING WITH` | Starts with string |

---

## See Also

- [DML Modification](07_dml_modification.md) - INSERT, UPDATE, DELETE
- [Operators](12_operators.md) - Operator reference
- [Functions](14_functions.md) - Built-in functions
- [Transactions](08_transactions.md) - Transaction control for FOR UPDATE

