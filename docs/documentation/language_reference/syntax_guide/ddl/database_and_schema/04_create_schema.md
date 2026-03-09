<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE SCHEMA

[Prev](./03_drop_database.md) | [Next](./05_alter_schema.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

## Synopsis

Creates a new schema within a database. Schemas are namespaces that contain tables, views, functions, and other schema-scoped objects.

## Syntax

```sql
CREATE SCHEMA [ IF NOT EXISTS ] schema_name
    [ AUTHORIZATION user_name ]
    [ schema_element [ ... ] ]

where schema_element can be:
    CREATE TABLE ...
    CREATE VIEW ...
    CREATE INDEX ...
    CREATE SEQUENCE ...
    CREATE TRIGGER ...
    CREATE FUNCTION ...
    CREATE PROCEDURE ...
    CREATE TYPE ...
    GRANT ...
```

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `IF NOT EXISTS` | keyword | No | Skip creation if schema exists |
| `schema_name` | identifier | Yes | Name of schema. Supports qualified paths. |
| `AUTHORIZATION` | identifier | No | User/role that owns the schema |
| `schema_element` | statement | No | Sub-statements to execute within new schema |

## Description

`CREATE SCHEMA` creates a new schema within the current or specified database. A schema is a namespace that:
- Contains tables, views, functions, procedures, triggers, types, and other objects
- Provides isolation between different applications or users
- Enables granular permission management

### Schema Paths

Schema names can be qualified with database paths:

| Form | Resolution |
|------|------------|
| `schema_name` | Current database |
| `database.schema` | Specified database, current environment |
| `!:env.db.schema` | Absolute path |
| `.:db.schema` | Relative to current environment |

### Sub-statements

When `schema_element` sub-statements are provided, they execute atomically:
- All succeed or all fail (single transaction)
- Objects are created within the new schema
- Owner defaults to schema owner

## Examples

### Basic Schema Creation

```sql
-- Create schema in current database
CREATE SCHEMA app_data;

-- Create with explicit authorization
CREATE SCHEMA app_data AUTHORIZATION app_admin;

-- Create with IF NOT EXISTS
CREATE SCHEMA IF NOT EXISTS app_data;
```

### Qualified Path Creation

```sql
-- Create in specific database
CREATE SCHEMA prod.public;

-- Create with absolute path
CREATE SCHEMA !:prod.mydb.app_schema;

-- Create in different environment
CREATE SCHEMA !:dev.mydb.test_schema;
```

### Schema with Objects

```sql
-- Create schema and objects atomically
CREATE SCHEMA app_v2 AUTHORIZATION app_owner
    CREATE TABLE users (
        id UUID PRIMARY KEY,
        email TEXT NOT NULL
    )
    CREATE TABLE orders (
        id UUID PRIMARY KEY,
        user_id UUID REFERENCES users(id)
    )
    CREATE INDEX idx_users_email ON users(email)
    CREATE VIEW active_users AS 
        SELECT * FROM users WHERE status = 'active'
    GRANT SELECT ON active_users TO report_user;
```

### Emulated Schema (Database)

```sql
-- Create schema that presents as PostgreSQL database
CREATE SCHEMA !:sb_main.emulated_pg.legacy_app;

-- Create schema that presents as MySQL database  
CREATE SCHEMA !:sb_main.emulated_mysql.webstore;
```

### Multi-Tenant Schemas

```sql
-- Tenant isolation via schemas
CREATE SCHEMA tenant_123_data AUTHORIZATION tenant_123_admin;
CREATE SCHEMA tenant_456_data AUTHORIZATION tenant_456_admin;

-- Cross-schema query (native SB only)
SELECT t1.id, t1.name, t2.subscription
FROM tenant_123_data.customers t1
JOIN tenant_456_data.subscriptions t2 ON t1.id = t2.customer_id;
```

## Parser Acceptance Cases

```sql
CREATE SCHEMA myschema;
CREATE SCHEMA IF NOT EXISTS myschema;
CREATE SCHEMA myschema AUTHORIZATION user1;
CREATE SCHEMA !:prod.mydb.myschema;
CREATE SCHEMA mydb.myschema;
CREATE SCHEMA myschema CREATE TABLE t1 (id INT);
```

## Parser Rejection Cases

```sql
-- Missing schema name
CREATE SCHEMA;

-- Duplicate in same transaction (without IF NOT EXISTS)
CREATE SCHEMA myschema;
CREATE SCHEMA myschema;  -- Error: schema already exists

-- Invalid path
CREATE SCHEMA !:env_only_no_schema;  -- Error: incomplete path

-- Non-existent authorization target
CREATE SCHEMA myschema AUTHORIZATION nonexistent;  -- Error: user does not exist

-- Invalid sub-statement
CREATE SCHEMA myschema CREATE TABLE;  -- Error: invalid table definition
```

## Error Conditions

| Error Code | Condition | Resolution |
|------------|-----------|------------|
| `duplicate_schema` | Schema exists (no IF NOT EXISTS) | Use IF NOT EXISTS or DROP first |
| `invalid_schema_name` | Reserved name or invalid characters | Use different name |
| `undefined_database` | Database in path does not exist | Create database first |
| `undefined_user` | AUTHORIZATION target does not exist | Create user first |
| `insufficient_privilege` | User lacks CREATE permission | Grant permission |
| `invalid_schema_element` | Sub-statement invalid | Check sub-statement syntax |

## Notes

- Schema names follow identifier rules: max 63 characters, case-insensitive (unless quoted)
- Reserved schema names: `pg_catalog`, `information_schema`, `sys`, `public` (can be used but not recommended)
- Each database has a `public` schema by default
- Search path controls unqualified object resolution
- Schema owner has all privileges within the schema by default
- Sub-statements in CREATE SCHEMA execute with schema owner privileges
- `current_schema()` function returns the current schema name

## Completion Checklist

- [x] Canonical forms documented
- [x] Clause matrix completed
- [x] Positive and negative parser cases listed
- [x] Examples validated against v3 parser behavior
- [x] Error conditions documented
- [x] Cross-references added

## See Also

- [ALTER SCHEMA](05_alter_schema.md)
- [DROP SCHEMA](06_drop_schema.md)
- [CREATE DATABASE](01_create_database.md)
- [Path Resolution and Scoping](../../03_path_resolution_and_scoping.md)
