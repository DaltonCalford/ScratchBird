# DML Operations Complete Specification

**Last Updated:** November 23, 2025
**Status:** Alpha 1 - 100% Complete
**Purpose:** Complete specification of all Data Manipulation Language operations

---

## Overview

ScratchBird implements a complete set of DML operations with advanced features including CTEs, window functions, RETURNING clauses, and MERGE statements. All operations are MGA-compliant using Firebird's Multi-Generational Architecture with TIP-based visibility.

**Implementation Status:** ✅ 100% Complete

---

## Core DML Statements

### 1. INSERT - Insert Data

**Opcode:** `INSERT` (0x11)

**Syntax:**

```sql
[ WITH cte_name AS ( ... ) [, ...] ]
INSERT INTO table_name [ ( column_name [, ...] ) ]
{
    VALUES ( { expression | DEFAULT } [, ...] ) [, ...]
    | select_statement
    | DEFAULT VALUES
}
[ ON CONFLICT ( conflict_target ) DO { NOTHING | UPDATE SET ... } ]
[ RETURNING { * | output_expression [ [ AS ] output_name ] } [, ...] ];
```

**Features:**

#### 1.1 Single-Row Insert

Insert exactly one row into a table.

```sql
-- Explicitly list columns
INSERT INTO products (product_id, product_name, unit_price)
VALUES (101, 'Chai', 18.00);

-- Omit column list (requires values for all columns in order)
INSERT INTO products VALUES (102, 'Chang', 19.00, 100);
```

**Status:** ✅ 100% Complete

---

#### 1.2 Multi-Row Insert

Insert multiple rows in a single statement.

```sql
INSERT INTO employees (first_name, last_name, department_id) VALUES
    ('John', 'Smith', 5),
    ('Jane', 'Doe', 5),
    ('Peter', 'Jones', 2);
```

**Performance:** Significantly more efficient than multiple single-row INSERTs

**Status:** ✅ 100% Complete

---

#### 1.3 Insert from Query (INSERT ... SELECT)

Insert the result set of a SELECT statement.

```sql
-- Archive all completed orders from the last year
INSERT INTO orders_archive (order_id, customer_id, order_date, total)
SELECT order_id, customer_id, order_date, total
FROM orders
WHERE status = 'COMPLETED' AND order_date < (CURRENT_DATE - INTERVAL '1 year');
```

**Status:** ✅ 100% Complete

---

#### 1.4 Upsert with ON CONFLICT

Handle duplicate key conflicts with either UPDATE or IGNORE.

```sql
-- Update existing row if conflict
INSERT INTO products (product_name, units_in_stock)
VALUES ('New Widget', 10)
ON CONFLICT (product_name)
DO UPDATE SET
    units_in_stock = products.units_in_stock + EXCLUDED.units_in_stock;
    -- `EXCLUDED` refers to the values from the proposed INSERT

-- Ignore duplicates
INSERT INTO event_log (event_id, event_data)
VALUES (12345, '{"status":"login"}')
ON CONFLICT (event_id)
DO NOTHING;
```

**Conflict Targets:**
- Single column: `ON CONFLICT (column_name)`
- Multiple columns: `ON CONFLICT (col1, col2, ...)`
- Unique constraint: `ON CONFLICT ON CONSTRAINT constraint_name`

**Status:** ✅ 100% Complete

---

#### 1.5 RETURNING Clause

Return values from inserted rows.

```sql
-- Get server-generated values (UUID, timestamps, etc.)
INSERT INTO users (username, email)
VALUES ('j.doe', 'jane.doe@example.com')
RETURNING user_id, created_at;

-- Result: single row with generated values
-- | user_id                                | created_at                   |
-- |----------------------------------------|------------------------------|
-- | 'c4a4e8d3-3b8e-4a8e-9b3b-1b3b3b3b3b3b' | '2024-09-15 14:30:00.123456' |
```

**RETURNING expressions:**
- `*` - All columns
- Column names
- Expressions
- Aliased outputs

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

#### 1.6 DEFAULT VALUES

Insert a row with all default values.

```sql
INSERT INTO audit_log DEFAULT VALUES
RETURNING log_id, created_at;
```

**Status:** ✅ 100% Complete

---

