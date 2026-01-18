[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL Indexes, Views, and Sequences

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

This document covers secondary database objects in MySQL emulation mode: indexes, views, and sequences. These objects enhance query performance, provide abstraction layers, and manage auto-incrementing values.

**Important:** Most features in this area are not yet implemented in the MySQL parser. This documentation describes the intended functionality and current implementation status.

---

## Indexes

### Overview

Indexes improve query performance by creating efficient lookup structures on table columns. MySQL supports several index types including B-tree, hash, full-text, and spatial indexes.

### CREATE INDEX

Creates an index on one or more columns of a table.

#### Syntax

```sql
CREATE [UNIQUE | FULLTEXT | SPATIAL] INDEX index_name
    ON table_name (column_name [(length)] [ASC | DESC], ...)
    [USING {BTREE | HASH}]
    [COMMENT 'string']
    [ALGORITHM {DEFAULT | INPLACE | COPY}]
    [LOCK {DEFAULT | NONE | SHARED | EXCLUSIVE}]
```

#### Parameters

- `UNIQUE`: Creates a unique index where all values must be distinct
- `FULLTEXT`: Creates a full-text search index (for TEXT columns)
- `SPATIAL`: Creates a spatial index (for geometry types)
- `index_name`: Name of the index to create
- `table_name`: Name of the table on which to create the index
- `column_name`: Column(s) to include in the index
- `length`: For string columns, number of characters to index (prefix indexing)
- `ASC | DESC`: Sort order for the index (ascending or descending)
- `USING {BTREE | HASH}`: Index implementation method
- `COMMENT`: Descriptive comment for the index
- `ALGORITHM`: How MySQL processes the index operation
- `LOCK`: Locking strategy during index creation

#### Examples

**Basic index:**
```sql
CREATE INDEX idx_email ON users (email);
```

**Unique index:**
```sql
CREATE UNIQUE INDEX idx_username ON users (username);
```

**Composite index:**
```sql
CREATE INDEX idx_name ON users (last_name, first_name);
```

**Index with sort order:**
```sql
CREATE INDEX idx_created ON orders (created_at DESC);
```

**Prefix index (for long strings):**
```sql
CREATE INDEX idx_description ON products (description(100));
```

**Full-text index:**
```sql
CREATE FULLTEXT INDEX idx_content ON articles (title, content);
```

**Spatial index:**
```sql
CREATE SPATIAL INDEX idx_location ON places (coordinates);
```

**Index with specific type:**
```sql
CREATE INDEX idx_hash ON cache (key_name) USING HASH;
```

**Index with comment:**
```sql
CREATE INDEX idx_status ON orders (status) COMMENT 'Index for order status queries';
```

#### Notes

- B-tree indexes are the default and most common type
- Unique indexes enforce uniqueness constraints
- Full-text indexes enable full-text search capabilities
- Spatial indexes support geographic queries
- Prefix indexes save space for long string columns

#### Current Status

**NOT IMPLEMENTED:** The CREATE INDEX statement is not currently implemented in the MySQL parser. Attempts to create standalone indexes will result in parse errors.

**Workaround:** Define indexes inline within CREATE TABLE statements:
```sql
CREATE TABLE users (
    id INT PRIMARY KEY,
    email VARCHAR(255),
    username VARCHAR(100),
    INDEX idx_email (email),
    UNIQUE INDEX idx_username (username)
);
```

### DROP INDEX

Removes an index from a table.

#### Syntax

```sql
DROP INDEX index_name ON table_name
    [ALGORITHM {DEFAULT | INPLACE | COPY}]
    [LOCK {DEFAULT | NONE | SHARED | EXCLUSIVE}]
```

#### Parameters

- `index_name`: Name of the index to drop
- `table_name`: Name of the table containing the index
- `ALGORITHM`: How MySQL processes the drop operation
- `LOCK`: Locking strategy during index removal

#### Examples

**Drop an index:**
```sql
DROP INDEX idx_email ON users;
```

**Drop with online operation:**
```sql
DROP INDEX idx_status ON orders ALGORITHM=INPLACE LOCK=NONE;
```

#### Notes

- Dropping an index does not affect table data
- Primary key indexes cannot be dropped with DROP INDEX
- Use ALTER TABLE to drop primary keys

#### Current Status

**NOT IMPLEMENTED:** The DROP INDEX statement is not currently implemented in the MySQL parser and will result in parse errors.

### ALTER TABLE ... ADD/DROP INDEX

Alternative syntax for managing indexes through ALTER TABLE.

#### Syntax

```sql
ALTER TABLE table_name
    ADD [UNIQUE | FULLTEXT | SPATIAL] INDEX index_name (column_name, ...);

ALTER TABLE table_name
    DROP INDEX index_name;
```

#### Examples

**Add index via ALTER TABLE:**
```sql
ALTER TABLE users ADD INDEX idx_email (email);
```

**Add unique index via ALTER TABLE:**
```sql
ALTER TABLE users ADD UNIQUE INDEX idx_username (username);
```

**Drop index via ALTER TABLE:**
```sql
ALTER TABLE users DROP INDEX idx_email;
```

#### Current Status

**NOT IMPLEMENTED:** These ALTER TABLE clauses are not currently supported. Only ALTER TABLE ... RENAME TO is implemented.

### Index Usage Notes

#### When to Use Indexes

Indexes improve performance for:
- WHERE clauses filtering on indexed columns
- JOIN conditions on indexed columns
- ORDER BY clauses on indexed columns
- MIN/MAX queries on indexed columns

#### When Not to Use Indexes

Avoid indexes for:
- Small tables (full table scan is faster)
- Columns with low cardinality (few distinct values)
- Columns frequently updated (index maintenance overhead)
- Wide composite indexes (storage overhead)

#### Index Best Practices

1. **Index selectivity**: Create indexes on columns with high cardinality
2. **Composite index order**: Put most selective columns first
3. **Prefix indexes**: Use for long string columns to save space
4. **Monitor usage**: Remove unused indexes to reduce overhead
5. **Avoid redundancy**: Don't create redundant overlapping indexes

---

## Views

### Overview

Views are virtual tables defined by SELECT queries. They provide data abstraction, simplify complex queries, and can enforce security by limiting column access.

### CREATE VIEW

Creates a view based on a SELECT statement.

#### Syntax

```sql
CREATE [OR REPLACE] [ALGORITHM = {UNDEFINED | MERGE | TEMPTABLE}]
    VIEW view_name [(column_list)]
    AS select_statement
    [WITH [CASCADED | LOCAL] CHECK OPTION]
```

#### Parameters

- `OR REPLACE`: Replace existing view if it exists
- `ALGORITHM`: How MySQL processes the view
  - `UNDEFINED`: Let MySQL choose (default)
  - `MERGE`: Merge view definition into referencing query
  - `TEMPTABLE`: Materialize view as temporary table
- `view_name`: Name of the view to create
- `column_list`: Optional list of column names for the view
- `select_statement`: SELECT query defining the view
- `WITH CHECK OPTION`: Ensures INSERT/UPDATE through view meet WHERE conditions
  - `CASCADED`: Check all underlying views (default)
  - `LOCAL`: Check only this view's WHERE clause

#### Examples

**Basic view:**
```sql
CREATE VIEW active_users AS
    SELECT id, email, name
    FROM users
    WHERE status = 'active';
```

**View with OR REPLACE:**
```sql
CREATE OR REPLACE VIEW user_summary AS
    SELECT
        u.id,
        u.name,
        COUNT(o.id) AS order_count,
        SUM(o.total) AS total_spent
    FROM users u
    LEFT JOIN orders o ON u.id = o.customer_id
    GROUP BY u.id, u.name;
```

**View with explicit column names:**
```sql
CREATE VIEW monthly_sales (month, year, total_sales) AS
    SELECT
        MONTH(order_date),
        YEAR(order_date),
        SUM(total)
    FROM orders
    GROUP BY YEAR(order_date), MONTH(order_date);
```

**View with CHECK OPTION:**
```sql
CREATE VIEW premium_customers AS
    SELECT id, name, email
    FROM customers
    WHERE tier = 'premium'
    WITH CHECK OPTION;

-- This will fail because it violates the WHERE clause:
-- INSERT INTO premium_customers VALUES (1, 'John', 'john@example.com');
```

**Complex view with joins:**
```sql
CREATE VIEW order_details AS
    SELECT
        o.order_id,
        o.order_date,
        c.name AS customer_name,
        p.name AS product_name,
        oi.quantity,
        oi.price
    FROM orders o
    JOIN customers c ON o.customer_id = c.id
    JOIN order_items oi ON o.order_id = oi.order_id
    JOIN products p ON oi.product_id = p.id;
```

**View with subquery:**
```sql
CREATE VIEW high_value_orders AS
    SELECT *
    FROM orders
    WHERE total > (SELECT AVG(total) FROM orders);
```

#### Notes

- Views simplify complex queries by encapsulating logic
- Views can be queried like regular tables
- Some views are updatable (single table, no aggregates)
- Views don't store data (they're virtual)
- View definitions are stored in the data dictionary

