[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL Tables and Constraints

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

**PostgreSQL Emulation Layer - Table DDL Reference**

This document covers PostgreSQL-compatible table and constraint operations in ScratchBird's PostgreSQL emulation layer.

---

## Overview

The PostgreSQL emulation layer provides compatibility with PostgreSQL 16 syntax for table definition and management. Tables are the fundamental structure for storing data, and constraints ensure data integrity.

**Key Points:**
- CREATE TABLE syntax is PostgreSQL-compatible but execution is currently stubbed
- Parser supports full PostgreSQL table syntax including constraints, TEMP, UNLOGGED
- Bytecode format mismatch prevents execution (see Known Limitations)
- This documentation describes the intended syntax when implementation is completed

---

## CREATE TABLE

Creates a new table in the database.

### Description

`CREATE TABLE` defines a new table with columns, data types, and constraints. PostgreSQL's CREATE TABLE supports many advanced features including temporary tables, unlogged tables, inheritance, and partitioning.

### Syntax

```sql
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE [ IF NOT EXISTS ] table_name
    ( [ { column_definition | table_constraint | LIKE source_table [ like_option ... ] }
        [, ... ]
    ] )
    [ INHERITS ( parent_table [, ... ] ) ]
    [ PARTITION BY { RANGE | LIST | HASH } ( { column_name | ( expression ) } [ opclass ] [, ... ] ) ]
    [ USING method ]
    [ WITH ( storage_parameter [= value] [, ... ] ) | WITHOUT OIDS ]
    [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
    [ TABLESPACE tablespace_name ]

column_definition:
    column_name data_type [ COLLATE collation ]
    [ column_constraint [ ... ] ]

column_constraint:
    [ CONSTRAINT constraint_name ]
    { NOT NULL |
      NULL |
      CHECK ( expression ) [ NO INHERIT ] |
      DEFAULT default_expr |
      GENERATED ALWAYS AS ( generation_expr ) STORED |
      GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY [ ( sequence_options ) ] |
      UNIQUE [ NULLS [ NOT ] DISTINCT ] |
      PRIMARY KEY |
      REFERENCES reftable [ ( refcolumn ) ]
        [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ]
        [ ON DELETE referential_action ]
        [ ON UPDATE referential_action ] }
    [ DEFERRABLE | NOT DEFERRABLE ]
    [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]

table_constraint:
    [ CONSTRAINT constraint_name ]
    { CHECK ( expression ) [ NO INHERIT ] |
      UNIQUE [ NULLS [ NOT ] DISTINCT ] ( column_name [, ... ] ) |
      PRIMARY KEY ( column_name [, ... ] ) |
      EXCLUDE [ USING index_method ] ( exclude_element WITH operator [, ... ] )
        [ WHERE ( predicate ) ] |
      FOREIGN KEY ( column_name [, ... ] )
        REFERENCES reftable [ ( refcolumn [, ... ] ) ]
        [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ]
        [ ON DELETE referential_action ]
        [ ON UPDATE referential_action ] }
    [ DEFERRABLE | NOT DEFERRABLE ]
    [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]

referential_action:
    NO ACTION | RESTRICT | CASCADE | SET NULL | SET DEFAULT
```

### Parameters

- **TEMPORARY / TEMP** - Create a temporary table (session or transaction scoped)
- **UNLOGGED** - Data is not written to WAL (faster but not crash-safe)
- **IF NOT EXISTS** - Do not throw error if table already exists
- **table_name** - Name of the table to create
- **column_name** - Name of a column
- **data_type** - Data type for the column
- **COLLATE** - Collation for text columns
- **NOT NULL** - Column cannot contain NULL values
- **DEFAULT** - Default value for the column
- **GENERATED ALWAYS AS** - Computed column (stored)
- **GENERATED AS IDENTITY** - Auto-incrementing column (SQL standard alternative to SERIAL)
- **UNIQUE** - Column or columns must contain unique values
- **PRIMARY KEY** - Designates primary key column(s)
- **REFERENCES** - Foreign key constraint
- **CHECK** - Boolean expression that values must satisfy
- **DEFERRABLE** - Constraint can be deferred until transaction commit
- **INHERITS** - Inherit columns from parent table(s)
- **PARTITION BY** - Create a partitioned table
- **WITH** - Storage parameters
- **ON COMMIT** - Behavior of temporary table at transaction commit
- **TABLESPACE** - Tablespace for the table

### Examples

**Basic table creation:**
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    name TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

**Table with IF NOT EXISTS:**
```sql
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    email TEXT UNIQUE
);
```

**Table with various constraints:**
```sql
CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES users(id),
    total NUMERIC(10,2) CHECK (total >= 0),
    status TEXT CHECK (status IN ('pending', 'shipped', 'delivered')),
    order_date DATE NOT NULL DEFAULT CURRENT_DATE,
    CONSTRAINT positive_total CHECK (total > 0)
);
```

**Temporary table:**
```sql
CREATE TEMP TABLE session_data (
    session_id TEXT PRIMARY KEY,
    data JSONB,
    expires_at TIMESTAMP
) ON COMMIT DELETE ROWS;
```

**Unlogged table (high performance):**
```sql
CREATE UNLOGGED TABLE logs (
    log_id BIGSERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT NOW()
);
```

**Table with generated column:**
```sql
CREATE TABLE products (
    product_id SERIAL PRIMARY KEY,
    name TEXT,
    price NUMERIC(10,2),
    tax_rate NUMERIC(3,2) DEFAULT 0.08,
    price_with_tax NUMERIC(10,2) GENERATED ALWAYS AS (price * (1 + tax_rate)) STORED
);
```

**Table with identity column:**
```sql
CREATE TABLE customers (
    customer_id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name TEXT NOT NULL,
    email TEXT UNIQUE
);

-- Alternative with BY DEFAULT (allows manual insertion)
CREATE TABLE invoices (
    invoice_id INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
    customer_id INTEGER REFERENCES customers,
    total NUMERIC(10,2)
);
```

**Table with composite primary key:**
```sql
CREATE TABLE order_items (
    order_id INTEGER REFERENCES orders(order_id),
    product_id INTEGER REFERENCES products(product_id),
    quantity INTEGER NOT NULL CHECK (quantity > 0),
    unit_price NUMERIC(10,2) NOT NULL,
    PRIMARY KEY (order_id, product_id)
);
```

**Table with deferrable constraint:**
```sql
CREATE TABLE employees (
    emp_id INTEGER PRIMARY KEY,
    manager_id INTEGER,
    name TEXT NOT NULL,
    CONSTRAINT fk_manager
        FOREIGN KEY (manager_id) REFERENCES employees(emp_id)
        DEFERRABLE INITIALLY DEFERRED
);
```

**Table with CHECK constraint (NO INHERIT):**
```sql
CREATE TABLE base_table (
    id INTEGER PRIMARY KEY,
    status TEXT CHECK (status IN ('active', 'inactive')) NO INHERIT
);
```

**Table with LIKE:**
```sql
CREATE TABLE users_archive (
    LIKE users INCLUDING ALL,
    archived_at TIMESTAMP DEFAULT NOW()
);
```

### Table Inheritance (PostgreSQL-Specific)

**Create child table inheriting from parent:**
```sql
CREATE TABLE cities (
    name TEXT,
    population REAL,
    elevation INTEGER
);

CREATE TABLE capitals (
    state CHAR(2)
) INHERITS (cities);
```

**Query including children:**
```sql
SELECT name, elevation FROM cities;  -- includes capitals
```

**Query only parent:**
```sql
SELECT name, elevation FROM ONLY cities;  -- excludes capitals
```

### Table Partitioning

**Range partitioning by date:**
```sql
CREATE TABLE measurement (
    logdate DATE NOT NULL,
    peaktemp INTEGER,
    unitsales INTEGER
) PARTITION BY RANGE (logdate);

CREATE TABLE measurement_y2023 PARTITION OF measurement
    FOR VALUES FROM ('2023-01-01') TO ('2024-01-01');

CREATE TABLE measurement_y2024 PARTITION OF measurement
    FOR VALUES FROM ('2024-01-01') TO ('2025-01-01');
```

**List partitioning by region:**
```sql
CREATE TABLE sales (
    sale_id INTEGER,
    region TEXT,
    amount NUMERIC
) PARTITION BY LIST (region);

CREATE TABLE sales_north PARTITION OF sales
    FOR VALUES IN ('north', 'northeast', 'northwest');

CREATE TABLE sales_south PARTITION OF sales
    FOR VALUES IN ('south', 'southeast', 'southwest');
```

**Hash partitioning for load distribution:**
```sql
CREATE TABLE events (
    event_id BIGINT,
    event_data JSONB,
    created_at TIMESTAMP
) PARTITION BY HASH (event_id);

CREATE TABLE events_p0 PARTITION OF events
    FOR VALUES WITH (MODULUS 4, REMAINDER 0);

CREATE TABLE events_p1 PARTITION OF events
    FOR VALUES WITH (MODULUS 4, REMAINDER 1);

CREATE TABLE events_p2 PARTITION OF events
    FOR VALUES WITH (MODULUS 4, REMAINDER 2);

CREATE TABLE events_p3 PARTITION OF events
    FOR VALUES WITH (MODULUS 4, REMAINDER 3);
```

### Notes

- Table names must be unique within a schema
- Column names must be unique within a table
- PRIMARY KEY implies NOT NULL and UNIQUE
- SERIAL and BIGSERIAL are convenience types that create sequences
- GENERATED AS IDENTITY is the SQL standard way to create auto-incrementing columns
- Temporary tables are automatically dropped at session end (or transaction end with ON COMMIT DROP)
- Unlogged tables are faster but data is lost on crash
- Inheritance allows table hierarchies (PostgreSQL-specific feature)
- Partitioning improves query performance on large tables

### Related Statements

- [ALTER TABLE](#alter-table)
- [DROP TABLE](#drop-table)
- [TRUNCATE TABLE](#truncate-table)
- [CREATE INDEX](03_indexes_views_sequences.md)

---

## ALTER TABLE

Modifies the structure of an existing table.

### Description

`ALTER TABLE` allows you to change table definitions by adding/dropping/modifying columns, constraints, and other table properties.

### Syntax

```sql
ALTER TABLE [ IF EXISTS ] [ ONLY ] table_name [ * ]
    alter_action [, ... ]

alter_action:
    ADD [ COLUMN ] [ IF NOT EXISTS ] column_name data_type [ column_constraint [ ... ] ]
  | DROP [ COLUMN ] [ IF EXISTS ] column_name [ RESTRICT | CASCADE ]
  | ALTER [ COLUMN ] column_name { SET DEFAULT expression | DROP DEFAULT }
  | ALTER [ COLUMN ] column_name { SET | DROP } NOT NULL
  | ALTER [ COLUMN ] column_name SET DATA TYPE data_type [ USING expression ]
  | ADD table_constraint [ NOT VALID ]
  | DROP CONSTRAINT [ IF EXISTS ] constraint_name [ RESTRICT | CASCADE ]
  | VALIDATE CONSTRAINT constraint_name
  | DISABLE TRIGGER [ trigger_name | ALL | USER ]
  | ENABLE TRIGGER [ trigger_name | ALL | USER ]
  | RENAME [ COLUMN ] column_name TO new_column_name
  | RENAME TO new_table_name
  | SET SCHEMA new_schema
  | OWNER TO new_owner
  | INHERIT parent_table
  | NO INHERIT parent_table
```

### Parameters

- **IF EXISTS** - Do not error if table doesn't exist
- **ONLY** - Apply only to specified table, not child tables
- **ADD COLUMN** - Add a new column to the table
- **DROP COLUMN** - Remove a column from the table
- **ALTER COLUMN** - Modify column properties
- **SET DEFAULT** - Set a default value for a column
- **DROP DEFAULT** - Remove the default value
- **SET NOT NULL** - Add NOT NULL constraint
- **DROP NOT NULL** - Remove NOT NULL constraint
- **SET DATA TYPE** - Change the data type of a column
- **ADD constraint** - Add a new table constraint
- **DROP CONSTRAINT** - Remove a constraint
- **VALIDATE CONSTRAINT** - Validate a constraint previously added with NOT VALID
- **RENAME** - Rename column or table
- **SET SCHEMA** - Move table to different schema
- **OWNER TO** - Change table owner
- **INHERIT / NO INHERIT** - Add/remove inheritance relationship

### Examples

**Add a column:**
```sql
ALTER TABLE users ADD COLUMN phone TEXT;
```

**Add a column with NOT NULL and default:**
```sql
ALTER TABLE users ADD COLUMN active BOOLEAN NOT NULL DEFAULT true;
```

**Add column with IF NOT EXISTS:**
```sql
ALTER TABLE users ADD COLUMN IF NOT EXISTS middle_name TEXT;
```

**Drop a column:**
```sql
ALTER TABLE users DROP COLUMN phone;
```

**Drop column with CASCADE (drops dependent objects):**
```sql
ALTER TABLE users DROP COLUMN email CASCADE;
```

**Change column data type:**
```sql
ALTER TABLE users ALTER COLUMN age SET DATA TYPE BIGINT;
```

**Change column type with USING (for conversion):**
```sql
ALTER TABLE users
    ALTER COLUMN age SET DATA TYPE TEXT
    USING age::TEXT;
```

**Set default value:**
```sql
ALTER TABLE users ALTER COLUMN status SET DEFAULT 'active';
```

**Drop default value:**
```sql
ALTER TABLE users ALTER COLUMN status DROP DEFAULT;
```

**Add NOT NULL constraint:**
```sql
ALTER TABLE users ALTER COLUMN email SET NOT NULL;
```

**Drop NOT NULL constraint:**
```sql
ALTER TABLE users ALTER COLUMN middle_name DROP NOT NULL;
```

**Add primary key:**
```sql
ALTER TABLE users ADD PRIMARY KEY (id);
```

**Add unique constraint:**
```sql
ALTER TABLE users ADD CONSTRAINT users_email_unique UNIQUE (email);
```

**Add foreign key:**
```sql
ALTER TABLE orders
    ADD CONSTRAINT fk_user
    FOREIGN KEY (user_id)
    REFERENCES users(id)
    ON DELETE CASCADE;
```

**Add CHECK constraint:**
```sql
ALTER TABLE products
    ADD CONSTRAINT positive_price
    CHECK (price > 0);
```

**Add constraint with NOT VALID (doesn't check existing rows):**
```sql
ALTER TABLE large_table
    ADD CONSTRAINT check_status
    CHECK (status IN ('active', 'inactive'))
    NOT VALID;

-- Later, validate existing rows
ALTER TABLE large_table VALIDATE CONSTRAINT check_status;
```

**Drop a constraint:**
```sql
ALTER TABLE users DROP CONSTRAINT users_email_unique;
```

**Rename a column:**
```sql
ALTER TABLE users RENAME COLUMN name TO full_name;
```

**Rename a table:**
```sql
ALTER TABLE users RENAME TO customers;
```

**Move table to different schema:**
```sql
ALTER TABLE users SET SCHEMA archive;
```

**Change table owner:**
```sql
ALTER TABLE users OWNER TO new_owner;
```

**Add inheritance:**
```sql
ALTER TABLE child_table INHERIT parent_table;
```

**Remove inheritance:**
```sql
ALTER TABLE child_table NO INHERIT parent_table;
```

**Multiple alterations in one statement:**
```sql
ALTER TABLE users
    ADD COLUMN created_at TIMESTAMP DEFAULT NOW(),
    ADD COLUMN updated_at TIMESTAMP,
    ALTER COLUMN email SET NOT NULL;
```

### Notes

- Some ALTER TABLE operations require a table rewrite (e.g., changing column type)
- Use CASCADE carefully as it may drop dependent objects
- NOT VALID constraints can speed up adding constraints to large tables
- Adding NOT NULL requires that all existing rows already have non-null values
- Multiple alterations in one statement are more efficient than separate statements

### Related Statements

- [CREATE TABLE](#create-table)
- [DROP TABLE](#drop-table)

---

## DROP TABLE

Removes one or more tables from the database.

### Description

`DROP TABLE` permanently deletes a table and all its data. Use CASCADE to also drop dependent objects.

### Syntax

```sql
DROP TABLE [ IF EXISTS ] table_name [, ...] [ CASCADE | RESTRICT ]
```

### Parameters

- **IF EXISTS** - Do not error if table doesn't exist
- **table_name** - Name of the table(s) to drop
- **CASCADE** - Automatically drop objects that depend on the table (views, foreign keys, etc.)
- **RESTRICT** - Refuse to drop if any objects depend on it (default)

### Examples

**Drop a single table:**
```sql
DROP TABLE users;
```

**Drop with IF EXISTS:**
```sql
DROP TABLE IF EXISTS temporary_data;
```

**Drop multiple tables:**
```sql
DROP TABLE users, orders, products;
```

**Drop with CASCADE:**
```sql
DROP TABLE users CASCADE;
```

**Drop temporary table:**
```sql
DROP TABLE IF EXISTS temp_calculations;
```

### Notes

- This operation is irreversible - all table data is permanently deleted
- RESTRICT is the default behavior
- CASCADE will drop views, foreign key constraints, and other dependent objects
- IF EXISTS is useful in scripts that may run multiple times
- Dropping a table automatically drops its indexes

### Related Statements

- [CREATE TABLE](#create-table)
- [ALTER TABLE](#alter-table)
- [TRUNCATE TABLE](#truncate-table)

---

## TRUNCATE TABLE

Quickly removes all rows from a table.

### Description

`TRUNCATE TABLE` efficiently removes all rows from one or more tables. It's faster than DELETE because it doesn't scan the table, but it cannot be rolled back in the same way.

### Syntax

```sql
TRUNCATE [ TABLE ] table_name [, ...]
    [ RESTART IDENTITY | CONTINUE IDENTITY ]
    [ CASCADE | RESTRICT ]
```

### Parameters

- **table_name** - Name of the table(s) to truncate
- **RESTART IDENTITY** - Reset sequences owned by columns (e.g., SERIAL columns) to starting values
- **CONTINUE IDENTITY** - Do not reset sequences (default)
- **CASCADE** - Automatically truncate tables with foreign keys referencing this table
- **RESTRICT** - Refuse if other tables have foreign key references (default)

### Examples

**Truncate a table:**
```sql
TRUNCATE TABLE logs;
```

**Truncate and reset identity columns:**
```sql
TRUNCATE TABLE users RESTART IDENTITY;
```

**Truncate with CONTINUE IDENTITY (explicit):**
```sql
TRUNCATE TABLE events CONTINUE IDENTITY;
```

**Truncate with CASCADE:**
```sql
TRUNCATE TABLE users CASCADE;
```

**Truncate multiple tables:**
```sql
TRUNCATE TABLE logs, events, audit_trail RESTART IDENTITY;
```

**Truncate in a transaction:**
```sql
BEGIN;
TRUNCATE TABLE staging_data;
-- Load new data
COPY staging_data FROM '/data/import.csv';
COMMIT;
```

### Notes

- TRUNCATE is much faster than DELETE for large tables
- TRUNCATE cannot be used if the table has foreign key references from other tables (unless CASCADE is used)
- TRUNCATE resets the table to empty but preserves the table structure
- Triggers are not fired for individual rows (table-level triggers may fire)
- TRUNCATE cannot be rolled back in some systems (though PostgreSQL supports it within transactions)
- RESTART IDENTITY resets AUTO_INCREMENT / SERIAL sequences to their starting values

### Related Statements

- [DELETE](07_dml_modification.md#delete)
- [DROP TABLE](#drop-table)

---

## Constraint Types

### PRIMARY KEY

Uniquely identifies each row in a table.

**Column-level:**
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name TEXT
);
```

**Table-level:**
```sql
CREATE TABLE users (
    id INTEGER,
    name TEXT,
    PRIMARY KEY (id)
);
```

**Composite primary key:**
```sql
CREATE TABLE order_items (
    order_id INTEGER,
    product_id INTEGER,
    quantity INTEGER,
    PRIMARY KEY (order_id, product_id)
);
```

### UNIQUE

Ensures all values in a column (or combination of columns) are unique.

**Column-level:**
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email TEXT UNIQUE
);
```

**Table-level:**
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email TEXT,
    UNIQUE (email)
);
```

**Composite unique:**
```sql
CREATE TABLE reservations (
    room_id INTEGER,
    date DATE,
    guest_name TEXT,
    UNIQUE (room_id, date)
);
```

**Named constraint:**
```sql
CREATE TABLE users (
    email TEXT,
    CONSTRAINT users_email_unique UNIQUE (email)
);
```

**NULLS NOT DISTINCT (PostgreSQL 15+):**
```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email TEXT UNIQUE NULLS NOT DISTINCT  -- Multiple NULLs not allowed
);
```

### FOREIGN KEY

Ensures referential integrity between tables.

**Column-level:**
```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES users(id)
);
```

**Table-level:**
```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER,
    FOREIGN KEY (user_id) REFERENCES users(id)
);
```

**With ON DELETE action:**
```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE
);
```

**With ON UPDATE action:**
```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    user_id INTEGER REFERENCES users(id)
        ON UPDATE CASCADE
        ON DELETE SET NULL
);
```

**Composite foreign key:**
```sql
CREATE TABLE order_details (
    order_id INTEGER,
    product_id INTEGER,
    detail_id INTEGER,
    FOREIGN KEY (order_id, product_id)
        REFERENCES order_items(order_id, product_id)
);
```

**Referential actions:**
- `NO ACTION` - Prevent deletion/update if referenced (default)
- `RESTRICT` - Similar to NO ACTION
- `CASCADE` - Delete/update dependent rows
- `SET NULL` - Set foreign key to NULL
- `SET DEFAULT` - Set foreign key to default value

### CHECK

Ensures values satisfy a boolean expression.

**Column-level:**
```sql
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    price NUMERIC CHECK (price > 0)
);
```

**Table-level:**
```sql
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    price NUMERIC,
    discount_price NUMERIC,
    CHECK (discount_price < price)
);
```

**Named CHECK constraint:**
```sql
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    price NUMERIC,
    CONSTRAINT positive_price CHECK (price > 0)
);
```

**Multi-column CHECK:**
```sql
CREATE TABLE events (
    id INTEGER PRIMARY KEY,
    start_date DATE,
    end_date DATE,
    CHECK (end_date >= start_date)
);
```

**Complex CHECK with IN:**
```sql
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    status TEXT CHECK (status IN ('pending', 'processing', 'shipped', 'delivered', 'cancelled'))
);
```

### NOT NULL

Prevents NULL values in a column.

```sql
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email TEXT NOT NULL,
    name TEXT NOT NULL,
    phone TEXT  -- nullable
);
```

### DEFAULT

Provides a default value when none is specified.

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    active BOOLEAN DEFAULT true,
    role TEXT DEFAULT 'user',
    signup_bonus NUMERIC DEFAULT 0.00
);
```