### 2. UPDATE - Modify Existing Data

**Opcode:** `UPDATE` (0xC3)

**Syntax:**

```sql
[ WITH cte_name AS ( ... ) [, ...] ]
UPDATE table_name
SET { column_name = { expression | DEFAULT } } [, ...]
[ FROM from_list ]
[ WHERE condition ]
[ RETURNING { * | output_expression [ [ AS ] output_name ] } [, ...] ];
```

**Features:**

#### 2.1 Basic Update

Update rows matching a condition.

```sql
-- Update single column
UPDATE employees
SET salary = salary * 1.10
WHERE department_id = 5;

-- Update multiple columns
UPDATE products
SET unit_price = 25.00,
    units_in_stock = units_in_stock - 1
WHERE product_id = 101;
```

**Status:** ✅ 100% Complete

---

#### 2.2 Update with FROM Clause

Join with other tables for complex updates.

```sql
-- Update based on joined data
UPDATE employees e
SET salary = e.salary * (1 + d.raise_percent / 100.0)
FROM departments d
WHERE e.department_id = d.department_id
  AND d.performance_rating = 'EXCELLENT';
```

**Status:** ✅ 100% Complete

---

#### 2.3 Update with RETURNING

Return modified rows.

```sql
UPDATE inventory
SET quantity = quantity - 5
WHERE product_id = 42
RETURNING product_id, quantity, last_modified;
```

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

#### 2.4 Update with DEFAULT

Reset columns to their default values.

```sql
UPDATE users
SET preferences = DEFAULT
WHERE user_id = 'abc-123';
```

**Status:** ✅ 100% Complete

---

#### 2.5 Deferred Constraint Checking

Updates respect deferred constraints.

```sql
-- Set constraint checking to deferred
SET CONSTRAINTS ALL DEFERRED;

-- Updates that temporarily violate constraints are allowed
UPDATE accounts SET balance = balance - 1000 WHERE account_id = 1;
UPDATE accounts SET balance = balance + 1000 WHERE account_id = 2;

-- Constraints checked at COMMIT
COMMIT;
```

**Status:** ✅ 100% Complete (Alpha 1 - Constraint Features)

---

### 3. DELETE - Remove Data

**Opcode:** `DELETE` (0xC4)

**Syntax:**

```sql
[ WITH cte_name AS ( ... ) [, ...] ]
DELETE FROM table_name
[ USING using_list ]
[ WHERE condition ]
[ RETURNING { * | output_expression [ [ AS ] output_name ] } [, ...] ];
```

**Features:**

#### 3.1 Basic Delete

Delete rows matching a condition.

```sql
-- Delete with simple condition
DELETE FROM orders
WHERE order_date < '2020-01-01';

-- Delete all rows (use TRUNCATE instead for better performance)
DELETE FROM temp_data;
```

**Status:** ✅ 100% Complete

---

#### 3.2 Delete with USING Clause

Join with other tables for complex deletes.

```sql
-- Delete based on joined data
DELETE FROM order_items oi
USING orders o
WHERE oi.order_id = o.order_id
  AND o.status = 'CANCELLED'
  AND o.order_date < CURRENT_DATE - INTERVAL '90 days';
```

**Status:** ✅ 100% Complete

---

#### 3.3 Delete with RETURNING

Return deleted rows.

```sql
-- Archive deleted rows
WITH deleted_orders AS (
    DELETE FROM orders
    WHERE status = 'CANCELLED'
    RETURNING *
)
INSERT INTO orders_archive
SELECT * FROM deleted_orders;
```

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

#### 3.4 CASCADE Delete

Foreign key CASCADE actions automatically delete child rows.

```sql
-- CASCADE defined at table level
CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(customer_id) ON DELETE CASCADE
);

-- Deleting customer automatically deletes all their orders
DELETE FROM customers WHERE customer_id = 42;
```

**Foreign Key Actions:**
- `NO_ACTION` - Error if references exist (default)
- `RESTRICT` - Error immediately (same as NO_ACTION)
- `CASCADE` - Delete child rows
- `SET NULL` - Set FK columns to NULL
- `SET DEFAULT` - Set FK columns to DEFAULT

**Status:** ✅ 100% Complete (Alpha 1 - Foreign Key Phase C)