#### Updatable Views

A view is updatable if it meets these conditions:
- Based on a single table
- No DISTINCT, GROUP BY, HAVING, or aggregates
- No UNION or subqueries in FROM
- No reference to other views

**Example of updatable view:**
```sql
CREATE VIEW active_users AS
    SELECT id, name, email FROM users WHERE status = 'active';

-- This works because the view is updatable:
UPDATE active_users SET name = 'John Doe' WHERE id = 1;
```

#### Current Status

**NOT IMPLEMENTED:** The CREATE VIEW statement is not currently implemented in the MySQL parser and will result in parse errors.

### DROP VIEW

Removes one or more views.

#### Syntax

```sql
DROP VIEW [IF EXISTS] view_name [, view_name ...]
    [RESTRICT | CASCADE]
```

#### Parameters

- `IF EXISTS`: Prevents error if view doesn't exist
- `view_name`: Name of the view to drop
- `RESTRICT`: Fail if dependent objects exist (default)
- `CASCADE`: Drop dependent objects as well

#### Examples

**Drop a view:**
```sql
DROP VIEW active_users;
```

**Drop with IF EXISTS:**
```sql
DROP VIEW IF EXISTS temporary_view;
```

**Drop multiple views:**
```sql
DROP VIEW view1, view2, view3;
```

