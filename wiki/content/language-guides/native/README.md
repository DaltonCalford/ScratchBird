# Native ScratchBird SQL

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

---

## Overview

Native ScratchBird SQL is the primary SQL dialect providing full access to all ScratchBird features. It combines SQL standards with Firebird-inspired extensions.

**Port:** 3092
**CLI Tool:** `sb-isql`

---

## Quick Start

### Connect

```bash
# Using sb-isql
sb-isql -p 3092 -d mydb -u myuser

# Connection string
sb-isql -c "host=localhost port=3092 dbname=mydb user=myuser"
```

### Create Database

```sql
CREATE DATABASE mydb;
```

### Create Table

```sql
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Basic Operations

```sql
-- Insert
INSERT INTO users (id, name, email) VALUES (1, 'Alice', 'alice@example.com');

-- Select
SELECT * FROM users WHERE name LIKE 'A%';

-- Update
UPDATE users SET email = 'new@example.com' WHERE id = 1;

-- Delete
DELETE FROM users WHERE id = 1;
```

---

## Parser Pipeline

Native SQL follows this processing pipeline:

```
SQL Text
    │
    ▼
┌──────────────────┐
│  V2 Parser       │  src/parser/parser_v2.cpp
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  AST v2          │  Abstract Syntax Tree
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Semantic        │  src/sblr/semantic_analyzer_v2.cpp
│  Analyzer        │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Bytecode        │  src/sblr/bytecode_generator_v2.cpp
│  Generator       │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Executor        │  src/sblr/executor.cpp
└─────────────────┘
```

---

## Language Topics

### Data Definition
- [Databases and Schemas](01_databases_and_schemas.md) - CREATE DATABASE, CREATE SCHEMA
- [Tables and Constraints](02_tables_and_constraints.md) - CREATE TABLE, constraints
- [Indexes, Views, Sequences](03_indexes_views_sequences.md) - Indexes, views, sequences
- [Types and Domains](04_types_and_domains.md) - Data types, custom domains

### Queries
- [DML: SELECT](06_dml_select.md) - Queries, joins, subqueries
- [DML: Modification](07_dml_modification.md) - INSERT, UPDATE, DELETE

### Programmability
- [Programmable SQL](05_programmable_sql.md) - Stored procedures, triggers
- [Functions](14_functions.md) - Built-in functions

### Administration
- [Transactions](08_transactions.md) - Transaction control, isolation
- [Security (DCL)](09_security_dcl.md) - Users, roles, permissions
- [Session, SHOW, SET](10_session_show_set.md) - Session management
- [Utilities](11_utilities.md) - EXPLAIN, COPY, maintenance

### Reference
- [Operators](12_operators.md) - All operators
- [System Catalog](13_system_catalog.md) - sb_catalog tables

---

## Key Features

### Index Types

Native SQL supports multiple index types:

```sql
-- B-Tree (default, general purpose)
CREATE INDEX idx_name ON users (name);

-- Hash (equality only)
CREATE INDEX idx_email ON users USING HASH (email);

-- Bitmap (low cardinality)
CREATE INDEX idx_status ON orders USING BITMAP (status);

-- GIN (full-text, arrays, JSON)
CREATE INDEX idx_tags ON posts USING GIN (tags);

-- GIST (spatial)
CREATE INDEX idx_location ON places USING GIST (location);
```

### JSON/JSONB

Full JSON support:

```sql
CREATE TABLE documents (
    id INT PRIMARY KEY,
    data JSONB
);

INSERT INTO documents VALUES (1, '{"name": "test", "tags": ["a", "b"]}');

SELECT data->>'name' FROM documents;
SELECT data->'tags'->0 FROM documents;
```

### Arrays

```sql
CREATE TABLE inventory (
    id INT PRIMARY KEY,
    values INT[]
);

INSERT INTO inventory VALUES (1, ARRAY[10, 20, 30]);
SELECT * FROM inventory WHERE values[1] > 5;
```

### Common Table Expressions

```sql
WITH active_users AS (
    SELECT * FROM users WHERE active = true
)
SELECT * FROM active_users WHERE created_at > NOW() - INTERVAL '30 days';

-- Recursive CTE
WITH RECURSIVE tree AS (
    SELECT id, parent_id, name, 1 as depth
    FROM categories WHERE parent_id IS NULL
    UNION ALL
    SELECT c.id, c.parent_id, c.name, t.depth + 1
    FROM categories c JOIN tree t ON c.parent_id = t.id
)
SELECT * FROM tree;
```

### Window Functions

```sql
SELECT
    name,
    department,
    salary,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) as rank
FROM employees;
```

---

## System Catalog

Native SQL uses the `sb_catalog` schema for system tables:

```sql
-- List tables
SELECT * FROM sb_catalog.tables;

-- List columns
SELECT * FROM sb_catalog.columns WHERE table_name = 'users';

-- List indexes
SELECT * FROM sb_catalog.indexes;

-- List constraints
SELECT * FROM sb_catalog.constraints;
```

---

## Transaction Syntax

```sql
-- Begin transaction
BEGIN;
-- or
START TRANSACTION;

-- With isolation level
BEGIN ISOLATION LEVEL SERIALIZABLE;
BEGIN ISOLATION LEVEL REPEATABLE READ;
BEGIN ISOLATION LEVEL READ COMMITTED;

-- Savepoints
SAVEPOINT sp1;
-- ... work ...
ROLLBACK TO SAVEPOINT sp1;
RELEASE SAVEPOINT sp1;

-- Commit or rollback
COMMIT;
ROLLBACK;
```

---

## Session Commands

```sql
-- Show settings
SHOW ALL;
SHOW work_mem;

-- Set session variables
SET work_mem = '256MB';
SET search_path = 'myschema, public';

-- Show current values
SELECT current_database();
SELECT current_schema();
SELECT current_user;
```

---

## Utility Commands

```sql
-- Query plan
EXPLAIN SELECT * FROM users WHERE id = 1;
EXPLAIN ANALYZE SELECT * FROM users WHERE id = 1;

-- Bulk load
COPY users FROM '/path/to/data.csv' WITH (FORMAT CSV, HEADER true);
COPY users TO '/path/to/export.csv' WITH (FORMAT CSV, HEADER true);
```

---

## Differences from Other Dialects

| Feature | Native | PostgreSQL | MySQL | Firebird |
|---------|--------|------------|-------|----------|
| Type casting | CAST() | `::type` | CAST() | CAST() |
| String concat | `\|\|` | `\|\|` | CONCAT() | `\|\|` |
| Auto ID | SERIAL / GENERATED | SERIAL | AUTO_INCREMENT | GENERATOR |
| Case sensitivity | Insensitive | Insensitive | Insensitive | Insensitive |
| Quoted identifiers | `"name"` | `"name"` | `` `name` `` | `"name"` |
| System catalog | sb_catalog | pg_catalog | information_schema | RDB$ |

---

## Implementation Notes

- V2 is the core dialect
- Features marked as PostgreSQL-style extensions are intentional extensions
- Temporary table flags are parsed but not fully applied end-to-end (see critical findings)

---

## Related Documents

- [SQL Syntax Reference](../../reference/SQL-Syntax.md)
- [Functions Reference](../../reference/Functions.md)
- [Data Types Reference](../../reference/Data-Types.md)
- [Operators Reference](../../reference/Operators.md)
- [PostgreSQL Guide](../postgresql/README.md)
- [MySQL Guide](../mysql/README.md)
- [Firebird Guide](../firebirdsql/README.md)