---

### 4. SELECT - Query Data

**Opcode:** `SELECT` (0x12)

**Syntax:**

```sql
[ WITH [ RECURSIVE ] cte_name AS ( query ) [, ...] ]
SELECT [ ALL | DISTINCT ]
    { * | expression [ [ AS ] output_name ] } [, ...]
[ FROM from_item [, ...] ]
[ WHERE condition ]
[ GROUP BY grouping_element [, ...] ]
[ HAVING condition ]
[ WINDOW window_name AS ( window_definition ) [, ...] ]
[ { UNION | INTERSECT | EXCEPT } [ ALL | DISTINCT ] select ]
[ ORDER BY expression [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] ]
[ LIMIT { count | ALL } ]
[ OFFSET start [ ROW | ROWS ] ]
[ FOR { UPDATE | SHARE } [ OF table_name [, ...] ] [ NOWAIT | SKIP LOCKED ] ];
```

**Status:** ✅ 100% Complete

---

#### 4.1 Basic SELECT

Simple column retrieval.

```sql
-- Select specific columns
SELECT first_name, last_name, email
FROM employees;

-- Select all columns
SELECT * FROM products;

-- Select with WHERE filter
SELECT product_name, unit_price
FROM products
WHERE unit_price > 50.00;
```

**Status:** ✅ 100% Complete

---

#### 4.2 DISTINCT

Eliminate duplicate rows.

```sql
-- Distinct values
SELECT DISTINCT department_id
FROM employees;

-- Distinct on multiple columns
SELECT DISTINCT department_id, job_title
FROM employees;
```

**Status:** ✅ 100% Complete

---

#### 4.3 JOIN Operations

All join types supported.

```sql
-- INNER JOIN
SELECT e.first_name, d.department_name
FROM employees e
INNER JOIN departments d ON e.department_id = d.department_id;

-- LEFT OUTER JOIN
SELECT c.customer_name, o.order_id
FROM customers c
LEFT JOIN orders o ON c.customer_id = o.customer_id;

-- RIGHT OUTER JOIN
SELECT e.first_name, d.department_name
FROM employees e
RIGHT JOIN departments d ON e.department_id = d.department_id;

-- FULL OUTER JOIN
SELECT e.first_name, d.department_name
FROM employees e
FULL OUTER JOIN departments d ON e.department_id = d.department_id;

-- CROSS JOIN
SELECT p.product_name, c.category_name
FROM products p
CROSS JOIN categories c;

-- NATURAL JOIN
SELECT *
FROM employees
NATURAL JOIN departments;
```

**Join Types:**
- ✅ INNER JOIN
- ✅ LEFT JOIN / LEFT OUTER JOIN
- ✅ RIGHT JOIN / RIGHT OUTER JOIN
- ✅ FULL JOIN / FULL OUTER JOIN
- ✅ CROSS JOIN
- ✅ NATURAL JOIN

**Status:** ✅ 100% Complete (Phase 1 Task 3.3)

---

#### 4.4 Aggregation (GROUP BY, HAVING)

Aggregate functions with grouping.

```sql
-- GROUP BY with aggregate functions
SELECT department_id, COUNT(*) as employee_count, AVG(salary) as avg_salary
FROM employees
GROUP BY department_id;

-- HAVING clause (filter after aggregation)
SELECT department_id, AVG(salary) as avg_salary
FROM employees
GROUP BY department_id
HAVING AVG(salary) > 50000;
```

**Aggregate Functions:**
- SUM, AVG, MIN, MAX, COUNT
- STDDEV, VARIANCE
- ARRAY_AGG
- String aggregation functions

**Status:** ✅ 100% Complete

---

#### 4.5 ORDER BY

Sort result rows.

```sql
-- Single column ascending
SELECT * FROM products ORDER BY unit_price;

-- Multiple columns with direction
SELECT * FROM employees ORDER BY department_id ASC, salary DESC;

-- NULL handling
SELECT * FROM employees ORDER BY commission NULLS LAST;

-- Order by expression
SELECT first_name, last_name, salary
FROM employees
ORDER BY salary * 1.1 DESC;
```

**Features:**
- ✅ ASC / DESC
- ✅ NULLS FIRST / NULLS LAST
- ✅ Multiple columns
- ✅ Expression ordering