#### Current Status

**NOT IMPLEMENTED:** The DROP VIEW statement is not currently implemented in the MySQL parser and will result in parse errors.

### ALTER VIEW

Modifies an existing view definition.

#### Syntax

```sql
ALTER VIEW view_name [(column_list)]
    AS select_statement
    [WITH [CASCADED | LOCAL] CHECK OPTION]
```

#### Examples

**Alter a view:**
```sql
ALTER VIEW active_users AS
    SELECT id, name, email, created_at
    FROM users
    WHERE status = 'active';
```

#### Notes

- ALTER VIEW is equivalent to CREATE OR REPLACE VIEW
- Using CREATE OR REPLACE VIEW is more common

#### Current Status

**NOT IMPLEMENTED:** The ALTER VIEW statement is not currently implemented in the MySQL parser.

---

## Sequences

### Overview

MySQL does not have a standalone SEQUENCE object like PostgreSQL or Oracle. Instead, MySQL uses the AUTO_INCREMENT column attribute to generate sequential values.

### AUTO_INCREMENT

MySQL's mechanism for generating sequential integer values.

#### Syntax

```sql
-- Column definition:
column_name INT AUTO_INCREMENT

-- Table option (sets starting value):
CREATE TABLE table_name (...) AUTO_INCREMENT = start_value;
```

#### Examples

**Basic AUTO_INCREMENT:**
```sql
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100)
);

INSERT INTO users (name) VALUES ('Alice');
INSERT INTO users (name) VALUES ('Bob');
-- id values: 1, 2
```

**Set starting value:**
```sql
CREATE TABLE invoices (
    invoice_id INT AUTO_INCREMENT PRIMARY KEY,
    amount DECIMAL(10, 2)
) AUTO_INCREMENT = 1000;

INSERT INTO invoices (amount) VALUES (99.99);
-- invoice_id: 1000
```

**Alter AUTO_INCREMENT value:**
```sql
ALTER TABLE users AUTO_INCREMENT = 5000;
-- Next insert will use 5000
```

**Get last inserted ID:**
```sql
INSERT INTO users (name) VALUES ('Charlie');
SELECT LAST_INSERT_ID();  -- Returns the auto-generated ID
```

#### AUTO_INCREMENT Behavior

- Values start at 1 by default
- Each INSERT increments the counter
- Deleted IDs are not reused
- Only one AUTO_INCREMENT column per table
- Must be defined on a key (PRIMARY KEY or UNIQUE KEY)
- Can only be used with integer types

#### Resetting AUTO_INCREMENT

