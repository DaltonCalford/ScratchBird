# Native V2 SQL - Indexes, Views, and Sequences

## Overview

This document describes index, view, and sequence management features in ScratchBird's Native V2 SQL dialect.

- **Indexes** provide fast access to table rows and enforce uniqueness constraints
- **Views** are named queries that act as virtual tables
- **Sequences** generate unique numeric values, typically for primary keys

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`
- Index Implementation: `/ScratchBird/src/core/btree.cpp`, `/ScratchBird/src/core/hash_index.cpp`

---

## CREATE INDEX

### Description

Creates an index on one or more columns of a table. Indexes speed up queries by providing fast access paths to data and can enforce uniqueness constraints.

### Syntax

```sql
CREATE [UNIQUE] INDEX [IF NOT EXISTS] <index_name>
    ON <table_name>
    [USING <method>]
    ( <column_or_expression> [ASC | DESC] [NULLS FIRST | NULLS LAST] [, ...] )
    [INCLUDE ( <column> [, ...] )]
    [WHERE <predicate>]
    [TABLESPACE <tablespace_name>]
```

### Parameters

- **UNIQUE**: Creates a unique index that prevents duplicate values
- **IF NOT EXISTS**: Prevents error if index already exists
- **index_name**: Name of the index (must be unique within schema)
- **table_name**: Table to index (with optional schema qualification)
- **USING method**: Index type - `btree` (default), `hash`, `gin`, `gist`, `brin`
- **column_or_expression**: Column name or expression to index
- **ASC | DESC**: Sort order (ASC is default)
- **NULLS FIRST | NULLS LAST**: NULL value placement
- **INCLUDE**: Additional columns to store in index (covering index)
- **WHERE predicate**: Partial index condition (index only matching rows)
- **TABLESPACE**: Tablespace to store the index

### Index Types

**Implemented Index Types:**
- **btree**: B-Tree index (default) - General purpose, supports ranges, sorting
- **hash**: Hash index - Equality lookups only, very fast
- **gin**: Generalized Inverted Index - Array and full-text search (partial)
- **gist**: Generalized Search Tree - Spatial and custom data types (partial)
- **brin**: Block Range Index - Very large tables with natural ordering (stub)

**Index Type Characteristics:**

| Type | Use Case | Operators | Status |
|------|----------|-----------|--------|
| btree | General purpose, ordering | <, <=, =, >=, >, BETWEEN | Implemented |
| hash | Equality only | = | Implemented |
| gin | Arrays, JSONB, full-text | @>, ?, &&, @@ | Partial |
| gist | Spatial, ranges | &&, @>, <@, << | Partial |
| brin | Large tables, natural order | <, <=, =, >=, > | Stub |

### Examples

**Example 1: Create a simple B-Tree index**
```sql
CREATE INDEX idx_users_email ON users (email);
```

**Example 2: Create a unique index**
```sql
CREATE UNIQUE INDEX idx_products_sku ON products (sku);
```

**Example 3: Create a composite index**
```sql
CREATE INDEX idx_orders_customer_date ON orders (customer_id, order_date DESC);
```

**Example 4: Create a hash index for equality lookups**
```sql
CREATE INDEX idx_sessions_token ON sessions USING hash (session_token);
```

**Example 5: Create a partial index**
```sql
CREATE INDEX idx_active_users ON users (email)
WHERE active = true;
```

**Example 6: Create an index on an expression**
```sql
CREATE INDEX idx_users_lower_email ON users (LOWER(email));
```

**Example 7: Create a covering index with INCLUDE**
```sql
CREATE INDEX idx_orders_customer ON orders (customer_id)
INCLUDE (order_date, total_amount);
```

**Example 8: Create an index with custom NULL handling**
```sql
CREATE INDEX idx_employees_manager ON employees (manager_id NULLS FIRST);
```

**Example 9: Create a GIN index on a JSONB column**
```sql
CREATE INDEX idx_documents_data ON documents USING gin (data);
```

**Example 10: Create an index in a specific tablespace**
```sql
CREATE INDEX idx_large_table_id ON large_table (id)
TABLESPACE fast_ssd;
```

### Notes

- B-Tree is the default and most versatile index type
- Hash indexes are faster for equality but don't support ranges or sorting
- UNIQUE indexes automatically create unique constraints
- Partial indexes (with WHERE) save space and improve performance
- Expression indexes allow indexing computed values
- INCLUDE columns create covering indexes that can satisfy queries without table access
- Index creation locks the table (shared lock, allows reads)
- Large index creation can take significant time

---

## DROP INDEX

### Description

Removes an index from the database.

### Syntax

```sql
DROP INDEX [IF EXISTS] <index_name> [CASCADE | RESTRICT]
```

### Parameters

- **IF EXISTS**: Prevents error if index doesn't exist
- **index_name**: Name of the index to drop
- **CASCADE**: Automatically drops dependent objects
- **RESTRICT**: Default. Prevents drop if dependencies exist

### Examples

**Example 1: Drop an index**
```sql
DROP INDEX idx_users_email;
```

**Example 2: Safely drop an index that might not exist**
```sql
DROP INDEX IF EXISTS idx_old_column;
```

**Example 3: Drop with cascade**
```sql
DROP INDEX idx_products_sku CASCADE;
```

### Notes

- Dropping an index does not affect table data
- UNIQUE indexes created by UNIQUE or PRIMARY KEY constraints cannot be dropped directly
- Must drop the constraint instead
- Index drops acquire exclusive lock on the index
- Does not block concurrent reads or writes to the table

---

## CREATE VIEW

### Description

Creates a named query that acts as a virtual table. Views simplify complex queries, provide abstraction, and can restrict access to specific columns or rows.

### Syntax

```sql
CREATE [OR REPLACE] VIEW [IF NOT EXISTS] <view_name>
    [( <column_name> [, ...] )]
    AS <select_statement>
    [WITH CHECK OPTION]