**Status:** ✅ 100% Complete

---

#### 4.6 LIMIT and OFFSET

Pagination support.

```sql
-- Get first 10 rows
SELECT * FROM products LIMIT 10;

-- Skip first 20 rows, then get 10
SELECT * FROM products LIMIT 10 OFFSET 20;

-- Equivalent syntax
SELECT * FROM products OFFSET 20 ROWS FETCH NEXT 10 ROWS ONLY;
```

**Status:** ✅ 100% Complete

---

#### 4.7 Subqueries

Nested queries in various contexts.

```sql
-- Scalar subquery (single value)
SELECT first_name, salary,
       (SELECT AVG(salary) FROM employees) as avg_salary
FROM employees;

-- IN subquery
SELECT * FROM employees
WHERE department_id IN (SELECT department_id FROM departments WHERE location = 'NY');

-- EXISTS subquery
SELECT * FROM customers c
WHERE EXISTS (SELECT 1 FROM orders o WHERE o.customer_id = c.customer_id);

-- NOT EXISTS subquery
SELECT * FROM products p
WHERE NOT EXISTS (SELECT 1 FROM order_items oi WHERE oi.product_id = p.product_id);

-- ANY/ALL subqueries
SELECT * FROM employees
WHERE salary > ALL (SELECT salary FROM employees WHERE department_id = 10);
```

**Subquery Types:**
- ✅ Scalar subqueries
- ✅ IN / NOT IN
- ✅ EXISTS / NOT EXISTS
- ✅ ANY / ALL
- ✅ ARRAY subqueries

**Status:** ✅ 100% Complete (Phase 2 Wave 2)

---

#### 4.8 Common Table Expressions (CTEs)

Temporary named result sets.

```sql
-- Non-recursive CTE
WITH high_earners AS (
    SELECT * FROM employees WHERE salary > 100000
),
department_stats AS (
    SELECT department_id, AVG(salary) as avg_salary
    FROM employees
    GROUP BY department_id
)
SELECT he.first_name, he.salary, ds.avg_salary
FROM high_earners he
JOIN department_stats ds ON he.department_id = ds.department_id;

-- Recursive CTE
WITH RECURSIVE subordinates AS (
    -- Base case
    SELECT employee_id, first_name, manager_id, 1 as level
    FROM employees
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive case
    SELECT e.employee_id, e.first_name, e.manager_id, s.level + 1
    FROM employees e
    JOIN subordinates s ON e.manager_id = s.employee_id
)
SELECT * FROM subordinates ORDER BY level;
```

**CTE Features:**
- ✅ Non-recursive CTEs
- ✅ Recursive CTEs
- ✅ Multiple CTEs in single query
- ✅ CTE references in main query
- ✅ Cycle detection in recursive CTEs

**Status:** ✅ 100% Complete (Phase 2 Wave 2 - Advanced SQL)

---

#### 4.9 Window Functions

Analytical functions over row windows.

```sql
-- ROW_NUMBER with ORDER BY
SELECT first_name, salary,
       ROW_NUMBER() OVER (ORDER BY salary DESC) as row_num
FROM employees;

-- PARTITION BY with ORDER BY
SELECT first_name, department_id, salary,
       RANK() OVER (PARTITION BY department_id ORDER BY salary DESC) as dept_rank
FROM employees;

-- Multiple window functions
SELECT first_name, salary,
       ROW_NUMBER() OVER w as row_num,
       RANK() OVER w as rank,
       DENSE_RANK() OVER w as dense_rank
FROM employees
WINDOW w AS (ORDER BY salary DESC);

-- Frame specifications
SELECT order_id, order_date, amount,
       SUM(amount) OVER (ORDER BY order_date ROWS BETWEEN 2 PRECEDING AND CURRENT ROW) as running_sum
FROM orders;

-- LAG and LEAD
SELECT order_id, order_date, amount,
       LAG(amount, 1) OVER (ORDER BY order_date) as prev_amount,
       LEAD(amount, 1) OVER (ORDER BY order_date) as next_amount
FROM orders;
```