```sql
-- Reset to 1 (after TRUNCATE):
TRUNCATE TABLE users;

-- Manually reset:
ALTER TABLE users AUTO_INCREMENT = 1;
```

#### Multi-Column Keys with AUTO_INCREMENT

```sql
CREATE TABLE composite_example (
    category_id INT,
    item_id INT AUTO_INCREMENT,
    name VARCHAR(100),
    PRIMARY KEY (category_id, item_id)
);
-- AUTO_INCREMENT can be part of composite key
```

#### Current Status

**IMPLEMENTED:** AUTO_INCREMENT is fully supported in the MySQL parser and executor. It works as expected for generating sequential values.

### Sequence Alternatives

Since MySQL doesn't have CREATE SEQUENCE, here are common patterns:

#### Pattern 1: Separate Counter Table

```sql
CREATE TABLE sequence_table (
    sequence_name VARCHAR(50) PRIMARY KEY,
    current_value BIGINT NOT NULL
);

-- Initialize:
INSERT INTO sequence_table VALUES ('invoice_seq', 1000);

-- Get next value (application-level):
UPDATE sequence_table SET current_value = current_value + 1 WHERE sequence_name = 'invoice_seq';
SELECT current_value FROM sequence_table WHERE sequence_name = 'invoice_seq';
```

#### Pattern 2: AUTO_INCREMENT Table

```sql
CREATE TABLE sequence_generator (
    id BIGINT AUTO_INCREMENT PRIMARY KEY
);

-- Get next value:
INSERT INTO sequence_generator VALUES (NULL);
SELECT LAST_INSERT_ID();
DELETE FROM sequence_generator WHERE id < LAST_INSERT_ID();
```

### Notes on Sequences

- MySQL's AUTO_INCREMENT is table-specific (not shared across tables)
- For shared sequences, use application-level generators or counter tables
- UUID/GUID functions can provide unique identifiers without sequences
- For distributed systems, consider UUIDv7 or snowflake IDs

---

## Known Limitations

### Missing Features

- **CREATE INDEX**: Standalone CREATE INDEX statements are not implemented. Parse errors will occur.
  - **Workaround**: Define indexes inline within CREATE TABLE statements
  - **Priority**: CRITICAL for Alpha release

- **DROP INDEX**: Not implemented. Cannot drop indexes via SQL.
  - **Workaround**: Recreate table without the index
  - **Priority**: CRITICAL for Alpha release

- **CREATE VIEW**: Not implemented. View creation will fail with parse errors.
  - **Workaround**: Use application-level abstraction or subqueries
  - **Priority**: CRITICAL for Alpha release

- **DROP VIEW**: Not implemented. Cannot drop views via SQL.
  - **Priority**: CRITICAL for Alpha release

- **ALTER VIEW**: Not implemented.
  - **Priority**: Medium

### Stubbed Features

- **Index types in CREATE TABLE**: While index definitions can be included in CREATE TABLE, advanced index types (FULLTEXT, SPATIAL) may not be properly emitted to bytecode:
  ```sql
  CREATE TABLE articles (
      id INT PRIMARY KEY,
      content TEXT,
      FULLTEXT INDEX idx_content (content)  -- Parsed but may not work
  );
  ```

### Working Features

- **AUTO_INCREMENT**: Fully implemented and working correctly
  - Sequential value generation works
  - LAST_INSERT_ID() function supported
  - Table-level AUTO_INCREMENT option supported

### Spec Deltas

- **No standalone SEQUENCE objects**: By design, MySQL does not support CREATE SEQUENCE. Use AUTO_INCREMENT instead.

- **Index algorithm hints**: USING BTREE, USING HASH, and other algorithm hints are parsed but may not affect actual index implementation (ScratchBird uses its own index structures)

- **View algorithms**: ALGORITHM = {MERGE | TEMPTABLE} hints are not supported

### Implementation Priority

According to `/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`:

**Alpha Blockers (Critical):**
- Implement CREATE INDEX (1-2 days)
- Implement DROP INDEX (1 day)
- Implement CREATE VIEW (2-3 days)
- Implement DROP VIEW (1 day)

**Post-Alpha (Medium Priority):**
- Implement ALTER TABLE ... ADD INDEX
- Implement ALTER TABLE ... DROP INDEX
- Implement ALTER VIEW
- Support advanced index types (FULLTEXT, SPATIAL)

**Beta Target (Lower Priority):**
- Full view updatability checking
- Materialized views
- Index usage statistics
