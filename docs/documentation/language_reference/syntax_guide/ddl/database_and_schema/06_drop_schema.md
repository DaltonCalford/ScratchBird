<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# DROP SCHEMA

[Prev](./05_alter_schema.md) | [Next](../table_and_constraints/README.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

Removes a schema and optionally all objects contained within it.

## Syntax

```sql
DROP SCHEMA [ IF EXISTS ] schema_name [, ...] [ CASCADE | RESTRICT ]
```

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `IF EXISTS` | keyword | No | - | Suppress error if schema does not exist |
| `schema_name` | identifier | Yes | - | Name of schema to drop. Supports qualified paths. |
| `CASCADE` | keyword | No | - | Drop dependent objects automatically |
| `RESTRICT` | keyword | No | ✅ | Refuse to drop if dependent objects exist |

## Description

`DROP SCHEMA` removes a schema from the database. By default (RESTRICT), the schema must be empty or the command fails. With CASCADE, all contained objects are dropped automatically.

### Drop Behavior

| Mode | Objects in Schema | Result |
|------|-------------------|--------|
| RESTRICT (default) | Empty | Schema dropped |
| RESTRICT (default) | Contains objects | Error: schema not empty |
| CASCADE | Any | Schema and all objects dropped |

### Multiple Schemas

Multiple schemas can be dropped in a single statement:
```sql
DROP SCHEMA schema1, schema2, schema3 CASCADE;
```

## Examples

### Basic Schema Drop

```sql
-- Drop empty schema (RESTRICT is default)
DROP SCHEMA temp_schema;

-- Explicit RESTRICT
DROP SCHEMA temp_schema RESTRICT;

-- With IF EXISTS
DROP SCHEMA IF EXISTS temp_schema;
```

### Cascade Drop

```sql
-- Drop schema and all contained objects
DROP SCHEMA app_v1 CASCADE;

-- Combined with IF EXISTS
DROP SCHEMA IF EXISTS app_v1 CASCADE;
```

### Qualified Path Drop

```sql
-- Drop from specific database
DROP SCHEMA prod.old_schema CASCADE;

-- Absolute path
DROP SCHEMA !:dev.mydb.test_schema CASCADE;

-- Multiple schemas
DROP SCHEMA !:prod.mydb.schema1, !:prod.mydb.schema2 CASCADE;
```

### Clean-up Patterns

```sql
-- Migration cleanup
DROP SCHEMA IF EXISTS migration_temp CASCADE;

-- Version upgrade
DROP SCHEMA IF EXISTS app_v1 CASCADE;
CREATE SCHEMA app_v2;
-- ... recreate objects in v2 ...

-- Tenant removal
DROP SCHEMA IF EXISTS tenant_999_data CASCADE;
DROP USER IF EXISTS tenant_999_admin;
```

## Parser Acceptance Cases

```sql
DROP SCHEMA myschema;
DROP SCHEMA IF EXISTS myschema;
DROP SCHEMA myschema RESTRICT;
DROP SCHEMA myschema CASCADE;
DROP SCHEMA !:prod.mydb.myschema CASCADE;
DROP SCHEMA schema1, schema2 CASCADE;
```

## Parser Rejection Cases

```sql
-- Missing schema name
DROP SCHEMA;

-- Schema has objects (RESTRICT default)
-- (schema 'myschema' contains table 't1')
DROP SCHEMA myschema;  -- Error: schema not empty

-- Cannot drop system schemas
DROP SCHEMA pg_catalog CASCADE;  -- Error: system schema
DROP SCHEMA information_schema;  -- Error: system schema

-- Non-existent schema (without IF EXISTS)
DROP SCHEMA nonexistent;  -- Error: schema does not exist
```

## Error Conditions

| Error Code | Condition | Resolution |
|------------|-----------|------------|
| `undefined_schema` | Schema does not exist (no IF EXISTS) | Use IF EXISTS or check name |
| `dependent_objects` | Schema contains objects (RESTRICT) | Use CASCADE or drop objects first |
| `system_schema` | Attempting to drop system schema | Not allowed |
| `insufficient_privilege` | User lacks DROP permission | Grant permission |
| `undefined_database` | Database in path does not exist | Check path |

## Notes

- DROP SCHEMA is transactional in ScratchBird
- CASCADE drops objects in dependency order (referencing objects before referenced)
- Foreign key constraints may prevent CASCADE if they cross schema boundaries
- System schemas (`pg_catalog`, `information_schema`) cannot be dropped
- Emulated database schemas (e.g., `emulated_pg.mydb`) can be dropped with CASCADE
- Consider backing up data before CASCADE drop on production schemas

## Completion Checklist

- [x] Canonical forms documented
- [x] Clause matrix completed
- [x] Positive and negative parser cases listed
- [x] Examples validated against v3 parser behavior
- [x] Error conditions documented
- [x] Cross-references added

## See Also

- [CREATE SCHEMA](04_create_schema.md)
- [ALTER SCHEMA](05_alter_schema.md)
- [DROP TABLE](../table_and_constraints/03_drop_table.md)
- [Path Resolution and Scoping](../../03_path_resolution_and_scoping.md)