**Window Functions:**
- ✅ ROW_NUMBER()
- ✅ RANK()
- ✅ DENSE_RANK()
- ✅ LAG(expr [, offset [, default]])
- ✅ LEAD(expr [, offset [, default]])
- ✅ FIRST_VALUE(expr)
- ✅ LAST_VALUE(expr)
- ✅ NTH_VALUE(expr, n)

**Frame Specifications:**
- ✅ PARTITION BY
- ✅ ORDER BY
- ✅ ROWS BETWEEN ... AND ...
- ✅ RANGE BETWEEN ... AND ...

**Status:** ✅ 100% Complete (Phase 1 Task 6.3)

---

#### 4.10 Set Operations

Combine results from multiple queries.

```sql
-- UNION (removes duplicates)
SELECT first_name FROM employees
UNION
SELECT first_name FROM contractors;

-- UNION ALL (keeps duplicates)
SELECT product_id FROM products_us
UNION ALL
SELECT product_id FROM products_eu;

-- INTERSECT (common rows)
SELECT email FROM users
INTERSECT
SELECT email FROM subscribers;

-- EXCEPT (rows in first but not second)
SELECT email FROM all_contacts
EXCEPT
SELECT email FROM unsubscribed;
```

**Set Operations:**
- ✅ UNION
- ✅ UNION ALL
- ✅ INTERSECT
- ✅ INTERSECT ALL
- ✅ EXCEPT
- ✅ EXCEPT ALL

**Status:** ✅ 100% Complete (Phase 2 Wave 2 - Advanced SQL)

---

### 5. MERGE - Conditional Insert/Update/Delete

**Opcodes:**
- `EXT_MERGE_START` (0x4F)
- `EXT_MERGE_SOURCE` (0x50)
- `EXT_MERGE_ON` (0x51)
- `EXT_MERGE_WHEN_MATCHED` (0x52)
- `EXT_MERGE_WHEN_NOT_MATCHED` (0x53)
- `EXT_MERGE_WHEN_NOT_MATCHED_SOURCE` (0x54)
- `EXT_MERGE_END` (0x55)

**Syntax:**

```sql
MERGE INTO target_table
USING source_table
ON merge_condition
WHEN MATCHED [ AND condition ] THEN
    { UPDATE SET ... | DELETE }
WHEN NOT MATCHED [ AND condition ] THEN
    INSERT ( columns ) VALUES ( values )
WHEN NOT MATCHED BY SOURCE [ AND condition ] THEN
    { UPDATE SET ... | DELETE };
```

**Features:**

#### 5.1 Basic MERGE (Upsert)

Insert or update based on match condition.

```sql
MERGE INTO products_inventory pi
USING incoming_shipments s
ON pi.product_id = s.product_id
WHEN MATCHED THEN
    UPDATE SET pi.quantity = pi.quantity + s.quantity
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity)
    VALUES (s.product_id, s.quantity);
```

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

#### 5.2 MERGE with Multiple WHEN Clauses

Different actions based on conditions.

```sql
MERGE INTO employees e
USING employee_updates u
ON e.employee_id = u.employee_id
WHEN MATCHED AND u.status = 'TERMINATED' THEN
    DELETE
WHEN MATCHED AND u.salary IS NOT NULL THEN
    UPDATE SET e.salary = u.salary, e.last_modified = NOW()
WHEN NOT MATCHED THEN
    INSERT (employee_id, first_name, last_name, salary)
    VALUES (u.employee_id, u.first_name, u.last_name, u.salary);
```

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

#### 5.3 MERGE with NOT MATCHED BY SOURCE

Handle rows in target but not in source.

```sql
MERGE INTO product_catalog pc
USING external_products ep
ON pc.sku = ep.sku
WHEN MATCHED THEN
    UPDATE SET pc.price = ep.price, pc.description = ep.description
WHEN NOT MATCHED THEN
    INSERT (sku, price, description)
    VALUES (ep.sku, ep.price, ep.description)
WHEN NOT MATCHED BY SOURCE THEN
    DELETE;  -- Remove products not in external source
```

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

## Advanced DML Features

### 6. RETURNING Clause

Return data from modified rows.

**Supported Statements:**
- ✅ INSERT ... RETURNING
- ✅ UPDATE ... RETURNING
- ✅ DELETE ... RETURNING
- ✅ MERGE ... RETURNING (future)

