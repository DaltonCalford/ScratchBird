[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - System Catalog Surface

**Status:** Alpha documentation
**Last Updated:** 2026-01-30

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers system catalog access in PostgreSQL emulation mode. PostgreSQL applications often query system catalogs (`pg_catalog`) and the SQL standard `information_schema` for metadata. ScratchBird emulates these catalogs by providing views over its native metadata.

**Spec refs:**
- `ScratchBird/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md`

---

## Catalog Namespaces

PostgreSQL provides two standard ways to access metadata:

| Namespace | Description | Standard |
|-----------|-------------|----------|
| `pg_catalog` | PostgreSQL-specific system catalogs | PostgreSQL |
| `information_schema` | SQL standard metadata views | SQL:2016 |

---

## pg_catalog Tables

### Core Catalog Tables

#### pg_class

Contains information about tables, indexes, sequences, views, and other relations.

```sql
SELECT
    relname AS table_name,
    relkind AS type,
    reltuples AS row_estimate,
    relpages AS page_count
FROM pg_catalog.pg_class
WHERE relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public')
AND relkind IN ('r', 'v', 'i');  -- r=table, v=view, i=index

-- Common relkind values:
-- 'r' = ordinary table
-- 'i' = index
-- 'S' = sequence
-- 'v' = view
-- 'm' = materialized view
-- 'c' = composite type
-- 't' = TOAST table
-- 'f' = foreign table
-- 'p' = partitioned table
```

#### pg_attribute

Contains column information for tables.

```sql
SELECT
    a.attname AS column_name,
    t.typname AS data_type,
    a.attnum AS position,
    a.attnotnull AS not_null,
    a.atthasdef AS has_default
FROM pg_catalog.pg_attribute a
JOIN pg_catalog.pg_class c ON a.attrelid = c.oid
JOIN pg_catalog.pg_type t ON a.atttypid = t.oid
WHERE c.relname = 'users'
AND a.attnum > 0  -- Exclude system columns
AND NOT a.attisdropped
ORDER BY a.attnum;
```

#### pg_namespace

Contains schema information.

```sql
SELECT
    nspname AS schema_name,
    nspowner AS owner_oid
FROM pg_catalog.pg_namespace
WHERE nspname NOT LIKE 'pg_%'
AND nspname != 'information_schema';
```

#### pg_type

Contains data type definitions.

```sql
SELECT
    typname AS type_name,
    typlen AS length,
    typtype AS type_kind,
    typcategory AS category
FROM pg_catalog.pg_type
WHERE typnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'pg_catalog')
AND typtype = 'b'  -- Base types only
ORDER BY typname;
```

#### pg_index

Contains index information.

```sql
SELECT
    c.relname AS index_name,
    t.relname AS table_name,
    i.indisunique AS is_unique,
    i.indisprimary AS is_primary,
    i.indkey AS column_positions
FROM pg_catalog.pg_index i
JOIN pg_catalog.pg_class c ON i.indexrelid = c.oid
JOIN pg_catalog.pg_class t ON i.indrelid = t.oid
WHERE t.relname = 'users';
```

#### pg_constraint

Contains constraint definitions.

```sql
SELECT
    conname AS constraint_name,
    contype AS type,
    c.relname AS table_name
FROM pg_catalog.pg_constraint con
JOIN pg_catalog.pg_class c ON con.conrelid = c.oid
WHERE c.relname = 'orders';

-- contype values:
-- 'c' = check constraint
-- 'f' = foreign key
-- 'p' = primary key
-- 'u' = unique
-- 't' = trigger constraint
-- 'x' = exclusion constraint
```

### Security Catalogs

#### pg_roles

Contains role information.

```sql
SELECT
    rolname AS role_name,
    rolsuper AS is_superuser,
    rolcreatedb AS can_create_db,
    rolcreaterole AS can_create_role,
    rolcanlogin AS can_login
FROM pg_catalog.pg_roles;
```

#### pg_user

View of roles with login capability.

```sql
SELECT
    usename AS username,
    usesysid AS user_id,
    usecreatedb AS can_create_db,
    usesuper AS is_superuser
FROM pg_catalog.pg_user;
```

#### pg_auth_members

Contains role membership information.

```sql
SELECT
    r.rolname AS role,
    m.rolname AS member,
    grantor.rolname AS grantor
FROM pg_catalog.pg_auth_members am
JOIN pg_catalog.pg_roles r ON am.roleid = r.oid
JOIN pg_catalog.pg_roles m ON am.member = m.oid
JOIN pg_catalog.pg_roles grantor ON am.grantor = grantor.oid;
```

### Statistics Catalogs

#### pg_stat_user_tables

Statistics for user tables.

```sql
SELECT
    schemaname,
    relname AS table_name,
    seq_scan,
    seq_tup_read,
    idx_scan,
    idx_tup_fetch,
    n_tup_ins AS inserts,
    n_tup_upd AS updates,
    n_tup_del AS deletes,
    n_live_tup AS live_rows,
    n_dead_tup AS dead_rows,
    last_vacuum,
    last_autovacuum,
    last_analyze
FROM pg_stat_user_tables
ORDER BY seq_scan DESC;
```

#### pg_stat_user_indexes

Statistics for user indexes.

```sql
SELECT
    schemaname,
    relname AS table_name,
    indexrelname AS index_name,
    idx_scan AS scans,
    idx_tup_read AS rows_read,
    idx_tup_fetch AS rows_fetched
FROM pg_stat_user_indexes
WHERE idx_scan = 0;  -- Unused indexes
```

#### pg_stat_activity

Current database activity.

```sql
SELECT
    pid,
    usename AS username,
    datname AS database,
    state,
    query,
    query_start,
    wait_event_type,
    wait_event
FROM pg_stat_activity
WHERE state = 'active';
```

---

## information_schema Views

The `information_schema` provides SQL-standard metadata views.

### Tables and Columns

#### information_schema.tables

```sql
SELECT
    table_catalog,
    table_schema,
    table_name,
    table_type
FROM information_schema.tables
WHERE table_schema = 'public'
ORDER BY table_name;

-- table_type values:
-- 'BASE TABLE'
-- 'VIEW'
-- 'FOREIGN TABLE'
-- 'LOCAL TEMPORARY'
```

#### information_schema.columns

```sql
SELECT
    table_name,
    column_name,
    ordinal_position,
    column_default,
    is_nullable,
    data_type,
    character_maximum_length,
    numeric_precision,
    numeric_scale
FROM information_schema.columns
WHERE table_schema = 'public'
AND table_name = 'users'
ORDER BY ordinal_position;
```

### Constraints

#### information_schema.table_constraints

```sql
SELECT
    constraint_name,
    table_name,
    constraint_type
FROM information_schema.table_constraints
WHERE table_schema = 'public'
ORDER BY table_name, constraint_type;

-- constraint_type values:
-- 'PRIMARY KEY'
-- 'FOREIGN KEY'
-- 'UNIQUE'
-- 'CHECK'
```

#### information_schema.key_column_usage

```sql
SELECT
    constraint_name,
    table_name,
    column_name,
    ordinal_position
FROM information_schema.key_column_usage
WHERE table_schema = 'public'
ORDER BY constraint_name, ordinal_position;
```

#### information_schema.referential_constraints

```sql
SELECT
    constraint_name,
    unique_constraint_name,
    match_option,
    update_rule,
    delete_rule
FROM information_schema.referential_constraints
WHERE constraint_schema = 'public';
```

### Routines and Parameters

#### information_schema.routines

```sql
SELECT
    routine_name,
    routine_type,  -- 'FUNCTION' or 'PROCEDURE'
    data_type AS return_type,
    routine_definition
FROM information_schema.routines
WHERE routine_schema = 'public';
```

#### information_schema.parameters

```sql
SELECT
    specific_name,
    parameter_name,
    ordinal_position,
    parameter_mode,  -- 'IN', 'OUT', 'INOUT'
    data_type
FROM information_schema.parameters
WHERE specific_schema = 'public'
ORDER BY specific_name, ordinal_position;
```

### Privileges

#### information_schema.table_privileges

```sql
SELECT
    grantee,
    table_name,
    privilege_type,
    is_grantable
FROM information_schema.table_privileges
WHERE table_schema = 'public'
ORDER BY table_name, grantee;
```

#### information_schema.column_privileges

```sql
SELECT
    grantee,
    table_name,
    column_name,
    privilege_type
FROM information_schema.column_privileges
WHERE table_schema = 'public';
```

### Views

#### information_schema.views

```sql
SELECT
    table_name AS view_name,
    view_definition,
    check_option,
    is_updatable
FROM information_schema.views
WHERE table_schema = 'public';
```

### Triggers

#### information_schema.triggers

```sql
SELECT
    trigger_name,
    event_manipulation,  -- 'INSERT', 'UPDATE', 'DELETE'
    event_object_table,
    action_timing,       -- 'BEFORE' or 'AFTER'
    action_statement
FROM information_schema.triggers
WHERE trigger_schema = 'public';
```

---

## Common Queries

### List All Tables with Columns

```sql
SELECT
    t.table_name,
    c.column_name,
    c.data_type,
    c.is_nullable,
    c.column_default
FROM information_schema.tables t
JOIN information_schema.columns c
    ON t.table_name = c.table_name
    AND t.table_schema = c.table_schema
WHERE t.table_schema = 'public'
AND t.table_type = 'BASE TABLE'
ORDER BY t.table_name, c.ordinal_position;
```

### List Foreign Key Relationships

```sql
SELECT
    tc.table_name AS from_table,
    kcu.column_name AS from_column,
    ccu.table_name AS to_table,
    ccu.column_name AS to_column
FROM information_schema.table_constraints tc
JOIN information_schema.key_column_usage kcu
    ON tc.constraint_name = kcu.constraint_name
JOIN information_schema.constraint_column_usage ccu
    ON tc.constraint_name = ccu.constraint_name
WHERE tc.constraint_type = 'FOREIGN KEY'
AND tc.table_schema = 'public';
```

### List Indexes with Columns

```sql
SELECT
    i.relname AS index_name,
    t.relname AS table_name,
    a.attname AS column_name,
    ix.indisunique AS is_unique,
    ix.indisprimary AS is_primary
FROM pg_catalog.pg_index ix
JOIN pg_catalog.pg_class i ON ix.indexrelid = i.oid
JOIN pg_catalog.pg_class t ON ix.indrelid = t.oid
JOIN pg_catalog.pg_attribute a ON a.attrelid = t.oid
    AND a.attnum = ANY(ix.indkey)
WHERE t.relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'public')
ORDER BY t.relname, i.relname;
```

### Check Table Size

```sql
SELECT
    relname AS table_name,
    pg_size_pretty(pg_total_relation_size(c.oid)) AS total_size,
    pg_size_pretty(pg_table_size(c.oid)) AS table_size,
    pg_size_pretty(pg_indexes_size(c.oid)) AS index_size
FROM pg_catalog.pg_class c
JOIN pg_catalog.pg_namespace n ON c.relnamespace = n.oid
WHERE n.nspname = 'public'
AND c.relkind = 'r'
ORDER BY pg_total_relation_size(c.oid) DESC;
```

### Find Unused Indexes

```sql
SELECT
    schemaname,
    relname AS table_name,
    indexrelname AS index_name,
    pg_size_pretty(pg_relation_size(i.indexrelid)) AS index_size
FROM pg_stat_user_indexes i
JOIN pg_index USING (indexrelid)
WHERE idx_scan = 0
AND NOT indisunique
AND NOT indisprimary
ORDER BY pg_relation_size(i.indexrelid) DESC;
```

### List All Privileges for User

```sql
SELECT
    grantee,
    table_schema,
    table_name,
    privilege_type
FROM information_schema.table_privileges
WHERE grantee = 'app_user'
ORDER BY table_schema, table_name;
```

### Schema Dump Query

```sql
-- List all object types in schema
SELECT
    CASE c.relkind
        WHEN 'r' THEN 'table'
        WHEN 'v' THEN 'view'
        WHEN 'm' THEN 'materialized view'
        WHEN 'i' THEN 'index'
        WHEN 'S' THEN 'sequence'
        WHEN 'f' THEN 'foreign table'
        WHEN 'p' THEN 'partitioned table'
    END AS object_type,
    c.relname AS object_name
FROM pg_catalog.pg_class c
JOIN pg_catalog.pg_namespace n ON c.relnamespace = n.oid
WHERE n.nspname = 'public'
AND c.relkind IN ('r', 'v', 'm', 'i', 'S', 'f', 'p')
ORDER BY object_type, object_name;
```

---

## System Functions

### Object Information Functions

```sql
-- Get OID of a table
SELECT 'users'::regclass::oid;

-- Get table name from OID
SELECT 16384::regclass;

-- Get current database
SELECT current_database();

-- Get current schema
SELECT current_schema();

-- Get search path
SHOW search_path;
SELECT current_schemas(true);
```

### Has Privilege Functions

```sql
-- Check table privilege
SELECT has_table_privilege('app_user', 'users', 'SELECT');
SELECT has_table_privilege('users', 'INSERT');  -- Current user

-- Check schema privilege
SELECT has_schema_privilege('app_user', 'public', 'USAGE');

-- Check database privilege
SELECT has_database_privilege('app_user', 'mydb', 'CONNECT');

-- Check column privilege
SELECT has_column_privilege('app_user', 'users', 'email', 'SELECT');
```

### Object Existence Functions

```sql
-- Check if table exists
SELECT to_regclass('public.users') IS NOT NULL;

-- Check if function exists
SELECT to_regproc('my_function') IS NOT NULL;

-- List all objects matching name
SELECT * FROM pg_catalog.pg_class WHERE relname = 'users';
```

---

## Known Limitations

### Current Implementation Status

| Catalog | Status | Notes |
|---------|--------|-------|
| pg_class | Partial | Core columns populated |
| pg_attribute | Partial | Basic column info |
| pg_namespace | Implemented | Schema listing works |
| pg_type | Partial | Built-in types only |
| pg_index | Partial | Basic index info |
| pg_constraint | Partial | PK/FK constraints |
| pg_roles | Partial | Basic role info |
| pg_stat_* | Stubbed | Statistics not collected |
| information_schema.tables | Implemented | Works correctly |
| information_schema.columns | Implemented | Works correctly |
| information_schema.constraints | Partial | Basic constraints |
| information_schema.routines | Stubbed | Limited support |

### Specific Issues

**Missing or Incomplete:**
- Statistics views (`pg_stat_*`) return empty or dummy data
- `pg_settings` is partially populated
- System activity views have limited data
- Some `pg_catalog` columns return NULL
- Object sizes may not be accurate

**Workarounds:**
- Use `information_schema` views where possible (better emulation coverage)
- For missing statistics, implement application-level monitoring
- Use native ScratchBird system tables for accurate metadata

### Native Metadata Alternative

For complete metadata access, use ScratchBird native system tables:

```sql
-- Native table listing
SELECT * FROM sb_catalog.tables WHERE schema_name = 'public';

-- Native column listing
SELECT * FROM sb_catalog.columns WHERE table_name = 'users';

-- Native index listing
SELECT * FROM sb_catalog.indexes WHERE table_name = 'users';
```

---

## See Also

- [Databases and Schemas](01_databases_and_schemas.md) - Schema management
- [Tables and Constraints](02_tables_and_constraints.md) - Table DDL
- [Indexes, Views, Sequences](03_indexes_views_sequences.md) - Index management
- [Security DCL](09_security_dcl.md) - Privilege management

