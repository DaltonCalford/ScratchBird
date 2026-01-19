# Native V2 SQL - DML SELECT

## Overview

This document describes SELECT query capabilities in ScratchBird's Native V2 SQL dialect. SELECT statements retrieve data from tables and views, supporting filtering, joining, grouping, ordering, and set operations.

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## SELECT Statement

### Description

Retrieves data from one or more tables or views, with extensive options for filtering, joining, aggregation, and ordering.

### Syntax

```sql
SELECT [ALL | DISTINCT] <select_list>
FROM <table_reference> [<join_clause> ...]
[WHERE <condition>]
[GROUP BY <expression> [, ...]]
[HAVING <condition>]
[WINDOW <window_definition> [, ...]]
[ORDER BY <expression> [ASC | DESC] [NULLS FIRST | NULLS LAST] [, ...]]
[LIMIT <count>] [OFFSET <start>]
[{ UNION | INTERSECT | EXCEPT } [ALL | DISTINCT] <select_statement>]
[FOR UPDATE | FOR SHARE [OF <table> [, ...]] [NOWAIT | SKIP LOCKED]]
```

### Parameters

- **ALL | DISTINCT**: Include all rows (default) or only unique rows
- **select_list**: Columns, expressions, or * for all columns
- **FROM**: Table or view to query from
- **WHERE**: Row filter condition
- **GROUP BY**: Group rows for aggregation
- **HAVING**: Filter grouped rows
- **WINDOW**: Define window functions
- **ORDER BY**: Sort result rows
- **LIMIT**: Maximum number of rows to return
- **OFFSET**: Number of rows to skip
- **UNION**: Combine results from multiple queries
- **FOR UPDATE/SHARE**: Lock rows for update

### Examples

**Example 1: Simple SELECT**
```sql
SELECT id, name, email
FROM users;
```

**Example 2: SELECT with WHERE clause**
```sql
SELECT *
FROM orders
WHERE status = 'pending'
  AND amount > 100;
```

**Example 3: SELECT with JOIN**
```sql
SELECT o.id, o.order_date, c.name AS customer_name, o.amount
FROM orders o
JOIN customers c ON o.customer_id = c.id
WHERE o.status = 'completed';
```

**Example 4: SELECT with GROUP BY and aggregation**
```sql
SELECT customer_id, COUNT(*) AS order_count, SUM(amount) AS total_spent
FROM orders
GROUP BY customer_id
HAVING SUM(amount) > 1000;
```

**Example 5: SELECT with ORDER BY**
```sql
SELECT name, created_at
FROM users
ORDER BY created_at DESC, name ASC
LIMIT 10;
```

**Example 6: SELECT with DISTINCT**
```sql
SELECT DISTINCT status
FROM orders;
```

**Example 7: SELECT with subquery**
```sql
SELECT *
FROM products
WHERE category_id IN (
    SELECT id FROM categories WHERE active = true
);
```

**Example 8: SELECT with UNION**
```sql
SELECT name FROM customers WHERE country = 'USA'
UNION
SELECT name FROM suppliers WHERE country = 'USA';
```

**Example 9: SELECT with window functions**
```sql
SELECT
    name,
    salary,
    department,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank
FROM employees;
```

**Example 10: SELECT with FOR UPDATE**
```sql
SELECT * FROM accounts
WHERE account_id = 12345
FOR UPDATE NOWAIT;
```

---

## JOINs

### INNER JOIN
```sql
SELECT *
FROM table1 t1
INNER JOIN table2 t2 ON t1.id = t2.foreign_id;
```

### LEFT OUTER JOIN
```sql
SELECT *
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id;
```

### RIGHT OUTER JOIN
```sql
SELECT *
FROM orders o
RIGHT JOIN customers c ON o.customer_id = c.id;
```

### FULL OUTER JOIN
```sql
SELECT *
FROM table1 t1
FULL OUTER JOIN table2 t2 ON t1.id = t2.id;
```

### CROSS JOIN
```sql
SELECT *
FROM colors
CROSS JOIN sizes;
```

### Self Join
```sql
SELECT e.name AS employee, m.name AS manager
FROM employees e
LEFT JOIN employees m ON e.manager_id = m.id;
```

---

## Aggregate Functions

- `COUNT(*)`, `COUNT(column)` - Count rows
- `SUM(column)` - Sum values
- `AVG(column)` - Average value
- `MIN(column)`, `MAX(column)` - Minimum/maximum value
- `STRING_AGG(column, delimiter)` - Concatenate strings
- `ARRAY_AGG(column)` - Aggregate into array
- `JSON_AGG(column)` - Aggregate into JSON array

