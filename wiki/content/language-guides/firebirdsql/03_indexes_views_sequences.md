[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Firebird SQL - Indexes, Views, and Sequences

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers Firebird SQL Data Definition Language (DDL) for secondary database objects: indexes for query optimization, views for data abstraction, and sequences (also called generators) for auto-incrementing values.

**Key Concepts:**
- **Indexes**: Improve query performance and enforce uniqueness
- **Views**: Virtual tables defined by queries
- **Sequences/Generators**: Generate sequential numeric values for primary keys

**Important**: CREATE/DROP INDEX, CREATE/DROP VIEW, CREATE/DROP SEQUENCE are fully implemented in the parser, semantic analyzer, and bytecode generator. ALTER INDEX and ALTER SEQUENCE are not yet implemented.

---

## CREATE INDEX

### Description

Creates an index on one or more columns of a table to improve query performance. Firebird supports unique indexes, ascending/descending indexes, expression-based indexes, and partial indexes (with WHERE clause).

Indexes are used automatically by the query optimizer when appropriate, and are essential for:
- Speeding up SELECT queries with WHERE clauses
- Enforcing UNIQUE constraints
- Improving JOIN performance
- Optimizing ORDER BY operations

### Syntax

```sql
CREATE [ UNIQUE ] [ ASCENDING | DESCENDING ] INDEX [ index_name ]
    ON table_name ( index_item [, index_item ...] )
    [ WHERE condition ]
```

Where `index_item` is:
```sql
{ column_name | expression } [ ASC | DESC ]
```

### Parameters

- **UNIQUE**: Enforces uniqueness; no two rows can have the same index key values
- **ASCENDING**: Index is ordered ascending (default if not specified)
- **DESCENDING**: Index is ordered descending
- **index_name**: Optional name for the index. If omitted, Firebird generates a name automatically
- **table_name**: The table to index
- **column_name**: Column to include in the index
- **expression**: Computed expression to index (expression-based index)
- **ASC | DESC**: Per-column sort order (ascending or descending)
- **WHERE condition**: Creates a partial index containing only rows matching the condition

### Examples

#### Simple Single-Column Index

```sql
CREATE INDEX idx_employees_last_name
    ON employees (last_name);
```

Creates an ascending index on the `last_name` column for faster lookups.

#### Multi-Column (Composite) Index

```sql
CREATE INDEX idx_orders_customer_date
    ON orders (customer_id, order_date);
```

Useful for queries that filter or sort by both columns.

#### Unique Index

```sql
CREATE UNIQUE INDEX idx_users_email
    ON users (email);
```

Ensures no duplicate email addresses exist in the table.

#### Descending Index

```sql
CREATE DESCENDING INDEX idx_products_price_desc
    ON products (unit_price);
```

Optimizes queries that sort by price in descending order.

#### Mixed Sort Order Index

```sql
CREATE INDEX idx_sales_date_amount
    ON sales (sale_date DESC, amount ASC);
```

Different columns have different sort orders within the same index.

#### Expression-Based Index

```sql
CREATE INDEX idx_users_lower_email
    ON users (LOWER(email));
```

Indexes the lowercase version of email for case-insensitive searches:
```sql
SELECT * FROM users WHERE LOWER(email) = 'user@example.com';
```

#### Expression Index with Function

```sql
CREATE INDEX idx_orders_year
    ON orders (EXTRACT(YEAR FROM order_date));
```

Optimizes queries filtering by year:
```sql
SELECT * FROM orders WHERE EXTRACT(YEAR FROM order_date) = 2024;
```

#### Partial Index (Filtered Index)

```sql
CREATE INDEX idx_active_customers
    ON customers (customer_id)
    WHERE status = 'ACTIVE';
```

Indexes only active customers, reducing index size and improving performance for queries on active customers.

#### Partial Index with Complex Condition

```sql
CREATE INDEX idx_high_value_orders
    ON orders (order_date, customer_id)
    WHERE total_amount > 1000 AND status = 'COMPLETED';
```

Optimizes queries for high-value completed orders.

#### Unique Partial Index

```sql
CREATE UNIQUE INDEX idx_active_user_email
    ON users (email)
    WHERE status = 'ACTIVE';
```

Ensures email uniqueness only among active users.

#### Index Without Name

```sql
CREATE INDEX ON products (category_id, product_name);
```

Firebird will auto-generate an index name.

### Usage Notes

1. **Performance Impact**:
   - Indexes speed up SELECT queries but slow down INSERT, UPDATE, and DELETE operations
   - Only create indexes that will be used by queries

2. **Column Order in Composite Indexes**:
   - The order matters: `(a, b)` is different from `(b, a)`
   - Put the most selective column first
   - Index can be used for queries on leading columns: index `(a, b, c)` helps queries on `a`, `(a, b)`, or `(a, b, c)`, but not on `b` or `c` alone

3. **Expression Indexes**:
   - Queries must use the exact same expression to benefit from the index
   - Functions in WHERE clause should match the index expression

4. **Partial Indexes**:
   - Smaller index size means better cache utilization
   - Only useful when most queries filter on the indexed condition
   - WHERE clause must be relatively simple

5. **Unique Indexes vs UNIQUE Constraints**:
   - Both enforce uniqueness
   - UNIQUE constraint creates a unique index internally
   - Explicit unique index gives more control (e.g., partial unique indexes)

6. **Automatic Index Creation**:
   - PRIMARY KEY and UNIQUE constraints automatically create indexes
   - No need to create separate indexes for these columns unless you need different sort order

### Current Implementation Status

**Status**: Implemented

- Parser creates `ast::CreateIndexStmt` with full syntax support (UNIQUE, ASC/DESC, expression indexes, partial indexes)
- Semantic analyzer resolves to `ResolvedCreateIndexStmt`
- Bytecode generator emits `CREATE_INDEX` opcode with table reference, column list, and options

### Related Features

- [DROP INDEX](#drop-index) - Remove an index
- [ALTER INDEX](#alter-index) - Modify an index
- [CREATE TABLE](02_tables_and_constraints.md#create-table) - Tables can have inline UNIQUE constraints

---

## ALTER INDEX

### Description

Modifies an existing index. In Firebird, ALTER INDEX is primarily used to activate or deactivate an index.

**Status**: Not implemented

The Firebird parser does not currently parse ALTER INDEX statements.

### Standard Firebird ALTER INDEX Operations

In standard Firebird, you can:

```sql
-- Deactivate an index (stops using it, keeps structure)
ALTER INDEX index_name INACTIVE;

-- Reactivate an index
ALTER INDEX index_name ACTIVE;

-- Rebuild an index (reorganize for better performance)
-- This is typically done automatically by the database
```

### Workaround

Since ALTER INDEX is not supported:

**To rebuild an index**:
```sql
-- Drop and recreate
DROP INDEX idx_my_index;
CREATE INDEX idx_my_index ON my_table (my_column);
```

**To change an index**:
```sql
-- Drop old index
DROP INDEX idx_old;

-- Create new index with desired properties
CREATE INDEX idx_new ON my_table (other_column);
```

### Related Features

- [CREATE INDEX](#create-index) - Create an index
- [DROP INDEX](#drop-index) - Remove an index

---

## DROP INDEX

### Description

Removes an index from the database. This operation removes the index structure but does not affect the underlying table data.

### Syntax

```sql
DROP INDEX index_name
```

### Parameters

- **index_name**: The name of the index to drop

### Examples

#### Drop a Simple Index

```sql
DROP INDEX idx_employees_last_name;
```

#### Drop Multiple Indexes

```sql
DROP INDEX idx_old_index1;
DROP INDEX idx_old_index2;
DROP INDEX idx_obsolete;
```

#### Drop Index in Transaction

```sql
SET TRANSACTION;

-- Drop indexes
DROP INDEX idx_temp1;
DROP INDEX idx_temp2;

-- Commit or rollback
COMMIT;
```

### Usage Notes

1. **Constraint Indexes**: You cannot drop indexes created by PRIMARY KEY or UNIQUE constraints using DROP INDEX. You must use `ALTER TABLE ... DROP CONSTRAINT` instead.

2. **Performance Impact**:
   - Dropping an index that's heavily used by queries will slow down those queries
   - However, it speeds up INSERT, UPDATE, and DELETE operations

3. **Recreating Indexes**: Sometimes you drop and recreate an index to:
   - Change the index definition
   - Rebuild the index for better performance
   - Change from ascending to descending

4. **No Cascade**: Dropping an index doesn't affect dependent objects (unlike dropping a table)

5. **Index Name**: You need to know the exact index name. Query the system catalog to find index names if needed.

### Examples with Constraint Indexes

```sql
-- This will FAIL if idx_pk was created by PRIMARY KEY constraint
DROP INDEX idx_pk;  -- ERROR

-- Instead, drop the constraint
ALTER TABLE my_table DROP CONSTRAINT pk_my_table;
```

### Current Implementation Status

**Status**: Implemented

- Parser creates `ast::DropIndexStmt`
- Semantic analyzer resolves to `ResolvedDropIndexStmt`
- Bytecode generator emits `DROP_INDEX` opcode with index name

### Related Features

- [CREATE INDEX](#create-index) - Create an index
- [ALTER TABLE](02_tables_and_constraints.md#alter-table) - Drop constraints (which may drop associated indexes)

---

## CREATE VIEW

### Description

Creates a view, which is a virtual table based on a SELECT query. Views provide data abstraction, security (by limiting column/row access), and can simplify complex queries.

Firebird also supports RECREATE VIEW, which drops and recreates the view if it exists (equivalent to CREATE OR REPLACE VIEW).

### Syntax

```sql
CREATE [ OR REPLACE ] VIEW view_name [ ( column_name [, ...] ) ]
    AS select_statement
    [ WITH CHECK OPTION ]
```

### Parameters

- **OR REPLACE**: Drop and recreate the view if it already exists
- **view_name**: Name of the view to create
- **column_name**: Optional column name list. If omitted, column names come from the SELECT statement
- **select_statement**: The SELECT query that defines the view
- **WITH CHECK OPTION**: Ensures that INSERT/UPDATE through the view only affect rows visible through the view

### Examples

#### Basic View

```sql
CREATE VIEW v_active_customers AS
    SELECT customer_id, company_name, email, phone
    FROM customers
    WHERE status = 'ACTIVE';
```

Query the view like a table:
```sql
SELECT * FROM v_active_customers;
```

#### View with Explicit Column Names

```sql
CREATE VIEW v_employee_summary (id, full_name, department, hire_year) AS
    SELECT employee_id,
           first_name || ' ' || last_name,
           department_name,
           EXTRACT(YEAR FROM hire_date)
    FROM employees
    JOIN departments ON employees.department_id = departments.department_id;
```

#### View with Aggregation

```sql
CREATE VIEW v_sales_by_customer AS
    SELECT
        customer_id,
        COUNT(*) AS order_count,
        SUM(total_amount) AS total_sales,
        AVG(total_amount) AS avg_order_value
    FROM orders
    WHERE status = 'COMPLETED'
    GROUP BY customer_id;
```

#### View with JOIN

```sql
CREATE VIEW v_order_details AS
    SELECT
        o.order_id,
        o.order_date,
        c.company_name AS customer,
        c.email AS customer_email,
        o.total_amount,
        o.status
    FROM orders o
    JOIN customers c ON o.customer_id = c.customer_id;
```

#### Updatable View with CHECK OPTION

```sql
CREATE VIEW v_california_customers AS
    SELECT customer_id, company_name, state, email
    FROM customers
    WHERE state = 'CA'
    WITH CHECK OPTION;
```

With CHECK OPTION, this will fail:
```sql
-- ERROR: violates check option
UPDATE v_california_customers
SET state = 'NY'
WHERE customer_id = 123;
```

#### CREATE OR REPLACE VIEW

```sql
-- First creation
CREATE VIEW v_products AS
    SELECT product_id, product_name, unit_price
    FROM products;

-- Later, replace with enhanced version
CREATE OR REPLACE VIEW v_products AS
    SELECT product_id, product_name, unit_price, in_stock, category_id
    FROM products
    WHERE discontinued = 0;
```

#### RECREATE VIEW (Firebird-specific)

```sql
RECREATE VIEW v_summary AS
    SELECT category_id, COUNT(*) AS product_count
    FROM products
    GROUP BY category_id;
```

RECREATE is equivalent to CREATE OR REPLACE.

#### Complex View with Subquery

```sql
CREATE VIEW v_top_customers AS
    SELECT
        c.customer_id,
        c.company_name,
        c.email,
        (SELECT COUNT(*) FROM orders o WHERE o.customer_id = c.customer_id) AS order_count,
        (SELECT SUM(total_amount) FROM orders o WHERE o.customer_id = c.customer_id) AS lifetime_value
    FROM customers c
    WHERE status = 'ACTIVE'
    ORDER BY lifetime_value DESC;
```

#### Security View (Hiding Sensitive Columns)

```sql
CREATE VIEW v_public_employees AS
    SELECT employee_id, first_name, last_name, department_id, hire_date
    FROM employees;
    -- Excludes salary, SSN, and other sensitive fields
```

### Usage Notes

1. **View Updates**:
   - Simple views (single table, no aggregation, no DISTINCT) can often be updated
   - Complex views (with JOINs, GROUP BY, UNION, etc.) are usually read-only
   - WITH CHECK OPTION enforces view's WHERE clause on updates

2. **Performance**:
   - Views don't store data; they execute the query each time
   - Complex views can be slow; consider materialized tables for frequently-accessed aggregations
   - Views can sometimes prevent query optimization

3. **Dependencies**:
   - If you drop a table that a view depends on, the view becomes invalid
   - Changing column names/types in base tables can break views

4. **Recursive Views**:
   - Standard Firebird supports recursive views (using WITH RECURSIVE)
   - Check current parser support for this feature

5. **Column Names**:
   - If you don't specify column names explicitly, they come from the SELECT
   - Expressions without aliases will have generated names

6. **OR REPLACE vs RECREATE**:
   - Both drop and recreate the view
   - OR REPLACE is SQL standard
   - RECREATE is Firebird-specific synonym

### Current Implementation Status

**Status**: Implemented

- Parser creates `ast::CreateViewStmt` with OR REPLACE, column names, query, and WITH CHECK OPTION support
- Semantic analyzer resolves to `ResolvedCreateViewStmt`
- Bytecode generator emits `CREATE_VIEW` opcode with view definition
- RECREATE VIEW syntax is also supported

### Related Features

- [DROP VIEW](#drop-view) - Remove a view
- [SELECT](06_dml_select.md#select) - Query syntax used to define views

---

## DROP VIEW

### Description

Removes a view from the database. This only removes the view definition, not the underlying table data.

### Syntax

```sql
DROP VIEW view_name
```

### Parameters

- **view_name**: The name of the view to drop

### Examples

#### Drop a View

```sql
DROP VIEW v_active_customers;
```

#### Drop Multiple Views

```sql
DROP VIEW v_summary;
DROP VIEW v_old_report;
DROP VIEW v_obsolete;
```

#### Drop View in Transaction

```sql
SET TRANSACTION;

DROP VIEW v_temp_view;

COMMIT;
```

### Usage Notes

1. **Dependencies**: If other views depend on this view, you may need to drop those first or recreate them after.

2. **No IF EXISTS**: Standard Firebird doesn't support `DROP VIEW IF EXISTS`. You'll get an error if the view doesn't exist.

3. **No Data Loss**: Dropping a view only removes the query definition, not any underlying table data.

4. **Permissions**: Typically requires ownership of the view or appropriate privileges.

### Current Implementation Status

**Status**: Implemented

- Parser creates `ast::DropViewStmt` with IF EXISTS support
- Semantic analyzer resolves to `ResolvedDropViewStmt`
- Bytecode generator emits `DROP_VIEW` opcode

### Related Features

- [CREATE VIEW](#create-view) - Create a view

---

## CREATE SEQUENCE / CREATE GENERATOR

### Description

Creates a sequence (also called a generator in Firebird terminology) that generates sequential numeric values. Sequences are commonly used for generating unique primary key values.

**Note**: In Firebird, SEQUENCE and GENERATOR are synonyms - both create the same object type.

### Syntax

```sql
CREATE SEQUENCE sequence_name
    [ START [ WITH ] initial_value ]
    [ INCREMENT [ BY ] increment_value ]
```

Or using GENERATOR syntax (Firebird-specific):

```sql
CREATE GENERATOR generator_name
    [ START [ WITH ] initial_value ]
    [ INCREMENT [ BY ] increment_value ]
```

### Parameters

- **sequence_name**: Name of the sequence
- **START WITH**: Initial value (default is 1)
- **INCREMENT BY**: Increment value (default is 1)

### Examples

#### Basic Sequence

```sql
CREATE SEQUENCE seq_customer_id;
```

Creates a sequence starting at 1, incrementing by 1.

#### Sequence with Custom Start Value

```sql
CREATE SEQUENCE seq_order_id START WITH 1000;
```

Starts at 1000 instead of 1.

#### Sequence with Custom Increment

```sql
CREATE SEQUENCE seq_batch_id START WITH 1 INCREMENT BY 100;
```

Generates: 1, 101, 201, 301, ...

#### Using GENERATOR Syntax

```sql
CREATE GENERATOR gen_employee_id START WITH 1;
```

Equivalent to CREATE SEQUENCE.

### Using Sequences in Queries

In standard Firebird, you would retrieve the next value using:

```sql
-- Get next value
SELECT NEXT VALUE FOR seq_customer_id FROM RDB$DATABASE;

-- Or in an INSERT
INSERT INTO customers (customer_id, company_name)
VALUES (NEXT VALUE FOR seq_customer_id, 'Acme Corp');

-- Or using GEN_ID function (Firebird-specific)
SELECT GEN_ID(gen_customer_id, 1) FROM RDB$DATABASE;
```

### Usage Notes

1. **SEQUENCE vs GENERATOR**:
   - SEQUENCE is SQL standard syntax
   - GENERATOR is Firebird-specific (legacy) syntax
   - Both create the same object type
   - SEQUENCE is recommended for new code

2. **Sequence Values**:
   - Each call to NEXT VALUE FOR or GEN_ID generates a new value
   - Values are never reused, even after ROLLBACK
   - Gaps in values are normal and expected

3. **Concurrency**:
   - Sequences are designed for concurrent use
   - Each session gets unique values
   - No locking or waiting required

4. **Identity Columns**:
   - Modern approach: use GENERATED AS IDENTITY on columns
   - Sequences are useful when you need the ID before INSERT
   - Or when one sequence serves multiple tables

### Current Implementation Status

**Status**: Implemented

- Parser creates `ast::CreateSequenceStmt` with START WITH and INCREMENT BY support
- GENERATOR syntax is accepted as a synonym for SEQUENCE
- Semantic analyzer resolves to `ResolvedCreateSequenceStmt`
- Bytecode generator emits `CREATE_SEQUENCE` opcode with all sequence options (increment, min/max, start, cache, cycle)

### Related Features

- [ALTER SEQUENCE](#alter-sequence-drop-sequence) - Modify or drop a sequence
- [CREATE TABLE](02_tables_and_constraints.md#create-table) - GENERATED AS IDENTITY columns

---

## ALTER SEQUENCE / DROP SEQUENCE

### Description

Modifies or removes a sequence/generator.

### DROP SEQUENCE / DROP GENERATOR

**Status**: Implemented

```sql
-- Drop a sequence
DROP SEQUENCE seq_customer_id;

-- Or using generator syntax
DROP GENERATOR gen_customer_id;

-- With IF EXISTS
DROP SEQUENCE IF EXISTS seq_old;
```

- Parser creates `ast::DropSequenceStmt`
- Semantic analyzer resolves to `ResolvedDropSequenceStmt`
- Bytecode generator emits `DROP_SEQUENCE` opcode

### ALTER SEQUENCE

**Status**: Not implemented

The Firebird parser does not currently support ALTER SEQUENCE.

In standard Firebird, you would be able to:

```sql
-- Reset sequence to specific value
ALTER SEQUENCE seq_customer_id RESTART WITH 1000;

-- Change increment
ALTER SEQUENCE seq_customer_id INCREMENT BY 10;
```

**Workaround**: Drop and recreate the sequence with new values.

### Related Features

- [CREATE SEQUENCE](#create-sequence-create-generator) - Create a sequence

---

## Known Limitations

### What Works

**CREATE INDEX**
- Full syntax support: UNIQUE, ASC/DESC, expression indexes, partial indexes (WHERE clause)
- Parser → Semantic Analyzer → Bytecode Generator pipeline is complete
- Emits `CREATE_INDEX` opcode

**DROP INDEX**
- IF EXISTS support
- Emits `DROP_INDEX` opcode

**CREATE VIEW / RECREATE VIEW**
- CREATE/OR REPLACE/RECREATE VIEW with full SELECT syntax
- Column name lists and WITH CHECK OPTION
- Emits `CREATE_VIEW` opcode

**DROP VIEW**
- IF EXISTS support
- Emits `DROP_VIEW` opcode

**CREATE SEQUENCE / CREATE GENERATOR**
- Both SQL standard SEQUENCE and Firebird GENERATOR syntax
- START WITH and INCREMENT BY options
- Emits `CREATE_SEQUENCE` opcode

### Not Implemented

**ALTER INDEX**
- Parser does not accept ALTER INDEX statements
- Workaround: Drop and recreate the index

**ALTER SEQUENCE**
- Parser does not currently accept ALTER SEQUENCE statements
- Workaround: Drop and recreate the sequence

**DROP SEQUENCE / DROP GENERATOR**
- Implemented: Parser, Semantic Analyzer, and Bytecode Generator support DROP SEQUENCE
- Emits `DROP_SEQUENCE` opcode

### Specification References

- `/home/dcalford/CliWork/ScratchBird/docs/specifications/reference/firebird/`
- `/home/dcalford/CliWork/ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`
