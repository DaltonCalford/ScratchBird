<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# DROP TABLE

[Prev](./02_alter_table.md) | [Next](./04_create_index.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

Removes one or more tables and optionally dependent objects.

## Syntax

```sql
DROP TABLE [ IF EXISTS ] table_name [, ...] [ CASCADE | RESTRICT ]
```

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `IF EXISTS` | keyword | No | - | Suppress error if table does not exist |
| `table_name` | identifier | Yes | - | Name of table to drop. Supports qualified paths. |
| `CASCADE` | keyword | No | - | Drop dependent objects (views, FKs, etc.) |
| `RESTRICT` | keyword | No | ✅ | Refuse if dependent objects exist |

## Description

`DROP TABLE` removes tables from the database. By default (RESTRICT), fails if other objects depend on the table. With CASCADE, automatically drops dependent objects.

### Dependency Types

| Dependency | CASCADE Behavior |
|------------|------------------|
| Foreign key constraints (referencing) | Dropped |
| Views referencing table | Dropped |
| Functions using table type | Dropped if possible |
| Triggers on table | Dropped |
| Indexes on table | Dropped |
| Child tables (inheritance) | Dropped |

## Examples

### Basic Drop

```sql
-- Drop table (RESTRICT default)
DROP TABLE temp_data;

-- With IF EXISTS
DROP TABLE IF EXISTS temp_data;
```

### Cascade Drop

```sql
-- Drop table and all dependencies
DROP TABLE old_users CASCADE;

-- Combined
DROP TABLE IF EXISTS old_users CASCADE;
```

### Multiple Tables

```sql
-- Drop multiple tables
DROP TABLE temp1, temp2, temp3;

-- With cascade
DROP TABLE IF EXISTS staging_*, temp_* CASCADE;
```

### Qualified Path

```sql
-- Absolute path
DROP TABLE !:prod.archive.events_2023q1 CASCADE;

-- Relative path  
DROP TABLE .:mydb.public.temp_results;
```

## Parser Acceptance Cases

```sql
DROP TABLE t1;
DROP TABLE IF EXISTS t1;
DROP TABLE t1 CASCADE;
DROP TABLE t1, t2 CASCADE;
DROP TABLE !:prod.db.public.t1 CASCADE;
```

## Parser Rejection Cases

```sql
-- Has dependent view (RESTRICT default)
-- (view 'user_view' depends on 'users')
DROP TABLE users;  -- Error: dependent objects exist

-- System table
DROP TABLE pg_tables;  -- Error: system catalog
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `undefined_table` | Table doesn't exist (no IF EXISTS) |
| `dependent_objects` | Dependencies exist (RESTRICT) |
| `system_table` | Attempting to drop system catalog |

## See Also

- [CREATE TABLE](01_create_table.md)
- [ALTER TABLE](02_alter_table.md)
- [TRUNCATE](../../dml/README.md)