**RETURNING Capabilities:**

```sql
-- Return all columns
INSERT INTO logs DEFAULT VALUES RETURNING *;

-- Return specific columns
UPDATE users SET last_login = NOW()
WHERE user_id = 42
RETURNING user_id, username, last_login;

-- Return expressions
DELETE FROM temp_data
WHERE created_at < CURRENT_DATE - INTERVAL '7 days'
RETURNING *, NOW() as deleted_at;

-- Use in CTEs
WITH deleted_rows AS (
    DELETE FROM old_data RETURNING *
)
INSERT INTO archive_data SELECT * FROM deleted_rows;
```

**Status:** ✅ 100% Complete (Alpha 1 - Advanced SQL)

---

### 7. Deferred Constraints

Constraints checked at transaction commit instead of immediately.

```sql
-- Create deferrable constraint
CREATE TABLE accounts (
    account_id INTEGER PRIMARY KEY,
    balance DECIMAL CHECK (balance >= 0) DEFERRABLE INITIALLY IMMEDIATE
);

-- Defer constraint checking
BEGIN;
SET CONSTRAINTS ALL DEFERRED;

-- These would temporarily violate CHECK constraint
UPDATE accounts SET balance = -100 WHERE account_id = 1;
UPDATE accounts SET balance = 100 WHERE account_id = 1;

-- Constraint checked here (passes)
COMMIT;
```

**Constraint Deferrability:**
- ✅ DEFERRABLE INITIALLY IMMEDIATE
- ✅ DEFERRABLE INITIALLY DEFERRED
- ✅ NOT DEFERRABLE (default)
- ✅ SET CONSTRAINTS { ALL | name } { IMMEDIATE | DEFERRED }

**Status:** ✅ 100% Complete (Alpha 1 - Constraint Features)

---

### 8. Generated Columns

Computed columns (STORED and VIRTUAL).

```sql
-- STORED generated column (computed once, stored on disk)
CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    unit_price DECIMAL NOT NULL,
    quantity INTEGER NOT NULL,
    total_price DECIMAL GENERATED ALWAYS AS (unit_price * quantity) STORED
);

-- VIRTUAL generated column (computed on read)
CREATE TABLE employees (
    employee_id INTEGER PRIMARY KEY,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    full_name VARCHAR(101) GENERATED ALWAYS AS (first_name || ' ' || last_name) VIRTUAL
);
```

**Generated Column Types:**
- ✅ STORED - Computed and stored on disk
- ✅ VIRTUAL - Computed on read

**DML Behavior:**
- Generated columns cannot be explicitly set in INSERT/UPDATE
- Computed automatically based on expression
- STORED columns participate in indexes
- VIRTUAL columns computed on SELECT

**Status:** ✅ 100% Complete (Alpha 1 - Constraint Features)

---

### 9. Identity Columns

Auto-incrementing columns.

```sql
-- GENERATED ALWAYS AS IDENTITY (cannot override)
CREATE TABLE orders (
    order_id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    customer_id INTEGER,
    order_date DATE
);

-- GENERATED BY DEFAULT AS IDENTITY (can override)
CREATE TABLE products (
    product_id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    product_name VARCHAR(100)
);

-- Insert without specifying identity column
INSERT INTO orders (customer_id, order_date) VALUES (42, CURRENT_DATE);

-- Override BY DEFAULT identity
INSERT INTO products (product_id, product_name) VALUES (1000, 'Special Product');
```

**Identity Column Types:**
- ✅ GENERATED ALWAYS AS IDENTITY - Cannot override
- ✅ GENERATED BY DEFAULT AS IDENTITY - Can override with explicit value

**Status:** ✅ 100% Complete (Alpha 1 - Constraint Features)

---

## MGA-Specific Behaviors

### 10. Multi-Generational Architecture

All DML operations use Firebird MGA model.

**Key Characteristics:**

1. **In-Place Updates**
   - Primary record modified at original location
   - Old data moved to back-version chain
   - Indexes never change (stable TIDs)

2. **TIP-Based Visibility**
   - No snapshots
   - Transaction states looked up in TIP (Transaction Inventory Pages)
   - Visibility determined by `isVersionVisible(xid, current_xid)`

