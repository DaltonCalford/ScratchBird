<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE TABLE

[Prev](../database_and_schema/06_drop_schema.md) | [Next](./02_alter_table.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

Creates a new table with specified columns, constraints, and storage parameters.

## Syntax

```sql
CREATE [ [ GLOBAL | LOCAL ] { TEMPORARY | TEMP } | UNLOGGED ] TABLE [ IF NOT EXISTS ] table_name
    ( [
        { column_name data_type [ column_constraint [ ... ] ]
          | table_constraint
          | LIKE source_table [ like_option ... ] }
        [, ... ]
    ] )
    [ INHERITS ( parent_table [, ... ] ) ]
    [ PARTITION BY { RANGE | LIST | HASH } ( partition_column [, ... ] ) ]
    [ WITH ( storage_parameter [= value] [, ... ] ) | WITHOUT OIDS ]
    [ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ]
    [ TABLESPACE tablespace_name ]

where column_constraint is:
    [ CONSTRAINT constraint_name ]
    { NOT NULL |
      NULL |
      CHECK ( expression ) [ NO INHERIT ] |
      DEFAULT default_expr |
      GENERATED ALWAYS AS ( generation_expr ) STORED |
      GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY [ ( sequence_options ) ] |
      UNIQUE [ NULLS [ NOT ] DISTINCT ] index_parameters |
      PRIMARY KEY index_parameters |
      REFERENCES reftable [ ( refcolumn ) ] [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ]
        [ ON DELETE referential_action ] [ ON UPDATE referential_action ] }
    [ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]

and table_constraint is:
    [ CONSTRAINT constraint_name ]
    { CHECK ( expression ) [ NO INHERIT ] |
      UNIQUE [ NULLS [ NOT ] DISTINCT ] ( column_name [, ... ] ) index_parameters |
      PRIMARY KEY ( column_name [, ... ] ) index_parameters |
      EXCLUDE [ USING index_method ] ( exclude_element WITH operator [, ... ] ) index_parameters [ WHERE ( predicate ) ] |
      FOREIGN KEY ( column_name [, ... ] ) REFERENCES reftable [ ( refcolumn [, ... ] ) ]
        [ MATCH FULL | MATCH PARTIAL | MATCH SIMPLE ] [ ON DELETE referential_action ] [ ON UPDATE referential_action ] }
    [ DEFERRABLE | NOT DEFERRABLE ] [ INITIALLY DEFERRED | INITIALLY IMMEDIATE ]

and like_option is:
    { INCLUDING | EXCLUDING } { COMMENTS | COMPRESSION | CONSTRAINTS | DEFAULTS | GENERATED | IDENTITY | INDEXES | STATISTICS | STORAGE | ALL }

and index_parameters in UNIQUE, PRIMARY KEY, and EXCLUDE constraints are:
    [ INCLUDE ( column_name [, ... ] ) ]
    [ WITH ( storage_parameter [= value] [, ... ] ) ]
    [ USING INDEX TABLESPACE tablespace_name ]

and exclude_element in an EXCLUDE constraint is:
    { column_name | ( expression ) } [ opclass ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ]

and referential_action is:
    { NO ACTION | RESTRICT | CASCADE | SET NULL [ ( column_name [, ... ] ) ] | SET DEFAULT [ ( column_name [, ... ] ) ] }
```

## Parameters

### Table Parameters

| Parameter | Description |
|-----------|-------------|
| `GLOBAL TEMPORARY` | Table definition visible to all sessions, data session-private |
| `LOCAL TEMPORARY` | Table and data session-private (default for TEMP) |
| `TEMPORARY`, `TEMP` | Short form for LOCAL TEMPORARY |
| `UNLOGGED` | Table changes not WAL-logged (faster, not crash-safe) |
| `IF NOT EXISTS` | Skip creation if table exists (no error) |
| `table_name` | Table name, supports full path syntax |
| `INHERITS` | Inherit columns from parent table(s) |
| `PARTITION BY` | Create as partitioned table |
| `WITH` | Storage parameters |
| `ON COMMIT` | Behavior for temp tables at transaction end |
| `TABLESPACE` | Physical storage location |

### Column Parameters

| Parameter | Description |
|-----------|-------------|
| `column_name` | Column identifier |
| `data_type` | SB data type or domain |
| `NOT NULL` | Column must have value |
| `DEFAULT` | Default value expression |
| `GENERATED ALWAYS AS` | Computed column (expression stored) |
| `GENERATED AS IDENTITY` | Auto-increment (standard SQL) |
| `UNIQUE` | Unique constraint |
| `PRIMARY KEY` | Primary key constraint |
| `REFERENCES` | Foreign key constraint |
| `CHECK` | Check constraint |

## Description

`CREATE TABLE` creates a new table in the specified schema. Tables are schema-scoped objects that store data in rows with defined columns and constraints.

### Table Types

| Type | Persistence | Visibility | Use Case |
|------|-------------|------------|----------|
| Regular | Permanent | All sessions | Production data |
| TEMPORARY | Session | Single session | Temp processing |
| UNLOGGED | Permanent (no WAL) | All sessions | Rebuildable data, ETL |
| GLOBAL TEMP | Permanent def, temp data | Definition: all; Data: session | Session isolation |

### Storage Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fillfactor` | integer | 100 | Page fill percentage |
| `autovacuum_enabled` | boolean | true | Enable autovacuum |
| `toast_compression` | string | 'lz4' | TOAST compression |
| `parallel_workers` | integer | - | Parallel scan workers |
| `page_size` | integer | database default | Page size for this table |

## Examples

### Basic Table

```sql
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email TEXT NOT NULL UNIQUE,
    created_at TIMESTAMPTZ DEFAULT NOW()
);
```

### With Constraints

```sql
CREATE TABLE orders (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL,
    amount DECIMAL(10,2) CHECK (amount > 0),
    status TEXT DEFAULT 'pending',
    created_at TIMESTAMPTZ DEFAULT NOW(),
    
    CONSTRAINT fk_user 
        FOREIGN KEY (user_id) 
        REFERENCES users(id) 
        ON DELETE CASCADE,
    
    CONSTRAINT valid_status 
        CHECK (status IN ('pending', 'paid', 'shipped', 'cancelled'))
);
```

### Temporary Table

```sql
CREATE TEMP TABLE temp_analysis (
    user_id UUID,
    order_count INT,
    total_spent DECIMAL(10,2)
) ON COMMIT DROP;
```

### Table Inheritance

```sql
CREATE TABLE products (
    id UUID PRIMARY KEY,
    name TEXT NOT NULL,
    price DECIMAL(10,2)
);

CREATE TABLE books (
    isbn TEXT UNIQUE,
    author TEXT,
    pages INT
) INHERITS (products);
```

### Partitioned Table

```sql
CREATE TABLE events (
    id UUID,
    event_time TIMESTAMPTZ,
    data JSONB
) PARTITION BY RANGE (event_time);

CREATE TABLE events_2024q1 PARTITION OF events
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');
```

### LIKE (Copy Structure)

```sql
CREATE TABLE users_archive (
    LIKE users INCLUDING ALL
);

CREATE TABLE users_staging (
    LIKE users INCLUDING DEFAULTS EXCLUDING INDEXES
);
```

### With Path

```sql
CREATE TABLE !:prod.mydb.public.metrics (
    id UUID PRIMARY KEY,
    name TEXT,
    value DOUBLE PRECISION
) TABLESPACE fast_ssd;
```

### Generated Columns

```sql
CREATE TABLE products (
    id UUID PRIMARY KEY,
    name TEXT,
    price DECIMAL(10,2),
    tax_rate DECIMAL(5,4),
    total_price DECIMAL(10,2) 
        GENERATED ALWAYS AS (price * (1 + tax_rate)) STORED
);
```

### Identity Column

```sql
CREATE TABLE invoices (
    id BIGINT GENERATED ALWAYS AS IDENTITY 
        (START WITH 1000 INCREMENT BY 1),
    customer TEXT,
    amount DECIMAL(10,2)
);
```

## Parser Acceptance Cases

```sql
CREATE TABLE t1 (id INT);
CREATE TABLE IF NOT EXISTS t1 (id INT);
CREATE TEMP TABLE t1 (id INT);
CREATE UNLOGGED TABLE t1 (id INT);
CREATE TABLE !:prod.db.public.t1 (id INT);
```

## Parser Rejection Cases

```sql
-- Duplicate column
CREATE TABLE t1 (id INT, id INT);  -- Error: duplicate column

-- Invalid type
CREATE TABLE t1 (col INVALID_TYPE);  -- Error: unknown type

-- Missing data type
CREATE TABLE t1 (col);  -- Error: missing data type
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `duplicate_table` | Table exists (no IF NOT EXISTS) |
| `duplicate_column` | Column defined twice |
| `undefined_type` | Data type doesn't exist |
| `undefined_schema` | Schema in path doesn't exist |

## See Also

- [ALTER TABLE](02_alter_table.md)
- [DROP TABLE](03_drop_table.md)
- [CREATE INDEX](04_create_index.md)
- [Path Resolution and Scoping](../../03_path_resolution_and_scoping.md)