```

### Parameters

- **OR REPLACE**: Replaces existing view with same name
- **IF NOT EXISTS**: Prevents error if view already exists (conflicts with OR REPLACE)
- **view_name**: Name of the view (schema-qualified if desired)
- **column_name**: Optional list of column names (overrides SELECT column names)
- **select_statement**: The query defining the view
- **WITH CHECK OPTION**: Ensures INSERTs/UPDATEs through view satisfy WHERE clause

### Examples

**Example 1: Create a simple view**
```sql
CREATE VIEW active_users AS
    SELECT id, username, email, created_at
    FROM users
    WHERE active = true;
```

**Example 2: Create a view with renamed columns**
```sql
CREATE VIEW customer_summary (customer, total_orders, total_spent) AS
    SELECT c.name,
           COUNT(o.id),
           SUM(o.amount)
    FROM customers c
    LEFT JOIN orders o ON c.id = o.customer_id
    GROUP BY c.id, c.name;
```

**Example 3: Create or replace an existing view**
```sql
CREATE OR REPLACE VIEW order_details AS
    SELECT o.id,
           o.order_date,
           c.name AS customer_name,
           o.amount
    FROM orders o
    JOIN customers c ON o.customer_id = c.id;
```

**Example 4: Create a view with WITH CHECK OPTION**
```sql
CREATE VIEW high_value_orders AS
    SELECT * FROM orders WHERE amount > 1000
    WITH CHECK OPTION;

-- This will fail because amount < 1000
-- INSERT INTO high_value_orders VALUES (1, 1, 500);
```

**Example 5: Create a view joining multiple tables**
```sql
CREATE VIEW employee_department AS
    SELECT e.id,
           e.first_name,
           e.last_name,
           d.name AS department_name,
           d.location
    FROM employees e
    JOIN departments d ON e.department_id = d.id;
```

**Example 6: Create a view with aggregation**
```sql
CREATE VIEW monthly_sales AS
    SELECT DATE_TRUNC('month', order_date) AS month,
           COUNT(*) AS order_count,
           SUM(amount) AS total_sales
    FROM orders
    GROUP BY DATE_TRUNC('month', order_date);
