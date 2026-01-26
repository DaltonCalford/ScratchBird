[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native V2 SQL - Tables and Constraints

## Overview

This document describes table management and constraint features in ScratchBird's Native V2 SQL dialect. Tables are the fundamental structures for storing data in a relational database, organized into rows and columns. ScratchBird supports standard SQL tables, temporary tables, and advanced features like generated columns and UUID identities.

Constraints ensure data integrity by enforcing rules on columns and tables, including primary keys, foreign keys, unique constraints, check constraints, and more.

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## CREATE TABLE

### Description

Creates a new table to store data. Tables can be permanent or temporary, and can include various column constraints and table constraints.

### Syntax

```sql
CREATE [TEMPORARY | UNLOGGED] TABLE [IF NOT EXISTS] <table_name> (
    <column_definition> [, ...]
    [, <table_constraint> ...]
)
```

**Column Definition:**
```sql
<column_name> <data_type> [<column_constraint> ...]
```

**Column Constraints:**
- `NOT NULL` - Column cannot contain NULL values
- `NULL` - Column can contain NULL values (default)
- `UNIQUE` - All values in column must be unique
- `PRIMARY KEY` - Combination of NOT NULL and UNIQUE, identifies each row
- `DEFAULT <expression>` - Default value for the column
- `CHECK (<expression>)` - Boolean expression that values must satisfy
- `REFERENCES <table>(<column>)` - Foreign key reference
- `GENERATED ALWAYS AS (<expression>) STORED` - Computed column
- `GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY` - Auto-incrementing column

**Table Constraints:**
- `PRIMARY KEY (<column> [, ...])` - Multi-column primary key
- `UNIQUE (<column> [, ...])` - Multi-column unique constraint
- `FOREIGN KEY (<column> [, ...]) REFERENCES <table> (<column> [, ...])` - Foreign key
- `CHECK (<expression>)` - Table-level check constraint

### Parameters

- **TEMPORARY**: Creates a temporary table visible only to the current session
- **UNLOGGED**: Creates an unlogged table (parsed but not enforced in current implementation)
- **IF NOT EXISTS**: Prevents error if table already exists
- **table_name**: Name of the table, optionally schema-qualified (e.g., `app.users`)

### Examples

**Example 1: Create a basic table**
```sql
CREATE TABLE employees (
    id INTEGER PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE,
    hire_date DATE DEFAULT CURRENT_DATE
);
```

**Example 2: Create a table with foreign keys and check constraints**
```sql
CREATE TABLE orders (
    order_id BIGINT PRIMARY KEY,
    customer_id INTEGER NOT NULL REFERENCES customers(id),
    order_date TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    amount DECIMAL(10, 2) NOT NULL CHECK (amount > 0),
    status VARCHAR(20) CHECK (status IN ('pending', 'shipped', 'delivered', 'cancelled'))
);
```

**Example 3: Create a table with composite primary key**
```sql
CREATE TABLE order_items (
    order_id BIGINT NOT NULL,
    line_number INTEGER NOT NULL,
    product_id INTEGER NOT NULL REFERENCES products(id),
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    unit_price DECIMAL(10, 2) NOT NULL,
    PRIMARY KEY (order_id, line_number),
    FOREIGN KEY (order_id) REFERENCES orders(order_id)
);
```

**Example 4: Create a table with generated identity column**
```sql
CREATE TABLE customers (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**Example 5: Create a table with computed column**
```sql
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    price DECIMAL(10, 2) NOT NULL,
    tax_rate DECIMAL(5, 4) NOT NULL,
    price_with_tax DECIMAL(10, 2) GENERATED ALWAYS AS (price * (1 + tax_rate)) STORED
);
```

**Example 6: Create a temporary table**
```sql
CREATE TEMPORARY TABLE session_data (
    key VARCHAR(100) PRIMARY KEY,
    value TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**Example 7: Create a table with named constraints**
```sql
CREATE TABLE accounts (
    account_id BIGINT PRIMARY KEY,
    account_number VARCHAR(20) NOT NULL,
    balance DECIMAL(15, 2) NOT NULL DEFAULT 0.00,
    CONSTRAINT unique_account_number UNIQUE (account_number),
    CONSTRAINT positive_balance CHECK (balance >= 0)
);
```

### Notes

- Table names must be unique within a schema.
- Column names must be unique within a table.
- Primary key columns automatically get NOT NULL constraint.
- Foreign keys require the referenced table and column to exist.
- Check constraints are evaluated at insert and update time.
- DEFAULT values can be expressions, including functions like CURRENT_TIMESTAMP.
- GENERATED columns are computed automatically and cannot be explicitly set.
- IDENTITY columns provide auto-incrementing values (integers or UUIDs).

---

## ALTER TABLE

### Description

Modifies the structure of an existing table by adding, dropping, or altering columns and constraints.

### Syntax

```sql
ALTER TABLE [IF EXISTS] [ONLY] <table_name>
    { ADD COLUMN <column_definition>
    | DROP COLUMN <column_name>
    | ALTER COLUMN <column_name> { SET DEFAULT <expression>
                                  | DROP DEFAULT
                                  | SET NOT NULL
                                  | DROP NOT NULL
                                  | SET DATA TYPE <data_type> }
    | ADD CONSTRAINT <constraint_definition>
    | DROP CONSTRAINT <constraint_name>
    | RENAME TO <new_table_name>
    | RENAME COLUMN <old_name> TO <new_name> }
```

### Parameters

- **IF EXISTS**: Prevents error if table doesn't exist
- **ONLY**: Applies change only to specified table (not inherited tables)
- **table_name**: Name of table to modify

### Examples

**Example 1: Add a new column**
```sql
ALTER TABLE employees ADD COLUMN phone VARCHAR(20);
```

**Example 2: Add a column with constraints**
```sql
ALTER TABLE employees ADD COLUMN department_id INTEGER REFERENCES departments(id);
```

**Example 3: Drop a column**
```sql
ALTER TABLE employees DROP COLUMN middle_name;
```

**Example 4: Change column data type**
```sql
ALTER TABLE products ALTER COLUMN price SET DATA TYPE DECIMAL(12, 2);
```

**Example 5: Set a default value**
```sql
ALTER TABLE orders ALTER COLUMN status SET DEFAULT 'pending';
```

**Example 6: Remove a default value**
```sql
ALTER TABLE orders ALTER COLUMN shipped_date DROP DEFAULT;
```

**Example 7: Make a column NOT NULL**
```sql
ALTER TABLE customers ALTER COLUMN email SET NOT NULL;
```

**Example 8: Allow NULL values**
```sql
ALTER TABLE orders ALTER COLUMN notes DROP NOT NULL;
```

**Example 9: Add a named constraint**
```sql
ALTER TABLE accounts ADD CONSTRAINT chk_positive_balance CHECK (balance >= 0);
```

**Example 10: Drop a constraint**
```sql
ALTER TABLE accounts DROP CONSTRAINT chk_positive_balance;
```

**Example 11: Rename a table**
```sql
ALTER TABLE employees RENAME TO staff;
```

**Example 12: Rename a column**
```sql
ALTER TABLE customers RENAME COLUMN phone TO phone_number;
```

### Notes

- Adding NOT NULL to a column fails if existing rows contain NULL values.
- Dropping a column referenced by foreign keys requires dropping the foreign keys first.
- Type changes may fail if existing data cannot be converted to the new type.
- Constraint names must be unique within a table.
- Renaming a table or column updates all internal references automatically.

---

## DROP TABLE

### Description

Removes a table and all its data from the database. This operation is irreversible.

**WARNING:** All data in the table will be permanently lost.

### Syntax

```sql
DROP TABLE [IF EXISTS] <table_name> [CASCADE | RESTRICT]
```

### Parameters

- **IF EXISTS**: Prevents error if table doesn't exist
- **CASCADE**: Automatically drops dependent objects (views, foreign keys)
- **RESTRICT**: Default. Prevents drop if dependencies exist

### Examples

**Example 1: Drop a table**
```sql
DROP TABLE old_orders;
```

**Example 2: Safely drop a table that might not exist**
```sql
DROP TABLE IF EXISTS temp_calculations;
```

**Example 3: Drop a table and its dependent objects**
```sql
DROP TABLE products CASCADE;
```

**Example 4: Prevent dropping if dependencies exist**
```sql
DROP TABLE customers RESTRICT;
```

**Example 5: Drop multiple tables in sequence**
```sql
DROP TABLE IF EXISTS order_items CASCADE;
DROP TABLE IF EXISTS orders CASCADE;
DROP TABLE IF EXISTS customers;
```

### Notes

- All table data is permanently deleted.
- Indexes on the table are automatically dropped.
- Triggers on the table are automatically dropped.
- Views that depend on the table will cause an error unless CASCADE is used.
- Foreign keys referencing the table will cause an error unless CASCADE is used.
- Requires table ownership or superuser privileges.
- RESTRICT is the default and provides safety against accidental data loss.

---

## TRUNCATE TABLE

### Description

Quickly removes all rows from a table without scanning it. Much faster than DELETE for large tables, but less flexible.

### Syntax

```sql
TRUNCATE [TABLE] <table_name>
    [RESTART IDENTITY | CONTINUE IDENTITY]
    [CASCADE | RESTRICT]
```

### Parameters

- **RESTART IDENTITY**: Resets identity/sequence columns to their start value
- **CONTINUE IDENTITY**: Keeps current identity/sequence values (default)
- **CASCADE**: Truncates tables with foreign keys referencing this table
- **RESTRICT**: Default. Prevents truncate if foreign keys exist

### Examples

**Example 1: Truncate a table**
```sql
TRUNCATE TABLE event_log;
```

**Example 2: Truncate and reset identity column**
```sql
TRUNCATE TABLE customers RESTART IDENTITY;
```

**Example 3: Truncate with continuing identity**
```sql
TRUNCATE TABLE orders CONTINUE IDENTITY;
```

**Example 4: Truncate with foreign key cascade**
```sql
TRUNCATE TABLE customers CASCADE;
```

**Example 5: Prevent truncate if foreign keys exist**
```sql
TRUNCATE TABLE products RESTRICT;
```

### Notes

- TRUNCATE is much faster than DELETE for large tables.
- TRUNCATE cannot be rolled back in some implementations (but is transactional in ScratchBird).
- TRUNCATE does not fire DELETE triggers.
- Cannot use WHERE clause - removes all rows.
- Requires table ownership or TRUNCATE privilege.
- RESTART IDENTITY affects sequences associated with IDENTITY or SERIAL columns.

---

## Constraint Types

### PRIMARY KEY Constraint

#### Description
Uniquely identifies each row in a table. A table can have only one primary key.

#### Syntax
```sql
-- Column constraint
<column_name> <data_type> PRIMARY KEY

-- Table constraint
PRIMARY KEY (<column_name> [, ...])
```

#### Examples
```sql
-- Single column primary key
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username VARCHAR(50)
);

-- Composite primary key
CREATE TABLE user_permissions (
    user_id INTEGER,
    permission_id INTEGER,
    PRIMARY KEY (user_id, permission_id)
);
```

#### Notes
- Primary key columns automatically become NOT NULL.
- Creates a unique index automatically.
- Only one primary key per table.
- Cannot contain NULL values.

### UNIQUE Constraint

#### Description
Ensures all values in a column or set of columns are unique.

#### Syntax
```sql
-- Column constraint
<column_name> <data_type> UNIQUE

-- Table constraint
UNIQUE (<column_name> [, ...])
```

#### Examples
```sql
-- Single column unique
CREATE TABLE employees (
    id INTEGER PRIMARY KEY,
    email VARCHAR(100) UNIQUE
);

-- Composite unique constraint
CREATE TABLE enrollments (
    student_id INTEGER,
    course_id INTEGER,
    UNIQUE (student_id, course_id)
);
```

#### Notes
- Allows NULL values (multiple NULLs are permitted).
- Creates a unique index automatically.
- Can have multiple UNIQUE constraints per table.

### FOREIGN KEY Constraint

#### Description
Enforces referential integrity by ensuring values in a column match values in another table's primary key or unique column.

#### Syntax
```sql
-- Column constraint
<column_name> <data_type> REFERENCES <table>(<column>)
    [ON DELETE {CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION}]
    [ON UPDATE {CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION}]

-- Table constraint
FOREIGN KEY (<column> [, ...]) REFERENCES <table> (<column> [, ...])
    [ON DELETE {CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION}]
    [ON UPDATE {CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION}]
```

#### Examples
```sql
-- Simple foreign key
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(id)
);

-- Foreign key with cascade delete
CREATE TABLE order_items (
    id INTEGER PRIMARY KEY,
    order_id INTEGER REFERENCES orders(id) ON DELETE CASCADE
);

-- Composite foreign key
CREATE TABLE course_assignments (
    assignment_id INTEGER,
    student_id INTEGER,
    course_id INTEGER,
    FOREIGN KEY (student_id, course_id) REFERENCES enrollments(student_id, course_id)
);
```

#### Notes
- Referenced column must be a primary key or have a unique constraint.
- ON DELETE CASCADE automatically deletes dependent rows.
- ON DELETE SET NULL sets foreign key to NULL when referenced row is deleted.
- RESTRICT (default) prevents deletion if dependent rows exist.

### CHECK Constraint

#### Description
Ensures column values satisfy a boolean expression.

#### Syntax
```sql
-- Column constraint
<column_name> <data_type> CHECK (<expression>)

-- Table constraint
CHECK (<expression>)
```

#### Examples
```sql
-- Simple check constraint
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    price DECIMAL(10, 2) CHECK (price > 0)
);

-- Complex check constraint
CREATE TABLE employees (
    id INTEGER PRIMARY KEY,
    hire_date DATE,
    termination_date DATE,
    CHECK (termination_date IS NULL OR termination_date > hire_date)
);

-- Check with multiple columns
CREATE TABLE discounts (
    min_quantity INTEGER,
    max_quantity INTEGER,
    CHECK (max_quantity > min_quantity)
);
```

#### Notes
- Evaluated during INSERT and UPDATE operations.
- Can reference multiple columns in table-level constraints.
- Expression must return boolean value.
- NULL values typically satisfy check constraints.

### NOT NULL Constraint

#### Description
Ensures a column cannot contain NULL values.

#### Syntax
```sql
<column_name> <data_type> NOT NULL
```

#### Examples
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username VARCHAR(50) NOT NULL,
    email VARCHAR(100) NOT NULL,
    phone VARCHAR(20)  -- NULL allowed
);
```

#### Notes
- Primary key columns are automatically NOT NULL.
- Can be added or removed with ALTER TABLE.
- Prevents INSERT/UPDATE with NULL values.

### DEFAULT Constraint

#### Description
Provides a default value for a column when no value is specified.

#### Syntax
```sql
<column_name> <data_type> DEFAULT <expression>
```

#### Examples
```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(20) DEFAULT 'pending',
    quantity INTEGER DEFAULT 1,
    discount DECIMAL(5, 2) DEFAULT 0.00
);
```

#### Notes
- Default value used when column is omitted in INSERT.
- Can be expressions or literals.
- Can use functions like CURRENT_TIMESTAMP, CURRENT_DATE.
- Can be changed with ALTER TABLE.

---

## Known Limitations

### Partial Implementation

**TEMPORARY Tables:**
- TEMPORARY flag is parsed but not enforced end-to-end
- Tables created with TEMPORARY keyword become permanent tables
- Session-scoped cleanup does not occur
- Spec reference: `/docs/specifications/TEMPORARY_TABLES_SPECIFICATION.md`

**UNLOGGED Tables:**
- UNLOGGED flag is parsed but not enforced
- All tables are logged regardless of UNLOGGED keyword
- No performance benefit from UNLOGGED currently

**Table Options:**
- Storage parameters (TOAST, compression) are spec-defined but not fully wired in V2
- CREATE TABLE ... TABLESPACE is parsed but ignored in bytecode
- ALTER TABLE ... SET TABLESPACE is implemented but requires an existing tablespace
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

### Stubbed Features

**CREATE TABLE AS SELECT (CTAS):**
- Not parsed in V2
- Spec defines syntax: `CREATE TABLE <name> AS SELECT ... [WITH [NO] DATA]`
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

**CREATE TABLE LIKE:**
- Not parsed in V2
- Spec defines syntax: `CREATE TABLE <name> LIKE <source_table>`
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

**Table Inheritance:**
- INHERITS clause not supported
- PostgreSQL-style table inheritance not implemented

**Table Partitioning:**
- PARTITION BY clause not parsed
- Range, list, and hash partitioning not supported
- Spec reference: `/docs/specifications/ddl/DDL_TABLE_PARTITIONING.md`

### Missing Features

**ALTER TABLE Subcommands:**
- Not all spec-defined ALTER TABLE operations are implemented
- Missing operations include:
  - ALTER COLUMN SET STATISTICS
  - ALTER COLUMN SET STORAGE
  - ENABLE/DISABLE TRIGGER
  - INHERIT/NO INHERIT
  - ADD/DROP PARTITION
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

**Constraint Features:**
- DEFERRABLE and INITIALLY DEFERRED: Implemented. SET CONSTRAINTS ALL/name DEFERRED/IMMEDIATE works.
- Named constraints: Fully supported in parser and catalog
- Constraint validation (VALIDATE CONSTRAINT): Not implemented
- PK/UNIQUE/FK/CHECK/NOT NULL enforcement: Fully implemented on INSERT/UPDATE/DELETE
- FK referential actions (CASCADE/RESTRICT/SET NULL/SET DEFAULT): Fully implemented

**Identity Column Options:**
- Full IDENTITY column options not completely wired
- GENERATED ALWAYS vs BY DEFAULT may not be fully enforced
- Sequence options for IDENTITY columns limited

### Spec Deltas

**DROP TABLE CASCADE:**
- Executor uses conservative RESTRICT policy for some dependencies
- CASCADE behavior may not fully match specification
- Spec reference: `/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`

**TRUNCATE TABLE:**
- CASCADE semantics need verification against spec
- Behavior with foreign keys may differ from specification
- Spec reference: `/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`

**Constraint Enforcement:**
- Check constraint evaluation may not match all edge cases in spec
- Foreign key ON UPDATE/ON DELETE actions need full validation
- Spec reference: `/docs/specifications/ddl/DDL_TABLES.md`

### General Notes

- All table DDL operations are fully transactional
- Tables are persisted using ScratchBird's Multi-Generational Architecture (MGA)
- Constraint violations produce runtime errors
- Full implementation status in `/docs/audit/parsers/V2/SUMMARY.md`
- Critical findings in `/docs/audit/parsers/CRITICAL_FINDINGS.md`