**Using functions:**
```sql
CREATE TABLE events (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMP DEFAULT NOW(),
    data JSONB DEFAULT '{}'::jsonb
);
```

---

## Best Practices

### Table Design

- Choose appropriate data types for columns
- Use PRIMARY KEY on every table
- Add FOREIGN KEY constraints to maintain referential integrity
- Use CHECK constraints to enforce business rules
- Consider indexes for frequently queried columns
- Normalize data to reduce redundancy

### Constraint Naming

Use descriptive names for constraints:
```sql
-- Good
CONSTRAINT users_email_unique UNIQUE (email)
CONSTRAINT orders_user_id_fk FOREIGN KEY (user_id) REFERENCES users(id)
CONSTRAINT products_price_positive CHECK (price > 0)

-- Less clear
CONSTRAINT c1 UNIQUE (email)
CONSTRAINT fk1 FOREIGN KEY (user_id) REFERENCES users(id)
```

### Performance Considerations

- Use UNLOGGED tables for temporary/cache data
- Consider partitioning for very large tables
- Add indexes on foreign key columns
- Use TRUNCATE instead of DELETE for clearing tables
- Batch ALTER TABLE operations when possible

### Data Integrity

- Always use NOT NULL where appropriate
- Add CHECK constraints for data validation
- Use FOREIGN KEY constraints to prevent orphan records
- Consider DEFERRABLE constraints for circular references