```

### Notes

- Views are re-executed each query (not materialized)
- Views can be queried like tables
- Some views are updatable (simple SELECT without aggregation)
- Complex views (aggregates, joins) are typically read-only
- WITH CHECK OPTION only applies to updatable views
- Views don't store data - they're stored queries
- View definitions are stored in system catalog

---

## DROP VIEW

### Description

Removes a view from the database.

### Syntax

```sql
DROP VIEW [IF EXISTS] <view_name> [CASCADE | RESTRICT]
```

### Parameters

- **IF EXISTS**: Prevents error if view doesn't exist
- **view_name**: Name of the view to drop
- **CASCADE**: Automatically drops dependent objects (views that reference this view)
- **RESTRICT**: Default. Prevents drop if dependencies exist

### Examples

**Example 1: Drop a view**
```sql
DROP VIEW active_users;
```

**Example 2: Safely drop a view**
```sql
DROP VIEW IF EXISTS old_report;
```

**Example 3: Drop view with cascade**
```sql
DROP VIEW customer_summary CASCADE;
```

### Notes

- Dropping a view doesn't affect underlying tables or data
- Dependent views must be dropped first or use CASCADE
- Cannot drop views that other objects depend on without CASCADE
- Views referenced in stored procedures may cause errors after dropping

---

## CREATE MATERIALIZED VIEW

### Description

Creates a materialized view, which stores the query results physically like a table. Unlike regular views, materialized views cache data for improved query performance.

### Syntax

```sql
CREATE MATERIALIZED VIEW <view_name>
    AS <select_statement>
```

### Parameters

- **view_name**: Name of the materialized view
- **select_statement**: Query whose results will be materialized

### Examples

**Example 1: Create a materialized view**
```sql
CREATE MATERIALIZED VIEW daily_sales_summary AS
    SELECT DATE(order_date) AS sale_date,
           COUNT(*) AS order_count,
           SUM(amount) AS total_sales
    FROM orders
    GROUP BY DATE(order_date);
```

**Example 2: Materialized view for expensive calculation**
```sql
CREATE MATERIALIZED VIEW product_statistics AS
    SELECT p.id,
           p.name,
           COUNT(oi.id) AS times_ordered,
           SUM(oi.quantity) AS total_quantity_sold,
           AVG(oi.unit_price) AS avg_price
    FROM products p
    LEFT JOIN order_items oi ON p.id = oi.product_id
    GROUP BY p.id, p.name;
```

### Notes

- Materialized views store actual data (unlike regular views)
- Data can become stale - requires manual refresh (REFRESH not exposed in V2 parser)
- Use for expensive queries that don't need real-time data
- Takes up storage space proportional to result set
- Can be indexed like regular tables for better performance
- Spec defines refresh behavior but parser doesn't expose REFRESH MATERIALIZED VIEW command

---

## DROP MATERIALIZED VIEW

### Description

Removes a materialized view and its stored data.

### Syntax

```sql
DROP MATERIALIZED VIEW [IF EXISTS] <view_name> [CASCADE | RESTRICT]
```

### Parameters

- **IF EXISTS**: Prevents error if view doesn't exist
- **view_name**: Name of the materialized view to drop
- **CASCADE**: Drops dependent objects
- **RESTRICT**: Default. Prevents drop if dependencies exist

### Examples

**Example 1: Drop a materialized view**
```sql
DROP MATERIALIZED VIEW daily_sales_summary;
```

**Example 2: Safely drop a materialized view**
```sql
DROP MATERIALIZED VIEW IF EXISTS old_stats;
```

### Notes

- Removes both the view definition and all stored data
- Indexes on the materialized view are also dropped
- Cannot be undone - backup data if needed

---

## CREATE SEQUENCE

### Description

Creates a sequence generator for producing unique numeric values, typically used for auto-incrementing primary keys.

### Syntax

```sql
CREATE SEQUENCE [IF NOT EXISTS] <sequence_name>
    [START WITH <start_value>]
    [INCREMENT BY <increment>]
    [MINVALUE <min_value> | NO MINVALUE]
    [MAXVALUE <max_value> | NO MAXVALUE]
    [CACHE <cache_size>]
    [CYCLE | NO CYCLE]
    [OWNED BY <table>.<column>]
```

### Parameters

- **IF NOT EXISTS**: Prevents error if sequence already exists
- **sequence_name**: Name of the sequence
- **START WITH**: Starting value (default: 1 for ascending, -1 for descending)
- **INCREMENT BY**: Step size (default: 1, negative for descending)
- **MINVALUE**: Minimum value (default: 1 for ascending)
- **NO MINVALUE**: Use default minimum
- **MAXVALUE**: Maximum value (default: large positive number)
- **NO MAXVALUE**: Use default maximum
- **CACHE**: Number of values to pre-allocate (default: 1, improves performance)
- **CYCLE**: Restart at min/max when limit reached
- **NO CYCLE**: Error when limit reached (default)
- **OWNED BY**: Tie sequence lifecycle to a table column

### Examples

**Example 1: Create a simple sequence**
```sql
CREATE SEQUENCE user_id_seq;
```

**Example 2: Create a sequence starting at 1000**
```sql
CREATE SEQUENCE order_id_seq START WITH 1000 INCREMENT BY 1;
```

**Example 3: Create a sequence with cache**
```sql
CREATE SEQUENCE invoice_seq
    START WITH 1
    INCREMENT BY 1
    CACHE 20;
