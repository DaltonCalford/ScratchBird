# PostgreSQL Indexes, Views, and Sequences

**PostgreSQL Emulation Layer - Indexes, Views, and Sequences Reference**

This document covers PostgreSQL-compatible index, view, and sequence operations in ScratchBird's PostgreSQL emulation layer.

---

## Overview

Indexes improve query performance, views provide abstracted query interfaces, and sequences generate unique numeric identifiers. PostgreSQL provides powerful features for all three.

**Key Points:**
- Full PostgreSQL 16 syntax support for indexes, views, and sequences
- Advanced indexing: partial indexes, expression indexes, INCLUDE columns, CONCURRENTLY
- Materialized views for pre-computed query results
- Sequences for auto-incrementing values (SERIAL types use sequences internally)
- Current implementation status: parser complete, executor integration pending (see Known Limitations)

---

## CREATE INDEX

Creates an index on one or more columns of a table.

### Description

`CREATE INDEX` builds an index to speed up queries on table columns. PostgreSQL supports multiple index types (B-tree, Hash, GiST, SP-GiST, GIN, BRIN) and advanced features like partial indexes, expression indexes, and covering indexes.

### Syntax

```sql
CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] [ [ IF NOT EXISTS ] name ]
    ON [ ONLY ] table_name [ USING method ]
    ( { column_name | ( expression ) } [ COLLATE collation ] [ opclass [ ( opclass_param = value [, ... ] ) ] ]
      [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )
    [ INCLUDE ( column_name [, ...] ) ]
    [ NULLS [ NOT ] DISTINCT ]
    [ WITH ( storage_parameter [= value] [, ... ] ) ]
    [ TABLESPACE tablespace_name ]
    [ WHERE predicate ]
```

### Parameters

- **UNIQUE** - Enforce uniqueness on indexed column(s)
- **CONCURRENTLY** - Build index without locking the table for writes
- **IF NOT EXISTS** - Do not error if index already exists
- **name** - Name of the index
- **table_name** - Name of the table to index
- **USING method** - Index type: btree (default), hash, gist, spgist, gin, brin
- **column_name** - Column to index
- **expression** - Expression to index
- **COLLATE** - Collation for text columns
- **opclass** - Operator class for the column
- **ASC / DESC** - Sort direction
- **NULLS FIRST / LAST** - NULL value ordering
- **INCLUDE** - Additional columns to include (covering index)
- **WHERE** - Partial index predicate
- **TABLESPACE** - Tablespace for the index

### Examples

**Basic index:**
```sql
CREATE INDEX users_email_idx ON users (email);
```

**Unique index:**
```sql
CREATE UNIQUE INDEX users_email_unique_idx ON users (email);
```

**Index with IF NOT EXISTS:**
```sql
CREATE INDEX IF NOT EXISTS users_name_idx ON users (name);
```

**Composite index (multiple columns):**
```sql
CREATE INDEX users_name_email_idx ON users (name, email);
```

**Index with sort order:**
```sql
CREATE INDEX events_timestamp_idx ON events (timestamp DESC);
```

**Index with NULL ordering:**
```sql
CREATE INDEX users_last_login_idx ON users (last_login DESC NULLS LAST);
```

**Partial index (conditional):**
```sql
CREATE INDEX active_users_idx ON users (email) WHERE active = true;
```

**Expression index (functional index):**
```sql
CREATE INDEX users_lower_email_idx ON users (LOWER(email));
```

**Covering index (with INCLUDE):**
```sql
CREATE INDEX orders_user_id_idx ON orders (user_id) INCLUDE (total, created_at);
```

**Index with specific operator class:**
```sql
CREATE INDEX users_name_pattern_idx ON users (name text_pattern_ops);
```

**Case-insensitive text index:**
```sql
CREATE INDEX users_email_ci_idx ON users (LOWER(email));
```

**GIN index for full-text search:**
```sql
CREATE INDEX documents_content_gin_idx ON documents USING GIN (to_tsvector('english', content));
```

**GIN index for JSONB:**
```sql
CREATE INDEX events_data_gin_idx ON events USING GIN (data);
```