3. **Back-Versioning**
   - Version chains go newest → oldest
   - Primary record points backward to old versions
   - Garbage collection removes obsolete back-versions

4. **Stable TIDs**
   - Tuple IDs never change
   - Indexes remain valid across updates
   - No index bloat from updates

**Implementation Files:**
- `/home/user/ScratchBird/src/core/transaction_manager.cpp`
- `/home/user/ScratchBird/src/core/heap_page.cpp`
- `/home/user/ScratchBird/MGA_RULES.md`

---

## Transaction Isolation

DML operations respect transaction isolation levels.

**Isolation Levels:**

| Level | Description | Status |
|-------|-------------|--------|
| **READ COMMITTED** | See committed changes from other transactions | ✅ 100% |
| **SNAPSHOT** | Point-in-time consistent view | ✅ 100% |
| **READ COMMITTED READ CONSISTENCY** | Firebird 4.0+ style (with RECORD_VERSION) | ✅ 100% |

**Example:**

```sql
-- Start transaction with SNAPSHOT isolation
BEGIN TRANSACTION ISOLATION LEVEL SNAPSHOT;

-- All queries see consistent snapshot
SELECT COUNT(*) FROM orders;  -- e.g., 1000 rows

-- Another transaction inserts 10 rows and commits

SELECT COUNT(*) FROM orders;  -- Still 1000 rows (snapshot isolation)

COMMIT;

SELECT COUNT(*) FROM orders;  -- Now 1010 rows
```

**Status:** ✅ 100% Complete

---

## Performance Optimizations

### 11. Batch Operations

Optimized multi-row operations.

**Batch Insert:**
- Single transaction for all rows
- Shared buffer pool locks
- Reduced WAL overhead
- Bulk index updates

**Batch Update/Delete:**
- Bulk operations with single WHERE evaluation
- Optimized version chain management
- Minimal lock contention

**Status:** ✅ Implemented

---

### 12. Index-Only Scans

Use index data without accessing heap when possible.

```sql
-- Index covers all selected columns
CREATE INDEX idx_product_name ON products(product_name);

SELECT product_name FROM products WHERE product_name LIKE 'A%';
-- Uses index-only scan (no heap access)
```

**Status:** ✅ Implemented (B-tree, BRIN, Bitmap indexes)

---

## File Locations

**Parser AST:** `/home/user/ScratchBird/include/scratchbird/parser/ast.h:54-59, 1829-2310`
**Executor:** `/home/user/ScratchBird/src/sblr/executor.cpp` (969KB)
**Opcodes:** `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h`
**MGA Rules:** `/home/user/ScratchBird/MGA_RULES.md`

---

## Summary

| DML Operation | Status | Features |
|---------------|--------|----------|
| **INSERT** | ✅ 100% | VALUES, SELECT, ON CONFLICT, RETURNING, DEFAULT VALUES |
| **UPDATE** | ✅ 100% | FROM clause, RETURNING, Deferred constraints |
| **DELETE** | ✅ 100% | USING clause, RETURNING, CASCADE |
| **SELECT** | ✅ 100% | JOINs, CTEs, Window functions, Set operations |
| **MERGE** | ✅ 100% | Multiple WHEN clauses, NOT MATCHED BY SOURCE |
| **RETURNING** | ✅ 100% | INSERT, UPDATE, DELETE |
| **CTEs** | ✅ 100% | Recursive and non-recursive |
| **Window Functions** | ✅ 100% | All standard functions + framing |
| **Set Operations** | ✅ 100% | UNION, INTERSECT, EXCEPT (with ALL) |
| **Subqueries** | ✅ 100% | Scalar, IN, EXISTS, ANY, ALL |
| **JOINs** | ✅ 100% | INNER, LEFT, RIGHT, FULL, CROSS, NATURAL |
| **Constraints** | ✅ 100% | Deferred checking, Generated columns, Identity |

**Overall Completion:** ✅ 100% (Alpha 1)

---

## Next Steps (Post-Alpha 1)

- ⏳ Updatable views (INSERT/UPDATE/DELETE through views)
- ⏳ MERGE with RETURNING clause
- ⏳ Parallel DML execution
- ⏳ Bulk loading utilities
- ⏳ Advanced query optimization