```

**Example 4: Create a cycling sequence**
```sql
CREATE SEQUENCE rotation_seq
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 100
    CYCLE;
```

**Example 5: Create a descending sequence**
```sql
CREATE SEQUENCE countdown_seq
    START WITH 1000
    INCREMENT BY -1
    MINVALUE 1;
```

**Example 6: Create a sequence owned by a table column**
```sql
CREATE SEQUENCE users_id_seq OWNED BY users.id;
```

**Example 7: Create a sequence with specific bounds**
```sql
CREATE SEQUENCE year_seq
    START WITH 2024
    INCREMENT BY 1
    MINVALUE 2024
    MAXVALUE 2099
    NO CYCLE;
```

### Notes

- Sequences are transaction-independent (values never roll back)
- CACHE improves performance in high-concurrency scenarios
- NEXTVAL() gets next value, CURRVAL() gets current value
- OWNED BY causes sequence to drop automatically when column is dropped
- Sequences can have gaps (transaction rollbacks, cached values)
- Default sequence range depends on bigint max/min values

---

## ALTER SEQUENCE

### Description

Modifies properties of an existing sequence.

### Syntax

```sql
ALTER SEQUENCE <sequence_name>
    [RESTART [WITH <value>]]
    [INCREMENT BY <increment>]
    [MINVALUE <min_value> | NO MINVALUE]
    [MAXVALUE <max_value> | NO MAXVALUE]
    [CACHE <cache_size>]
    [CYCLE | NO CYCLE]
    [OWNED BY <table>.<column> | OWNED BY NONE]
```

### Parameters

- **RESTART**: Resets sequence to starting value or specified value
- **INCREMENT BY**: Changes step size
- **MINVALUE/MAXVALUE**: Changes bounds
- **CACHE**: Changes cache size
- **CYCLE/NO CYCLE**: Changes cycling behavior
- **OWNED BY**: Changes or removes ownership

### Examples

**Example 1: Restart a sequence**
```sql
ALTER SEQUENCE user_id_seq RESTART WITH 5000;
```

**Example 2: Change increment**
```sql
ALTER SEQUENCE counter_seq INCREMENT BY 10;
```

**Example 3: Change cache size**
```sql
ALTER SEQUENCE invoice_seq CACHE 50;
```

**Example 4: Set new maximum**
```sql
ALTER SEQUENCE year_seq MAXVALUE 2100;
```

**Example 5: Enable cycling**
```sql
ALTER SEQUENCE rotation_seq CYCLE;
```

**Example 6: Remove ownership**
```sql
ALTER SEQUENCE old_seq OWNED BY NONE;
```

**Example 7: Multiple changes at once**
```sql
ALTER SEQUENCE event_seq
    RESTART WITH 1
    INCREMENT BY 5
    CACHE 100;