**GIN index for arrays:**
```sql
CREATE INDEX products_tags_gin_idx ON products USING GIN (tags);
```

**Hash index:**
```sql
CREATE INDEX users_id_hash_idx ON users USING HASH (id);
```

**Concurrent index creation (doesn't block writes):**
```sql
CREATE INDEX CONCURRENTLY users_email_idx ON users (email);
```

**BRIN index (block range index for very large tables):**
```sql
CREATE INDEX logs_timestamp_brin_idx ON logs USING BRIN (timestamp);
```

**Multi-column partial index:**
```sql
CREATE INDEX orders_user_status_idx ON orders (user_id, status)
WHERE status IN ('pending', 'processing');
```

**Expression index on JSON field:**
```sql
CREATE INDEX users_metadata_email_idx ON users ((data->>'email'));
```

### Index Types

**B-tree (default)** - General purpose, supports sorting, range queries:
```sql
CREATE INDEX idx ON table (column);  -- uses btree by default
CREATE INDEX idx ON table USING BTREE (column);
```

**Hash** - Equality comparisons only, faster than B-tree for simple lookups:
```sql
CREATE INDEX idx ON table USING HASH (column);
```

**GiST** - Geometric and text search data:
```sql
CREATE INDEX idx ON table USING GIST (geom_column);
```

**SP-GiST** - Space-partitioned data:
```sql
CREATE INDEX idx ON table USING SPGIST (point_column);
```

**GIN** - Multi-valued data (arrays, full-text, JSONB):
```sql
CREATE INDEX idx ON table USING GIN (array_column);
CREATE INDEX idx ON table USING GIN (jsonb_column);
```

**BRIN** - Very large tables with natural ordering:
```sql
CREATE INDEX idx ON table USING BRIN (timestamp_column);
```

### Notes

- B-tree is the default and most versatile index type
- Indexes speed up queries but slow down INSERT/UPDATE/DELETE
- Partial indexes reduce index size and maintenance cost
- Expression indexes enable indexing computed values
- INCLUDE columns create covering indexes (index-only scans)
- CONCURRENTLY allows index creation without table locking
- Hash indexes are now crash-safe (PostgreSQL 10+)
- UNIQUE indexes also create a constraint

### Related Statements

- [DROP INDEX](#drop-index)
- [REINDEX](#reindex-not-yet-documented)
- [CREATE TABLE](02_tables_and_constraints.md)

---

## DROP INDEX

Removes an index from the database.

### Description

`DROP INDEX` removes an existing index. Optionally drops dependent objects with CASCADE.

### Syntax

```sql
DROP INDEX [ CONCURRENTLY ] [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]
```

### Parameters

- **CONCURRENTLY** - Drop without locking the table
- **IF EXISTS** - Do not error if index doesn't exist
- **name** - Name of the index to drop
- **CASCADE** - Drop objects that depend on the index
- **RESTRICT** - Refuse if any objects depend on it (default)

### Examples

**Drop an index:**
```sql
DROP INDEX users_email_idx;
```

**Drop with IF EXISTS:**
```sql
DROP INDEX IF EXISTS users_name_idx;
```

**Drop multiple indexes:**
```sql
DROP INDEX users_email_idx, users_name_idx, users_phone_idx;
```

**Drop concurrently (doesn't block):**
```sql
DROP INDEX CONCURRENTLY users_old_idx;
```

**Drop with CASCADE:**
```sql
DROP INDEX users_email_idx CASCADE;
```

### Notes

- Dropping an index does not affect table data
- UNIQUE indexes that enforce constraints must be dropped using ALTER TABLE DROP CONSTRAINT
- CONCURRENTLY avoids locking but takes longer
- IF EXISTS is useful in migration scripts

### Related Statements

- [CREATE INDEX](#create-index)

---

## CREATE VIEW

Creates a named query (virtual table).

### Description

`CREATE VIEW` defines a view - a named query that can be referenced like a table. Views simplify complex queries, provide abstraction, and can restrict data access.

### Syntax

```sql
CREATE [ OR REPLACE ] [ TEMP | TEMPORARY ] [ RECURSIVE ] VIEW name [ ( column_name [, ...] ) ]
    [ WITH ( view_option_name [= view_option_value] [, ... ] ) ]
    AS query
    [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
```

### Parameters

- **OR REPLACE** - Replace existing view if it exists
- **TEMP / TEMPORARY** - Create a temporary view (session-scoped)
- **RECURSIVE** - Create a recursive view
- **name** - Name of the view
- **column_name** - Optional column name aliases
- **query** - SELECT query defining the view
- **WITH CHECK OPTION** - Ensure INSERTs/UPDATEs through view satisfy view conditions

### Examples

**Basic view:**
```sql
CREATE VIEW active_users AS
SELECT * FROM users WHERE active = true;
```

**View with column aliases:**
```sql
CREATE VIEW user_summary (user_id, full_name, email_address) AS
SELECT id, name, email FROM users;
```

**View with OR REPLACE:**
```sql
CREATE OR REPLACE VIEW recent_orders AS
SELECT * FROM orders
WHERE created_at > NOW() - INTERVAL '7 days';
```

**Join view:**
```sql
CREATE VIEW order_details AS
SELECT
    o.id AS order_id,
    u.name AS customer_name,
    o.total,
    o.created_at
FROM orders o
JOIN users u ON o.user_id = u.id;
```

**Aggregation view:**
```sql
CREATE VIEW monthly_sales AS
SELECT
    DATE_TRUNC('month', created_at) AS month,
    COUNT(*) AS order_count,
    SUM(total) AS total_sales
FROM orders
GROUP BY DATE_TRUNC('month', created_at);
```

**View with WHERE filter:**
```sql
CREATE VIEW premium_users AS
SELECT id, name, email
FROM users
WHERE account_type = 'premium';
```

**Recursive view (hierarchy):**
```sql
CREATE RECURSIVE VIEW employee_hierarchy AS
    -- Base case
    SELECT emp_id, name, manager_id, 1 AS level
    FROM employees
    WHERE manager_id IS NULL
  UNION ALL
    -- Recursive case
    SELECT e.emp_id, e.name, e.manager_id, eh.level + 1
    FROM employees e
    JOIN employee_hierarchy eh ON e.manager_id = eh.emp_id;
```

**Temporary view:**
```sql
CREATE TEMP VIEW session_analysis AS
SELECT * FROM events WHERE session_id = current_setting('app.session_id');
```

**View with CHECK OPTION:**
```sql
CREATE VIEW active_admins AS
SELECT * FROM users
WHERE role = 'admin' AND active = true
WITH CHECK OPTION;

-- This will fail because it violates the view condition
UPDATE active_admins SET active = false WHERE id = 1;
```

**Complex analytical view:**
```sql
CREATE VIEW customer_lifetime_value AS
SELECT
    u.id AS customer_id,
    u.name,
    COUNT(o.id) AS order_count,
    SUM(o.total) AS total_spent,
    AVG(o.total) AS avg_order_value,
    MAX(o.created_at) AS last_order_date
FROM users u
LEFT JOIN orders o ON u.id = o.user_id
GROUP BY u.id, u.name;
```

### Notes

- Views do not store data (computed each time they're queried)
- OR REPLACE is convenient for updating view definitions
- Recursive views can query hierarchical or graph data
- CHECK OPTION ensures data modifications through the view satisfy view conditions
- Temporary views are automatically dropped at session end
- Views can be used in queries just like tables

### Related Statements

- [ALTER VIEW](#alter-view-not-yet-documented)
- [DROP VIEW](#drop-view)
- [CREATE MATERIALIZED VIEW](#create-materialized-view)

---

## CREATE MATERIALIZED VIEW

Creates a view that physically stores query results.

### Description

`CREATE MATERIALIZED VIEW` creates a view that actually stores the query results on disk. Unlike regular views, materialized views cache results for faster access but must be refreshed to update data.

### Syntax

```sql
CREATE MATERIALIZED VIEW [ IF NOT EXISTS ] table_name
    [ ( column_name [, ...] ) ]
    [ USING method ]
    [ WITH ( storage_parameter [= value] [, ... ] ) ]
    [ TABLESPACE tablespace_name ]
    AS query
    [ WITH [ NO ] DATA ]
```

### Parameters

- **IF NOT EXISTS** - Do not error if view already exists
- **table_name** - Name of the materialized view
- **column_name** - Optional column name aliases
- **query** - SELECT query to materialize
- **WITH DATA** - Populate the view immediately (default)
- **WITH NO DATA** - Create empty view (populate later with REFRESH)

### Examples

**Basic materialized view:**
```sql
CREATE MATERIALIZED VIEW daily_sales AS
SELECT
    DATE_TRUNC('day', created_at) AS sale_date,
    COUNT(*) AS order_count,
    SUM(total) AS total_sales
FROM orders
GROUP BY DATE_TRUNC('day', created_at);
```

**Materialized view with IF NOT EXISTS:**
```sql
CREATE MATERIALIZED VIEW IF NOT EXISTS customer_summary AS
SELECT
    user_id,
    COUNT(*) AS order_count,
    SUM(total) AS lifetime_value
FROM orders
GROUP BY user_id;
```

**Create without initial data:**
```sql
CREATE MATERIALIZED VIEW product_stats
AS SELECT product_id, COUNT(*) AS sale_count FROM orders GROUP BY product_id
WITH NO DATA;

-- Populate later
REFRESH MATERIALIZED VIEW product_stats;
```

**Complex analytical materialized view:**
```sql
CREATE MATERIALIZED VIEW sales_by_region_month AS
SELECT
    r.region_name,
    DATE_TRUNC('month', o.created_at) AS month,
    COUNT(DISTINCT o.user_id) AS unique_customers,
    COUNT(o.id) AS order_count,
    SUM(o.total) AS revenue,
    AVG(o.total) AS avg_order_value
FROM orders o
JOIN users u ON o.user_id = u.id
JOIN regions r ON u.region_id = r.id
GROUP BY r.region_name, DATE_TRUNC('month', o.created_at);
```

**Materialized view with index:**
```sql
CREATE MATERIALIZED VIEW user_activity_summary AS
SELECT
    user_id,
    MAX(login_at) AS last_login,
    COUNT(*) AS login_count
FROM user_logins
GROUP BY user_id;

-- Add index for faster lookups
CREATE INDEX ON user_activity_summary (user_id);
```

### Refreshing Materialized Views

**Refresh (locks view during refresh):**
```sql
REFRESH MATERIALIZED VIEW daily_sales;
```

**Concurrent refresh (doesn't lock, requires unique index):**
```sql
CREATE UNIQUE INDEX ON daily_sales (sale_date);
REFRESH MATERIALIZED VIEW CONCURRENTLY daily_sales;
```

### Notes

- Materialized views store actual data on disk (like tables)
- Must be refreshed to see updated source data
- CONCURRENTLY refresh requires a unique index
- Useful for expensive queries that don't need real-time data
- Can create indexes on materialized views
- WITH NO DATA creates the view structure without populating it

### Related Statements

- [REFRESH MATERIALIZED VIEW](#refresh-materialized-view)
- [DROP MATERIALIZED VIEW](#drop-materialized-view)
- [CREATE VIEW](#create-view)

---

## DROP VIEW

Removes a view from the database.

### Description

`DROP VIEW` removes one or more views. Use CASCADE to also drop dependent objects.

### Syntax

```sql
DROP VIEW [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]
```

### Parameters

- **IF EXISTS** - Do not error if view doesn't exist
- **name** - Name of the view to drop
- **CASCADE** - Drop objects that depend on the view
- **RESTRICT** - Refuse if any objects depend on it (default)

### Examples

**Drop a view:**
```sql
DROP VIEW active_users;
```

**Drop with IF EXISTS:**
```sql
DROP VIEW IF EXISTS temp_analysis;
```

**Drop multiple views:**
```sql
DROP VIEW active_users, recent_orders, monthly_sales;
```

**Drop with CASCADE:**
```sql
DROP VIEW user_summary CASCADE;
```

### Notes

- Views don't contain data, only the query definition
- CASCADE drops views and other objects that depend on this view
- IF EXISTS prevents errors in repeatable scripts

### Related Statements

- [CREATE VIEW](#create-view)

---

## DROP MATERIALIZED VIEW

Removes a materialized view from the database.

### Description

`DROP MATERIALIZED VIEW` removes a materialized view and its stored data.

### Syntax

```sql
DROP MATERIALIZED VIEW [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]
```

### Parameters

- **IF EXISTS** - Do not error if view doesn't exist
- **name** - Name of the materialized view to drop
- **CASCADE** - Drop dependent objects
- **RESTRICT** - Refuse if any objects depend on it (default)

### Examples

**Drop a materialized view:**
```sql
DROP MATERIALIZED VIEW daily_sales;
```

**Drop with IF EXISTS:**
```sql
DROP MATERIALIZED VIEW IF NOT EXISTS summary_stats;
```

**Drop with CASCADE:**
```sql
DROP MATERIALIZED VIEW customer_summary CASCADE;
```

### Notes

- Drops both the view definition and stored data
- CASCADE drops dependent objects (views, etc.)

### Related Statements

- [CREATE MATERIALIZED VIEW](#create-materialized-view)
- [REFRESH MATERIALIZED VIEW](#refresh-materialized-view)

---

## REFRESH MATERIALIZED VIEW

Updates a materialized view with current data.

### Description

`REFRESH MATERIALIZED VIEW` re-executes the view's query and updates the stored results.

### Syntax

```sql
REFRESH MATERIALIZED VIEW [ CONCURRENTLY ] name [ WITH [ NO ] DATA ]
```

### Parameters

- **CONCURRENTLY** - Refresh without locking (requires unique index)
- **name** - Name of the materialized view to refresh
- **WITH DATA** - Populate the view (default)
- **WITH NO DATA** - Clear the view data

### Examples

**Standard refresh:**
```sql
REFRESH MATERIALIZED VIEW daily_sales;
```

**Concurrent refresh (non-blocking):**
```sql
REFRESH MATERIALIZED VIEW CONCURRENTLY daily_sales;
```

**Clear view data:**
```sql
REFRESH MATERIALIZED VIEW daily_sales WITH NO DATA;
```

**Refresh with subsequent query:**
```sql
REFRESH MATERIALIZED VIEW customer_summary;
SELECT * FROM customer_summary WHERE lifetime_value > 1000;
```

### Notes

- Standard refresh locks the view (cannot be queried during refresh)
- CONCURRENTLY allows queries during refresh but requires a unique index
- Use scheduled jobs or triggers to refresh automatically
- Consider partitioning large materialized views for incremental updates

### Related Statements

- [CREATE MATERIALIZED VIEW](#create-materialized-view)

---

## CREATE SEQUENCE

Creates a sequence number generator.

### Description

`CREATE SEQUENCE` creates a new sequence object that generates unique numeric values. Sequences are commonly used for auto-incrementing primary keys.

### Syntax

```sql
CREATE SEQUENCE [ IF NOT EXISTS ] name
    [ AS data_type ]
    [ INCREMENT [ BY ] increment ]
    [ MINVALUE minvalue | NO MINVALUE ]
    [ MAXVALUE maxvalue | NO MAXVALUE ]
    [ START [ WITH ] start ]
    [ CACHE cache ]
    [ [ NO ] CYCLE ]
    [ OWNED BY { table_name.column_name | NONE } ]
```

### Parameters

- **IF NOT EXISTS** - Do not error if sequence already exists
- **name** - Name of the sequence
- **data_type** - Data type (smallint, integer, bigint)
- **INCREMENT BY** - Value to add each time (default 1)
- **MINVALUE / MAXVALUE** - Range bounds
- **START WITH** - Starting value
- **CACHE** - Pre-allocate sequence numbers for performance
- **CYCLE / NO CYCLE** - Whether to wrap around at max/min
- **OWNED BY** - Associate with a table column (auto-drop with column)

### Examples

**Basic sequence:**
```sql
CREATE SEQUENCE user_id_seq;
```

**Sequence with START:**
```sql
CREATE SEQUENCE user_id_seq START WITH 1000;
```

**Sequence with INCREMENT:**
```sql
CREATE SEQUENCE even_numbers START WITH 2 INCREMENT BY 2;
```

**Descending sequence:**
```sql
CREATE SEQUENCE countdown START WITH 100 INCREMENT BY -1 MINVALUE 1;
```

**Sequence with range limits:**
```sql
CREATE SEQUENCE limited_seq
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 1000
    CYCLE;
```

**Sequence with caching:**
```sql
CREATE SEQUENCE high_volume_seq
    START WITH 1
    INCREMENT BY 1
    CACHE 100;
```

**Sequence owned by table column:**
```sql
CREATE SEQUENCE users_id_seq OWNED BY users.id;
```

**Using sequence with table:**
```sql
CREATE SEQUENCE order_id_seq;

CREATE TABLE orders (
    id INTEGER DEFAULT nextval('order_id_seq') PRIMARY KEY,
    customer_id INTEGER,
    total NUMERIC
);
```

**SERIAL shorthand (creates sequence automatically):**
```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,  -- Creates sequence users_id_seq automatically
    name TEXT
);
```

### Sequence Functions

**Get next value:**
```sql
SELECT nextval('user_id_seq');
```

**Get current value (without incrementing):**
```sql
SELECT currval('user_id_seq');
```

**Set sequence value:**
```sql
SELECT setval('user_id_seq', 1000);
```

**Set value with is_called flag:**
```sql
SELECT setval('user_id_seq', 1000, true);   -- next call returns 1001
SELECT setval('user_id_seq', 1000, false);  -- next call returns 1000
```

**Get last value:**
```sql
SELECT last_value FROM user_id_seq;
```

### Notes

- SERIAL, BIGSERIAL, SMALLSERIAL types create sequences automatically
- Sequences are not transaction-safe (values are never rolled back)
- CACHE improves performance but may create gaps after crash
- OWNED BY makes sequence lifecycle tied to table column
- Use setval() carefully to avoid duplicate key violations

### Related Statements

- [ALTER SEQUENCE](#alter-sequence)
- [DROP SEQUENCE](#drop-sequence)

---

## ALTER SEQUENCE

Modifies a sequence's parameters.

### Description

`ALTER SEQUENCE` changes the properties of an existing sequence.

### Syntax

```sql
ALTER SEQUENCE [ IF EXISTS ] name
    [ AS data_type ]
    [ INCREMENT [ BY ] increment ]
    [ MINVALUE minvalue | NO MINVALUE ]
    [ MAXVALUE maxvalue | NO MAXVALUE ]
    [ START [ WITH ] start ]
    [ RESTART [ [ WITH ] restart ] ]
    [ CACHE cache ]
    [ [ NO ] CYCLE ]
    [ OWNED BY { table_name.column_name | NONE } ]
```

### Parameters

- Same as CREATE SEQUENCE, plus:
- **RESTART** - Reset sequence to a specific value (or initial START value)

### Examples

**Change increment:**
```sql
ALTER SEQUENCE user_id_seq INCREMENT BY 10;
```

**Restart sequence:**
```sql
ALTER SEQUENCE user_id_seq RESTART WITH 1;
```

**Change max value:**
```sql
ALTER SEQUENCE user_id_seq MAXVALUE 1000000;
```

**Enable cycling:**
```sql
ALTER SEQUENCE user_id_seq CYCLE;
```

**Change ownership:**
```sql
ALTER SEQUENCE user_id_seq OWNED BY users.id;
```

**Remove ownership:**
```sql
ALTER SEQUENCE user_id_seq OWNED BY NONE;
```

### Notes

- Changes take effect for subsequent nextval() calls
- RESTART resets the current value
- Cannot change data type if sequence is in use

### Related Statements

- [CREATE SEQUENCE](#create-sequence)
- [DROP SEQUENCE](#drop-sequence)

---

## DROP SEQUENCE

Removes a sequence from the database.

### Description

`DROP SEQUENCE` removes one or more sequence objects.

### Syntax

```sql
DROP SEQUENCE [ IF EXISTS ] name [, ...] [ CASCADE | RESTRICT ]
```

### Parameters

- **IF EXISTS** - Do not error if sequence doesn't exist
- **name** - Name of the sequence to drop
- **CASCADE** - Drop objects that depend on the sequence
- **RESTRICT** - Refuse if any objects depend on it (default)

### Examples

**Drop a sequence:**
```sql
DROP SEQUENCE user_id_seq;
```

**Drop with IF EXISTS:**
```sql
DROP SEQUENCE IF EXISTS temp_seq;
```

**Drop multiple sequences:**
```sql
DROP SEQUENCE seq1, seq2, seq3;
```

**Drop with CASCADE:**
```sql
DROP SEQUENCE user_id_seq CASCADE;
```

### Notes

- Dropping a sequence fails if columns use it (unless CASCADE)
- Sequences created by SERIAL are automatically dropped with the table
- CASCADE drops dependent defaults on table columns

### Related Statements

- [CREATE SEQUENCE](#create-sequence)
- [ALTER SEQUENCE](#alter-sequence)

---

## Best Practices

### Indexing Strategy

- Index foreign key columns
- Index columns used in WHERE clauses
- Index columns used in JOIN conditions
- Use partial indexes for filtered queries
- Create covering indexes for frequently queried column sets
- Don't over-index (each index slows down writes)

### View Usage

- Use views to simplify complex queries
- Use views for access control (grant on view, not base tables)
- Use materialized views for expensive aggregations
- Refresh materialized views on schedule
- Add indexes to materialized views for query performance

### Sequence Management

- Use SERIAL types for simplicity
- Pre-allocate with CACHE for high-volume inserts
- Use CYCLE carefully (usually want NO CYCLE)
- Use OWNED BY to tie sequence lifecycle to table
- Consider UUID for distributed systems (instead of sequences)

---

## Known Limitations

### Stubbed Implementation

🚧 **CREATE INDEX** - Parser accepts full syntax but bytecode format mismatch prevents execution. Parser emits different name/table/flags ordering than executor expects.

🚧 **DROP INDEX** - Parser emits different format than executor expects.

🚧 **CREATE VIEW** - Parser emits SELECT bytecode, but executor expects SQL string and flags. Bytecode structure mismatch prevents execution.

🚧 **DROP VIEW** - Stubbed due to payload mismatch.

🚧 **CREATE MATERIALIZED VIEW** - Uses CREATE_VIEW opcode with materialized flag but format doesn't match executor expectations.

🚧 **CREATE SEQUENCE** - Parser drops sequence options during emission; executor expects different payload structure.

🚧 **ALTER SEQUENCE** - Bytecode payload mismatch.

🚧 **DROP SEQUENCE** - Bytecode payload mismatch.

### Missing Features

❌ **CONCURRENTLY for Indexes** - CONCURRENTLY flag is parsed but concurrent index building is not implemented.

❌ **Advanced Index Types** - GiST, SP-GiST, GIN, BRIN parsing is supported but execution depends on index subsystem implementation.

❌ **REFRESH MATERIALIZED VIEW** - Not yet implemented in parser.

### Spec Deltas

📝 **View Definitions** - The parser emits compiled SELECT bytecode instead of storing the original SQL string. The executor expects a SQL string that it can re-execute. This prevents views from working end-to-end.

📝 **Sequence Options** - Parser parses all sequence options (START, INCREMENT, MINVALUE, etc.) but doesn't emit them in bytecode. Only sequence name and flags are emitted.

📝 **Index Methods** - USING clause for index methods is parsed but the specific index type (hash, gin, etc.) may not be fully supported by the storage engine.

---

## See Also

### Related Documentation
- [Tables and Constraints](02_tables_and_constraints.md)
- [DML SELECT](06_dml_select.md)
- [Performance Tuning](#not-yet-available)

### Specifications
- `/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `/docs/specifications/ddl/DDL_INDEXES.md`
- `/docs/specifications/ddl/DDL_VIEWS.md`
- `/docs/audit/17_postgresql_parser_statement_reference_actual.md`

### Source Code
- Parser: `/src/parser/postgresql/pg_parser_ddl.cpp`
- Executor: `/src/sblr/executor.cpp`