---

## Known Limitations

### Stubbed Implementation

🚧 **CREATE TABLE** - Parser accepts full PostgreSQL syntax but executor bytecode format mismatch prevents execution. Parser emits IF NOT EXISTS byte and column/constraint format that executor doesn't expect.

🚧 **ALTER TABLE** - Parser emits legacy ALTER_TABLE format not compatible with current executor implementation.

🚧 **DROP TABLE** - Parser emits TABLE_REF lists and extra flags that don't match executor expectations (expects name string + flags).

🚧 **TRUNCATE TABLE** - Parser emits TABLE_REF list + flags not read by executor.

### Partial Implementation

⚠️ **Table Inheritance** - INHERITS clause is parsed but inheritance behavior may not be fully implemented in executor.

⚠️ **Table Partitioning** - PARTITION BY is parsed but partitioning logic may not be fully implemented.

⚠️ **Storage Parameters** - WITH options are parsed but may not affect actual storage behavior.

⚠️ **Generated Columns** - GENERATED ALWAYS AS is parsed but computed column logic may not be fully implemented.

### Missing Features

❌ **EXCLUDE Constraints** - Parsed but not implemented in executor.

❌ **Tablespaces** - TABLESPACE option is parsed but tablespace management is not implemented.

❌ **ON COMMIT for Temp Tables** - Parsed but behavior may not be fully enforced.

### Spec Deltas

📝 **Bytecode Format** - The PostgreSQL parser emits SBLR bytecode with a different structure than the executor expects. CREATE TABLE includes an extra IF NOT EXISTS byte, and column definitions lack the COLUMN_REF qualifier that the executor expects. This prevents table creation from working end-to-end.

📝 **Constraint Handling** - Table constraints are emitted as inline opcodes (PRIMARY_KEY, UNIQUE_CONSTRAINT, TABLE_FK) within the column list, but executor expects a different format.

---

## See Also

### Related Documentation
- [Indexes, Views, Sequences](03_indexes_views_sequences.md)
- [Types and Domains](04_types_and_domains.md)
- [DML Modification](07_dml_modification.md)

### Specifications
- `/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `/docs/specifications/ddl/DDL_TABLES.md`
- `/docs/audit/17_postgresql_parser_statement_reference_actual.md`

### Source Code
- Parser: `/src/parser/postgresql/pg_parser_ddl.cpp`
- Executor: `/src/sblr/executor.cpp`
