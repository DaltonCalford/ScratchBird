[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - DML Modification

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`

---

## Overview

This document covers Firebird SQL data modification statements:

- **INSERT** - Add new rows to a table
- **UPDATE** - Modify existing rows
- **DELETE** - Remove rows
- **UPDATE OR INSERT** - Firebird's upsert operation
- **MERGE** - Conditional insert/update/delete

All modification statements support the **RETURNING** clause to return values from the affected rows.

---

## INSERT

### Description

Inserts one or more rows into a table.

### Syntax

```sql
INSERT INTO table_name [(column_list)]
{
    VALUES (value_list) [, (value_list) ...] |
    SELECT ... |
    DEFAULT VALUES
}
[RETURNING column_list [INTO variable_list]]
```

### Examples

#### Insert All Columns

```sql
INSERT INTO employees
VALUES (100, 'John', 'Doe', '2024-01-15', 50000, 10);
```

#### Insert Specific Columns

```sql
INSERT INTO employees (first_name, last_name, department_id)
VALUES ('Jane', 'Smith', 20);
```

#### Insert with DEFAULT Values

```sql
INSERT INTO orders (customer_id, order_date, status)
VALUES (100, DEFAULT, DEFAULT);
```

#### Insert Multiple Rows

```sql
INSERT INTO categories (category_name)
VALUES ('Electronics'),
       ('Clothing'),
       ('Books'),
       ('Home & Garden');
```

**Note**: V2 pipeline may only emit the first VALUES row.

#### Insert with Expressions

```sql
INSERT INTO audit_log (action, created_at, user_name)
VALUES ('LOGIN', CURRENT_TIMESTAMP, CURRENT_USER);
```

#### Insert with Subquery

```sql
INSERT INTO archive_orders (order_id, customer_id, total_amount)
SELECT order_id, customer_id, total_amount
FROM orders
WHERE order_date < '2023-01-01';
```

**Note**: INSERT ... SELECT may not be compiled in the current implementation.

#### Insert DEFAULT VALUES

```sql
-- Insert a row with all default values
INSERT INTO config_settings DEFAULT VALUES;
```

### RETURNING Clause

Returns values from the inserted row, useful for getting generated IDs:

```sql
-- Get the auto-generated ID
INSERT INTO customers (company_name, email)
VALUES ('Acme Corp', 'contact@acme.com')
RETURNING customer_id;

-- Return multiple columns
INSERT INTO orders (customer_id, order_date)
VALUES (100, CURRENT_DATE)
RETURNING order_id, order_date, status;

-- Use computed expressions in RETURNING
INSERT INTO employees (first_name, last_name, salary)
VALUES ('John', 'Doe', 60000)
RETURNING employee_id, first_name || ' ' || last_name AS full_name;
```

### Insert with Generator/Sequence

```sql
-- Using NEXT VALUE FOR
INSERT INTO orders (order_id, customer_id, order_date)
VALUES (NEXT VALUE FOR seq_order_id, 100, CURRENT_DATE)
RETURNING order_id;

-- Using GEN_ID (legacy Firebird function)
INSERT INTO orders (order_id, customer_id, order_date)
VALUES (GEN_ID(gen_order_id, 1), 100, CURRENT_DATE)
RETURNING order_id;
```

**Note**: Sequences/generators may not be implemented.

---

## UPDATE

### Description

Modifies existing rows in a table.

### Syntax

```sql
UPDATE table_name
SET column = expression [, column = expression ...]
[WHERE condition]
[RETURNING column_list [INTO variable_list]]
```

### Examples

#### Update All Rows

```sql
-- Be careful! Updates every row
UPDATE products SET price = price * 1.10;
```

#### Update with WHERE

```sql
UPDATE employees
SET salary = 55000
WHERE employee_id = 100;
```

#### Update Multiple Columns

```sql
UPDATE employees
SET salary = 60000,
    department_id = 20,
    job_title = 'Senior Developer'
WHERE employee_id = 100;
```

#### Update with Expressions

```sql
-- Increase all salaries by 5%
UPDATE employees
SET salary = salary * 1.05
WHERE department_id = 10;

-- Update timestamp
UPDATE orders
SET updated_at = CURRENT_TIMESTAMP
WHERE order_id = 500;

-- Calculate new value
UPDATE inventory
SET quantity = quantity - 10
WHERE product_id = 200;
```

#### Update with Subquery

```sql
UPDATE products
SET category_id = (
    SELECT category_id FROM categories
    WHERE category_name = 'Electronics'
)
WHERE product_name LIKE 'Phone%';
```

#### Update with CASE

```sql
UPDATE employees
SET bonus = CASE
    WHEN performance_rating >= 4 THEN salary * 0.20
    WHEN performance_rating >= 3 THEN salary * 0.10
    WHEN performance_rating >= 2 THEN salary * 0.05
    ELSE 0
END;
```

### RETURNING Clause

```sql
-- Return updated values
UPDATE accounts
SET balance = balance - 100
WHERE account_id = 12345
RETURNING account_id, balance AS new_balance;

-- Return old and new values using expressions
UPDATE products
SET price = price * 1.10
WHERE product_id = 100
RETURNING product_id, price AS new_price;
```

### Update with NULL

```sql
-- Set column to NULL
UPDATE employees
SET manager_id = NULL
WHERE department_id = 10;

-- Update only where column is NULL
UPDATE orders
SET shipped_date = CURRENT_DATE
WHERE shipped_date IS NULL
AND status = 'READY';
```

---

## DELETE

### Description

Removes rows from a table.

### Syntax

```sql
DELETE FROM table_name
[WHERE condition]
[RETURNING column_list [INTO variable_list]]
```

### Examples

#### Delete All Rows

```sql
-- Be careful! Deletes all rows
DELETE FROM temporary_data;
```

#### Delete with WHERE

```sql
DELETE FROM employees
WHERE employee_id = 100;
```

#### Delete with Complex Condition

```sql
DELETE FROM orders
WHERE status = 'CANCELLED'
AND order_date < '2023-01-01';
```

#### Delete with Subquery

```sql
DELETE FROM order_items
WHERE order_id IN (
    SELECT order_id FROM orders
    WHERE status = 'CANCELLED'
);
```

#### Delete with EXISTS

```sql
DELETE FROM customers c
WHERE NOT EXISTS (
    SELECT 1 FROM orders o
    WHERE o.customer_id = c.customer_id
);
```

### RETURNING Clause

```sql
-- Return deleted row data
DELETE FROM audit_log
WHERE created_at < '2023-01-01'
RETURNING log_id, action, created_at;

-- Capture deleted IDs
DELETE FROM temp_results
WHERE session_id = 'abc123'
RETURNING result_id;
```

### Delete Patterns

#### Delete Duplicates (Keep First)

```sql
-- Delete duplicate emails, keeping the lowest ID
DELETE FROM users u1
WHERE EXISTS (
    SELECT 1 FROM users u2
    WHERE u2.email = u1.email
    AND u2.user_id < u1.user_id
);
```

#### Delete Old Records

```sql
-- Delete records older than 1 year
DELETE FROM logs
WHERE log_date < CURRENT_DATE - 365;
```

#### Delete in Batches

```sql
-- Delete in smaller batches to avoid long locks
DELETE FROM large_table
WHERE id IN (
    SELECT FIRST 1000 id FROM large_table
    WHERE status = 'DELETED'
);
```

---

## UPDATE OR INSERT (Upsert)

### Description

Firebird's upsert operation: updates existing rows or inserts new ones based on matching columns.

### Syntax

```sql
UPDATE OR INSERT INTO table_name (column_list)
VALUES (value_list)
[MATCHING (column_list)]
[RETURNING column_list]
```

- If **MATCHING** is specified, those columns are used to find existing rows
- If no MATCHING clause, the primary key is used
- If a matching row exists, UPDATE is performed; otherwise, INSERT

### Examples

#### Basic Upsert

```sql
UPDATE OR INSERT INTO products (product_id, product_name, price)
VALUES (100, 'Widget', 19.99)
MATCHING (product_id);
```

- If product_id 100 exists: updates product_name and price
- If product_id 100 doesn't exist: inserts new row

#### Upsert with Primary Key

```sql
-- Uses primary key by default
UPDATE OR INSERT INTO settings (setting_key, setting_value)
VALUES ('theme', 'dark');
```

#### Upsert with Multiple Matching Columns

```sql
UPDATE OR INSERT INTO inventory (warehouse_id, product_id, quantity)
VALUES (1, 100, 50)
MATCHING (warehouse_id, product_id);
```

#### Upsert with RETURNING

```sql
UPDATE OR INSERT INTO users (user_id, username, email, last_login)
VALUES (500, 'jdoe', 'jdoe@example.com', CURRENT_TIMESTAMP)
MATCHING (user_id)
RETURNING user_id, username;
```

### Current Implementation Status

**Status**: Stubbed

The UPDATE OR INSERT statement is parsed, but the UPDATE path is not implemented. It currently compiles as INSERT only, meaning:
- New rows will be inserted correctly
- Existing rows will cause constraint violations instead of being updated

### Workaround

Until UPDATE OR INSERT is fully implemented, use explicit logic:

```sql
-- Check if row exists, then update or insert
-- In your application code:
-- 1. Try UPDATE first
UPDATE products
SET product_name = 'Widget', price = 19.99
WHERE product_id = 100;

-- 2. If no rows affected, INSERT
-- (Check ROW_COUNT or similar)
INSERT INTO products (product_id, product_name, price)
VALUES (100, 'Widget', 19.99);
```

Or use a stored procedure pattern (if PSQL is available):

```sql
-- Not currently supported - PSQL not implemented
```

---

## MERGE

### Description

The MERGE statement (also known as UPSERT in some databases) conditionally inserts, updates, or deletes rows based on matching conditions.

**Status**: Not implemented

The Firebird parser does not currently parse MERGE statements.

### Standard Firebird MERGE Syntax

```sql
MERGE INTO target_table [AS alias]
USING source_table_or_subquery [AS alias]
ON condition
WHEN MATCHED [AND condition] THEN
    UPDATE SET column = expression [, ...]
    | DELETE
WHEN NOT MATCHED [AND condition] THEN
    INSERT [(column_list)] VALUES (value_list)
```

### Standard Firebird Examples

```sql
-- This will NOT work in current implementation
MERGE INTO products p
USING new_products np ON p.product_id = np.product_id
WHEN MATCHED THEN
    UPDATE SET p.price = np.price, p.quantity = np.quantity
WHEN NOT MATCHED THEN
    INSERT (product_id, product_name, price, quantity)
    VALUES (np.product_id, np.product_name, np.price, np.quantity);
```

### Workaround

Use separate UPDATE and INSERT statements:

```sql
-- Update existing rows
UPDATE products p
SET price = np.price, quantity = np.quantity
FROM new_products np
WHERE p.product_id = np.product_id;

-- Insert new rows
INSERT INTO products (product_id, product_name, price, quantity)
SELECT np.product_id, np.product_name, np.price, np.quantity
FROM new_products np
WHERE NOT EXISTS (
    SELECT 1 FROM products p
    WHERE p.product_id = np.product_id
);
```

Or use UPDATE OR INSERT (when fully implemented):

```sql
UPDATE OR INSERT INTO products (product_id, product_name, price, quantity)
VALUES (?, ?, ?, ?)
MATCHING (product_id);
```

---

## Transaction Context

### Auto-Commit vs Explicit Transactions

By default, each statement may auto-commit. For related modifications, use explicit transactions:

```sql
-- Start transaction
SET TRANSACTION;

-- Make related changes
INSERT INTO orders (customer_id, order_date)
VALUES (100, CURRENT_DATE)
RETURNING order_id;

-- Assume order_id = 500
INSERT INTO order_items (order_id, product_id, quantity)
VALUES (500, 10, 2);

INSERT INTO order_items (order_id, product_id, quantity)
VALUES (500, 20, 1);

-- Commit all changes together
COMMIT;
```

### Savepoints for Partial Rollback

```sql
SET TRANSACTION;

INSERT INTO accounts (account_id, balance) VALUES (1, 1000);
SAVEPOINT sp1;

INSERT INTO accounts (account_id, balance) VALUES (2, 2000);
-- Oops, wrong data
ROLLBACK TO SAVEPOINT sp1;

-- Re-insert correct data
INSERT INTO accounts (account_id, balance) VALUES (2, 5000);

COMMIT;
```

---

## Batch Operations

### Bulk Insert from Select

```sql
-- Copy data between tables
INSERT INTO archive_orders (order_id, customer_id, total, archived_at)
SELECT order_id, customer_id, total_amount, CURRENT_TIMESTAMP
FROM orders
WHERE order_date < '2023-01-01';
```

### Bulk Update with Select

```sql
-- Update based on another table
UPDATE products p
SET price = s.suggested_price
FROM pricing_suggestions s
WHERE p.product_id = s.product_id
AND s.effective_date <= CURRENT_DATE;
```

**Note**: UPDATE...FROM syntax may have limitations.

### Bulk Delete

```sql
-- Delete based on subquery
DELETE FROM order_items
WHERE order_id IN (
    SELECT order_id FROM orders
    WHERE status = 'CANCELLED'
);

-- Delete matching records
DELETE FROM products
WHERE category_id IN (
    SELECT category_id FROM categories
    WHERE discontinued = 1
);
```

---

## Error Handling

### Constraint Violations

```sql
-- Primary key violation
INSERT INTO products (product_id, name)
VALUES (1, 'Test');
-- Error if product_id 1 already exists

-- Foreign key violation
INSERT INTO orders (customer_id) VALUES (99999);
-- Error if customer 99999 doesn't exist

-- Check constraint violation
INSERT INTO employees (age) VALUES (-5);
-- Error if CHECK (age > 0) exists
```

### NULL Violations

```sql
-- NOT NULL violation
INSERT INTO customers (company_name)
VALUES (NULL);
-- Error if company_name is NOT NULL
```

### Handling Errors in Application

```
In your application code:
1. Execute the DML statement
2. Check for SQLCODE/SQLSTATE
3. If error, handle appropriately (rollback, retry, notify user)
```

---

## Performance Considerations

### Index Usage

```sql
-- Ensure WHERE clause uses indexes
UPDATE orders SET status = 'SHIPPED'
WHERE order_id = 500;  -- Uses primary key index

DELETE FROM logs
WHERE created_at < '2023-01-01';  -- Needs index on created_at
```

### Batch Size

For large modifications, process in batches:

```sql
-- Instead of one huge delete
-- Delete in chunks
DELETE FROM large_table
WHERE id IN (
    SELECT FIRST 10000 id FROM large_table
    WHERE status = 'OLD'
);
-- Repeat until done
```

### Trigger Overhead

Each modification fires triggers. For bulk operations:
- Consider disabling triggers temporarily (if possible)
- Use batch operations efficiently
- Monitor trigger execution time

---

## Known Limitations

### Partial Implementation

**INSERT**
- Basic INSERT with VALUES works
- RETURNING clause works
- **V2 pipeline limitation**: Only first VALUES row is emitted for multi-row inserts
- **Not compiled**: INSERT ... SELECT may not work
- Status: Partial

**UPDATE**
- Basic UPDATE works
- RETURNING clause works
- **V2 pipeline limitations** apply to complex expressions
- Status: Partial

**DELETE**
- Basic DELETE works
- RETURNING clause works
- **V2 pipeline limitations** apply to complex WHERE clauses
- Status: Partial

### Stubbed Implementation

**UPDATE OR INSERT**
- Parser accepts the syntax
- **UPDATE path not implemented**: Compiles as INSERT only
- Will fail on existing rows due to constraint violations
- Status: Stubbed

### Missing Features

**MERGE**
- Parser does not accept MERGE statements
- Will generate parse errors
- Status: Missing

### V2 Pipeline Limitations

**Multi-Row VALUES**
- Parser accepts multiple VALUES rows
- Only first row is processed by bytecode generator
- Workaround: Execute separate INSERT for each row

**INSERT...SELECT**
- Parser accepts the syntax
- Bytecode generation may fail
- Workaround: SELECT data first, then INSERT in application

**Complex Expressions**
- Some expressions in SET or VALUES may not be properly encoded
- Test complex expressions before relying on them

### Specification Deltas

**RETURNING Clause**
- Syntax is fully parsed
- Execution depends on proper bytecode generation
- INTO variable_list (for PSQL) is not available

**Subqueries in DML**
- Parser accepts subqueries
- Execution may have issues with complex subqueries
- Test thoroughly before production use

---

## Quick Reference

### INSERT Variants

| Syntax | Description |
|--------|-------------|
| `INSERT INTO t VALUES (...)` | Insert single row |
| `INSERT INTO t (cols) VALUES (...)` | Insert with column list |
| `INSERT INTO t VALUES (...), (...)` | Multi-row (partial support) |
| `INSERT INTO t SELECT ...` | Insert from query (may not work) |
| `INSERT INTO t DEFAULT VALUES` | Insert defaults |

### UPDATE Patterns

| Pattern | Example |
|---------|---------|
| Simple | `UPDATE t SET c=v WHERE id=?` |
| Multiple columns | `UPDATE t SET a=1, b=2 WHERE ...` |
| Expression | `UPDATE t SET c=c*1.1 WHERE ...` |
| From subquery | `UPDATE t SET c=(SELECT...)` |

### DELETE Patterns

| Pattern | Example |
|---------|---------|
| Simple | `DELETE FROM t WHERE id=?` |
| With subquery | `DELETE FROM t WHERE id IN (SELECT...)` |
| With EXISTS | `DELETE FROM t WHERE EXISTS (...)` |
| All rows | `DELETE FROM t` |

### RETURNING Options

| Syntax | Description |
|--------|-------------|
| `RETURNING col` | Return single column |
| `RETURNING col1, col2` | Return multiple columns |
| `RETURNING col AS alias` | Return with alias |
| `RETURNING *` | Return all columns |

---

## See Also

- [SELECT](06_dml_select.md) - Query data
- [Transactions](08_transactions.md) - Transaction control
- [Tables and Constraints](02_tables_and_constraints.md) - Constraint definitions
- [Indexes](03_indexes_views_sequences.md) - Index optimization