**Example:**
```sql
SELECT
    department,
    COUNT(*) AS employee_count,
    AVG(salary) AS avg_salary,
    MIN(hire_date) AS earliest_hire,
    STRING_AGG(name, ', ' ORDER BY name) AS employees
FROM employees
GROUP BY department;
```

---

## Window Functions

Window functions perform calculations across sets of rows related to the current row.

**Ranking Functions:**
- `ROW_NUMBER()` - Sequential number within partition
- `RANK()` - Rank with gaps for ties
- `DENSE_RANK()` - Rank without gaps
- `NTILE(n)` - Divide rows into n buckets

**Aggregate Window Functions:**
- `SUM()`, `AVG()`, `COUNT()`, `MIN()`, `MAX()` - Aggregate over window

**Offset Functions:**
- `LAG(column, offset)` - Access previous row
- `LEAD(column, offset)` - Access next row
- `FIRST_VALUE(column)` - First value in window
- `LAST_VALUE(column)` - Last value in window

**Example:**
```sql
SELECT
    order_date,
    amount,
    SUM(amount) OVER (ORDER BY order_date) AS running_total,
    AVG(amount) OVER (ORDER BY order_date ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) AS moving_avg,
    LAG(amount, 1) OVER (ORDER BY order_date) AS previous_amount
FROM orders;
```

---

## Set Operations

### UNION
Combines results, removing duplicates:
```sql
SELECT name FROM table1
UNION
SELECT name FROM table2;
```

### UNION ALL
Combines results, keeping duplicates:
```sql
SELECT name FROM table1
UNION ALL
SELECT name FROM table2;
```

### INTERSECT
Returns rows present in both queries:
```sql
SELECT id FROM customers
INTERSECT
SELECT customer_id FROM orders;
```

### EXCEPT
Returns rows from first query not in second:
```sql
SELECT id FROM all_users
EXCEPT
SELECT id FROM active_users;
```

---

## Subqueries

### Scalar Subquery
Returns single value:
```sql
SELECT name, (SELECT AVG(amount) FROM orders) AS avg_order
FROM customers;
```

### IN Subquery
```sql
SELECT *
FROM products
WHERE category_id IN (SELECT id FROM categories WHERE active = true);
```

### EXISTS Subquery
```sql
SELECT *
FROM customers c
WHERE EXISTS (
    SELECT 1 FROM orders o WHERE o.customer_id = c.id
);
```

### Correlated Subquery
```sql
SELECT *
FROM products p
WHERE price > (
    SELECT AVG(price)
    FROM products
    WHERE category = p.category
);
```

---

## Known Limitations

### Missing Features

**Common Table Expressions (WITH):**
- WITH (CTE) syntax not parsed in V2
- WITH RECURSIVE not supported
- Named subqueries not available
- Workaround: Use subqueries or views
- Spec reference: `/docs/specifications/dml/01_SELECT.md`

**Advanced Join Features:**
- NATURAL JOIN not supported
- USING clause not supported (use ON instead)
- Some join type combinations may have limitations

**Window Function Features:**
- Some advanced window frame specifications may be limited
- RANGE frames vs ROWS frames need validation
- GROUPS frame type not supported

**Query Hints:**
- Optimizer hints not supported
- Index hints not available
- Join order hints not parsed

### Partial Implementation

**Set Operations:**
- UNION, INTERSECT, EXCEPT implemented
- ALL vs DISTINCT behavior needs validation
- Complex nested set operations may have limitations

**Locking Clauses:**
- FOR UPDATE/FOR SHARE parsed
- NOWAIT and SKIP LOCKED options parsed
- Full lock semantics with MGA transactions need validation
- Spec reference: `/docs/specifications/transaction/TRANSACTION_MAIN.md`

### Spec Deltas

**SELECT Features:**
- Some PostgreSQL-style extensions are present (FROM in UPDATE/DELETE)
- These may need explicit spec approval or documentation
- Query optimizer coverage incomplete
- Spec reference: `/docs/specifications/query/QUERY_OPTIMIZER_SPEC.md`

### General Notes

- All SELECT operations are transactional
- Query execution uses MGA snapshot isolation
- Full implementation status in `/docs/audit/parsers/V2/SUMMARY.md`
- Critical findings in `/docs/audit/parsers/CRITICAL_FINDINGS.md`
