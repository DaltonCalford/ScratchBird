<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# ALTER TABLE

[Prev](./01_create_table.md) | [Next](./03_drop_table.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

Modifies the structure of an existing table - add/remove columns, constraints, change defaults, and more.

## Syntax

```sql
ALTER TABLE [ IF EXISTS ] [ ONLY ] table_name [ * ]
    action [, ... ]

where action is one of:
    -- Column operations
    ADD [ COLUMN ] [ IF NOT EXISTS ] column_name data_type [ column_constraint [ ... ] ]
    DROP [ COLUMN ] [ IF EXISTS ] column_name [ RESTRICT | CASCADE ]
    ALTER [ COLUMN ] column_name [ SET DATA ] TYPE data_type [ USING expression ]
    ALTER [ COLUMN ] column_name SET DEFAULT expression
    ALTER [ COLUMN ] column_name DROP DEFAULT
    ALTER [ COLUMN ] column_name { SET | DROP } NOT NULL
    ALTER [ COLUMN ] column_name SET STATISTICS integer
    ALTER [ COLUMN ] column_name SET ( attribute_option = value [, ... ] )
    ALTER [ COLUMN ] column_name RESET ( attribute_option [, ... ] )
    ALTER [ COLUMN ] column_name SET STORAGE { PLAIN | EXTERNAL | EXTENDED | MAIN }
    ALTER [ COLUMN ] column_name SET COMPRESSION compression_method
    
    -- Constraint operations
    ADD table_constraint [ NOT VALID ]
    ADD table_constraint_using_index
    ALTER CONSTRAINT constraint_name [ DEFERRABLE | NOT DEFERRABLE ] 
        [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]
    VALIDATE CONSTRAINT constraint_name
    DROP CONSTRAINT [ IF EXISTS ] constraint_name [ RESTRICT | CASCADE ]
    
    -- Index operations
    SET WITHOUT OIDS
    SET WITH OIDS
    
    -- Storage parameters
    SET ( storage_parameter [= value] [, ... ] )
    RESET ( storage_parameter [, ... ] )
    
    -- Partitioning
    ATTACH PARTITION partition_name { FOR VALUES partition_bound_spec | DEFAULT }
    DETACH PARTITION partition_name [ CONCURRENTLY | FINALIZE ]
    
    -- Inheritance
    INHERIT parent_table
    NO INHERIT parent_table
    
    -- Access method
    SET ACCESS METHOD new_access_method
    
    -- Tablespace
    SET TABLESPACE new_tablespace [ NOWAIT ]
    
    -- Logging
    SET LOGGED
    SET UNLOGGED
    
    -- Identity
    ALTER [ COLUMN ] column_name ADD GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY [ ( sequence_options ) ]
    ALTER [ COLUMN ] column_name { SET GENERATED { ALWAYS | BY DEFAULT } | SET sequence_option | RESTART [ [ WITH ] restart ] }
    ALTER [ COLUMN ] column_name DROP IDENTITY [ IF EXISTS ]
    
    -- Statistics
    ADD STATISTICS statistics_name
    DROP STATISTICS statistics_name

and table_constraint_using_index is:
    [ CONSTRAINT constraint_name ]
    { UNIQUE | PRIMARY KEY } USING INDEX index_name
    [ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]
```

## Column Operations

### ADD COLUMN

Add a new column to the table.

```sql
-- Basic add
ALTER TABLE users ADD COLUMN phone TEXT;

-- With constraints
ALTER TABLE users ADD COLUMN email_verified BOOLEAN NOT NULL DEFAULT FALSE;

-- IF NOT EXISTS
ALTER TABLE users ADD COLUMN IF NOT EXISTS profile_image TEXT;

-- Multiple columns
ALTER TABLE orders 
    ADD COLUMN shipping_address TEXT,
    ADD COLUMN billing_address TEXT;
```

**Notes:**
- Existing rows get NULL (or specified DEFAULT)
- Adding NOT NULL requires DEFAULT or table empty
- In MGA: adds column to new version schema

### DROP COLUMN

Remove a column from the table.

```sql
-- Basic drop
ALTER TABLE users DROP COLUMN temp_field;

-- IF EXISTS
ALTER TABLE users DROP COLUMN IF EXISTS obsolete_field;

-- CASCADE to drop dependent objects
ALTER TABLE users DROP COLUMN email CASCADE;  -- Drops index on email
```

### ALTER COLUMN TYPE

Change the data type of a column.

```sql
-- Simple type change
ALTER TABLE products ALTER COLUMN price TYPE DECIMAL(12,2);

-- With USING for conversion
ALTER TABLE events ALTER COLUMN event_time TYPE TIMESTAMPTZ 
    USING event_time AT TIME ZONE 'UTC';

-- Text to integer
ALTER TABLE orders ALTER COLUMN status_code TYPE INTEGER 
    USING status_code::INTEGER;
```

### SET/DROP DEFAULT

Manage column defaults.

```sql
-- Set default
ALTER TABLE users ALTER COLUMN status SET DEFAULT 'active';

-- Drop default
ALTER TABLE users ALTER COLUMN status DROP DEFAULT;

-- Change default
ALTER TABLE orders ALTER COLUMN created_at SET DEFAULT NOW();
```

### SET/DROP NOT NULL

Control nullability.

```sql
-- Add NOT NULL (requires no NULLs exist)
ALTER TABLE users ALTER COLUMN email SET NOT NULL;

-- Remove NOT NULL
ALTER TABLE users ALTER COLUMN phone DROP NOT NULL;
```

## Constraint Operations

### ADD CONSTRAINT

Add a new constraint to the table.

```sql
-- Primary key
ALTER TABLE users ADD CONSTRAINT pk_users PRIMARY KEY (id);

-- Unique
ALTER TABLE users ADD CONSTRAINT uq_email UNIQUE (email);

-- Check
ALTER TABLE products ADD CONSTRAINT chk_price_positive 
    CHECK (price > 0);

-- Foreign key
ALTER TABLE orders ADD CONSTRAINT fk_orders_user 
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE;

-- Exclude
ALTER TABLE reservations ADD CONSTRAINT no_overlapping 
    EXCLUDE USING GIST (room_id WITH =, duration WITH &&);
```

### NOT VALID Option

Add constraint without checking existing data (check later with VALIDATE).

```sql
-- Add constraint not valid (fast)
ALTER TABLE orders ADD CONSTRAINT chk_amount_positive 
    CHECK (amount > 0) NOT VALID;

-- Validate later (scans table)
ALTER TABLE orders VALIDATE CONSTRAINT chk_amount_positive;
```

### DROP CONSTRAINT

Remove a constraint.

```sql
-- Drop named constraint
ALTER TABLE users DROP CONSTRAINT uq_email;

-- IF EXISTS
ALTER TABLE users DROP CONSTRAINT IF EXISTS old_constraint;

-- CASCADE for dependent constraints
ALTER TABLE users DROP CONSTRAINT pk_users CASCADE;
```

## Table Operations

### RENAME

Rename table, column, or constraint.

```sql
-- Rename table
ALTER TABLE old_users RENAME TO users;

-- Rename column
ALTER TABLE users RENAME COLUMN email_addr TO email;

-- Rename constraint
ALTER TABLE users RENAME CONSTRAINT uq_email TO users_email_unique;
```

### SET TABLESPACE

Move table to different storage.

```sql
-- Move to fast SSD
ALTER TABLE active_orders SET TABLESPACE fast_ssd;

-- Move with no wait
ALTER TABLE large_table SET TABLESPACE archive NOWAIT;
```

### SET LOGGED/UNLOGGED

Change logging behavior.

```sql
-- Make table crash-safe
ALTER TABLE temp_results SET LOGGED;

-- Make table faster (no WAL)
ALTER TABLE cache_data SET UNLOGGED;
```

### SET STORAGE

Control TOAST storage for a column.

```sql
-- Force external storage (no compression)
ALTER TABLE documents ALTER COLUMN content SET STORAGE EXTERNAL;

-- Allow compression
ALTER TABLE documents ALTER COLUMN content SET STORAGE EXTENDED;
```

## Partitioning Operations

### ATTACH PARTITION

Add a partition to a partitioned table.

```sql
-- Range partition
ALTER TABLE events ATTACH PARTITION events_2024q1 
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

-- List partition
ALTER TABLE regions ATTACH PARTITION regions_west 
    FOR VALUES IN ('CA', 'OR', 'WA');

-- Default partition
ALTER TABLE events ATTACH PARTITION events_default DEFAULT;
```

### DETACH PARTITION

Remove a partition.

```sql
-- Detach (acquires lock)
ALTER TABLE events DETACH PARTITION events_2023q4;

-- Concurrent detach (online)
ALTER TABLE events DETACH PARTITION events_2023q4 CONCURRENTLY;

-- Finalize after concurrent
ALTER TABLE events DETACH PARTITION events_2023q4 FINALIZE;
```

## Inheritance Operations

### INHERIT/NO INHERIT

Manage table inheritance.

```sql
-- Make table inherit from parent
ALTER TABLE customers INHERIT persons;

-- Remove inheritance
ALTER TABLE customers NO INHERIT persons;
```

## Identity Column Operations

### ADD IDENTITY

Add identity to existing column.

```sql
-- Add identity
ALTER TABLE orders ALTER COLUMN id 
    ADD GENERATED ALWAYS AS IDENTITY (START WITH 1000);

-- Add identity by default
ALTER TABLE orders ALTER COLUMN id 
    ADD GENERATED BY DEFAULT AS IDENTITY;
```

### ALTER IDENTITY

Modify identity properties.

```sql
-- Change generation type
ALTER TABLE orders ALTER COLUMN id 
    SET GENERATED BY DEFAULT;

-- Restart sequence
ALTER TABLE orders ALTER COLUMN id RESTART WITH 5000;
```

### DROP IDENTITY

Remove identity from column.

```sql
ALTER TABLE orders ALTER COLUMN id DROP IDENTITY;

-- IF EXISTS
ALTER TABLE orders ALTER COLUMN id DROP IDENTITY IF EXISTS;
```

## Examples by Use Case

### Schema Evolution

```sql
-- Add new columns for feature
ALTER TABLE users 
    ADD COLUMN phone TEXT,
    ADD COLUMN preferences JSONB DEFAULT '{}';

-- Add constraints after data migration
ALTER TABLE users 
    ADD CONSTRAINT uq_phone UNIQUE (phone),
    VALIDATE CONSTRAINT uq_phone;

-- Drop old columns
ALTER TABLE users 
    DROP COLUMN legacy_field CASCADE;
```

### Performance Optimization

```sql
-- Move hot table to fast storage
ALTER TABLE active_sessions SET TABLESPACE ssd_tablespace;

-- Adjust fillfactor for update-heavy table
ALTER TABLE user_sessions SET (fillfactor = 70);

-- Add statistics for better query plans
ALTER TABLE orders ALTER COLUMN status SET STATISTICS 1000;
```

### Partitioning Maintenance

```sql
-- Create new partition
CREATE TABLE events_2024q2 (LIKE events INCLUDING ALL);

-- Attach partition
ALTER TABLE events ATTACH PARTITION events_2024q2 
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');

-- Detach old partition for archival
ALTER TABLE events DETACH PARTITION events_2023q1;
```

## Parser Acceptance Cases

```sql
ALTER TABLE t1 ADD COLUMN c1 INT;
ALTER TABLE t1 DROP COLUMN c1;
ALTER TABLE t1 ALTER COLUMN c1 TYPE TEXT;
ALTER TABLE t1 ALTER COLUMN c1 SET DEFAULT 0;
ALTER TABLE t1 ADD CONSTRAINT pk PRIMARY KEY (id);
ALTER TABLE t1 DROP CONSTRAINT IF EXISTS pk;
ALTER TABLE t1 RENAME TO t2;
ALTER TABLE t1 SET TABLESPACE ts1;
```

## Parser Rejection Cases

```sql
-- Column doesn't exist
ALTER TABLE t1 DROP COLUMN nonexistent;  -- Error

-- Duplicate column
ALTER TABLE t1 ADD COLUMN c1 INT, ADD COLUMN c1 INT;  -- Error

-- Type change loses data
ALTER TABLE t1 ALTER COLUMN c1 TYPE INT USING 'invalid';  -- Error
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `undefined_table` | Table doesn't exist (no IF EXISTS) |
| `undefined_column` | Column doesn't exist |
| `duplicate_column` | Column already exists |
| `constraint_violation` | Existing data violates new constraint |

## Completion Checklist

- [x] Canonical forms documented
- [x] Clause matrix completed
- [x] Positive and negative parser cases listed
- [x] Examples validated against v3 parser behavior
- [x] Error conditions documented
- [x] Cross-references added

## See Also

- [CREATE TABLE](01_create_table.md)
- [DROP TABLE](03_drop_table.md)
- [Constraints](01_create_table.md#constraints)
- [Partitioning](../../cluster_and_distribution/README.md)