```

### Notes

- RESTART resets the sequence immediately
- Changing INCREMENT doesn't affect existing generated values
- Cannot change START WITH after creation (use RESTART instead)
- Changes are transactional
- OWNED BY NONE removes automatic drop dependency

---

## DROP SEQUENCE

### Description

Removes a sequence from the database.

### Syntax

```sql
DROP SEQUENCE [IF EXISTS] <sequence_name> [CASCADE | RESTRICT]
```

### Parameters

- **IF EXISTS**: Prevents error if sequence doesn't exist
- **sequence_name**: Name of sequence to drop
- **CASCADE**: Drops dependent objects (columns using DEFAULT nextval())
- **RESTRICT**: Default. Prevents drop if dependencies exist

### Examples

**Example 1: Drop a sequence**
```sql
DROP SEQUENCE user_id_seq;
```

**Example 2: Safely drop a sequence**
```sql
DROP SEQUENCE IF EXISTS old_seq;
```

**Example 3: Drop with cascade**
```sql
DROP SEQUENCE invoice_seq CASCADE;
```

### Notes

- Cannot drop sequences with dependencies unless CASCADE used
- Columns with DEFAULT nextval(sequence) will fail without CASCADE
- Sequences owned by columns are auto-dropped when column is dropped
- Cannot be undone

---

## Sequence Functions

### NEXTVAL()

Returns the next value from a sequence and advances it.

```sql
SELECT NEXTVAL('user_id_seq');
INSERT INTO users (id, name) VALUES (NEXTVAL('user_id_seq'), 'Alice');
```

### CURRVAL()

Returns the current value of the sequence (last value returned by NEXTVAL in current session).

```sql
SELECT CURRVAL('user_id_seq');
```

### SETVAL()

Sets the sequence to a specific value.

```sql
SELECT SETVAL('user_id_seq', 1000);
SELECT SETVAL('user_id_seq', 1000, false); -- Next NEXTVAL returns 1000
```

### Notes

- NEXTVAL never returns the same value twice (even across transactions)
- CURRVAL fails if NEXTVAL hasn't been called in current session
- SETVAL is useful for migrating data or fixing sequence values

---

## Known Limitations

### Partial Implementation

**Index Types:**
- B-Tree: Fully implemented and production-ready
- Hash: Fully implemented and production-ready
- Bitmap: Fully implemented and production-ready
- R-Tree: Fully implemented (spatial indexing)
- GIN: Partial implementation (Phase 1-3 complete, not fully tested)
- GiST: Stub implementation only
- BRIN: Stub implementation only
- HNSW: Stub (vector search)
- Spec reference: `/docs/specifications/indexes/INDEX_IMPLEMENTATION_SPEC.md`

**Index Type Gaps in V2 Parser:**
- SPGIST not parsed
- RTREE not parsed (use GiST instead)
- HNSW not parsed
- BITMAP not parsed
- COLUMNSTORE not parsed
- LSM not parsed
- Spec reference: `/docs/specifications/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`

**Advanced Index Features:**
- Index-only scans work for B-Tree INCLUDE indexes
- Parallel index creation not implemented
- Concurrent index creation not supported
- Index reindexing (REINDEX) not exposed in V2 parser

### Stubbed Features

**Materialized View Refresh:**
- REFRESH MATERIALIZED VIEW command not parsed in V2
- Materialized views can be created but not refreshed through SQL
- Spec defines refresh behavior but parser doesn't expose it
- Spec reference: `/docs/specifications/ddl/DDL_VIEWS.md`

**Tablespace Commands:**
- CREATE TABLESPACE not parsed in V2
- ALTER TABLESPACE supports RENAME TO / SET SCHEMA (generic rename/move only)
- DROP TABLESPACE not parsed
- TABLESPACE clause in CREATE INDEX is parsed and enforced (errors if missing)
- Spec reference: `/docs/specifications/storage/TABLESPACE_SPECIFICATION.md`

### Missing Features

**View Features:**
- Updatable view detection incomplete
- INSTEAD OF triggers for views not implemented
- Security barrier views not supported
- Recursive views (WITH RECURSIVE) not in V2 parser

**Index Features:**
- Expression index validation limited
- Partial index predicate optimization incomplete
- Index usage in query plans needs improvement
- Functional/expression indexes parsed but optimization limited

**Sequence Features:**
- ALTER SEQUENCE OWNED BY may not be fully wired
- Sequence permissions (GRANT USAGE ON SEQUENCE) not fully implemented
- Distributed sequence generation not supported

### Spec Deltas

**Index Creation:**
- Some index types accepted by parser but not fully implemented in storage layer
- Index type validation incomplete
- Some index options parsed but ignored
- Spec reference: `/docs/specifications/indexes/AdvancedIndexes.md`

**Materialized Views:**
- Refresh behavior spec-defined but command not exposed
- Concurrent refresh not supported
- Materialized view indexes not automatically maintained
- Spec reference: `/docs/specifications/ddl/DDL_VIEWS.md`

**Sequences:**
- Full OWNED BY semantics may not be complete
- Sequence cache behavior in distributed scenarios undefined
- ALTER SEQUENCE all options may not be fully enforced
- Spec reference: `/docs/specifications/ddl/DDL_SEQUENCES.md`

### General Notes

- All DDL operations are fully transactional
- Objects persisted using ScratchBird's Multi-Generational Architecture (MGA)
- Index creation can be resource-intensive on large tables
- Full implementation status in `/docs/audit/parsers/V2/SUMMARY.md`
- Critical findings in `/docs/audit/parsers/CRITICAL_FINDINGS.md`
