<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Path Resolution and Scoping

[Prev](02_statement_family_index.md) | [Next](ddl/README.md) | [Topic README](README.md) | [Language Reference README](../README.md) | [Documentation Workspace README](../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

ScratchBird uses a hierarchical path syntax to identify objects across environments, databases, and schemas. Paths support both absolute (`!:`) and relative (`.:`, `..:`, `...:`) navigation with explicit scope boundaries.

## Path Prefixes

| Prefix | Name | Scope | Example |
|--------|------|-------|---------|
| `!:` | Absolute | Environment root | `!:prod.mydb.public.users` |
| `.:` | Current | Current environment | `.:mydb.public.users` |
| `..:` | Parent | Parent environment | `..:shared.public.config` |
| `...:` | Grandparent | Grandparent environment | `...:global.system.settings` |
| (none) | Contextual | Current schema | `users` or `public.users` |

## Syntax

```ebnf
schema_path ::= absolute_env_path
              | relative_env_path
              | parent_env_path
              | local_database_path
              | local_schema_path

absolute_env_path    ::= "!:" environment_path
relative_env_path    ::= ".:" environment_path
parent_env_path      ::= "..:" environment_path
                       | "...:" environment_path

environment_path     ::= [ database_group "." ] database_path
database_path        ::= database_name [ "." schema_path_tail ]
schema_path_tail     ::= schema_name [ "." object_path_tail ]
object_path_tail     ::= object_name { "." object_name }

local_database_path  ::= database_name "." schema_name [ "." object_name ]
local_schema_path    ::= schema_name "." object_name
                       | object_name
```

## Scope Levels and Path Requirements

Objects in ScratchBird exist at four scope levels, each with specific path requirements:

### Cluster-Global Objects

Simple names only. No path prefix required.

| Object Type | Example | Path Required |
|-------------|---------|---------------|
| TABLESPACE | `fast_ssd` | No |
| FOREIGN DATA WRAPPER | `postgres_fdw` | No |
| ACCESS METHOD | `btree` | No |

```sql
-- Cluster-global: simple names only
CREATE TABLESPACE fast_ssd LOCATION '/ssd/scratchbird';
CREATE FOREIGN DATA WRAPPER postgres_fdw;
```

### Environment-Scoped Objects

May include environment path prefix. If omitted, current environment is assumed.

| Object Type | Example | Path Support |
|-------------|---------|--------------|
| DATABASE | `!:prod.mydb` | `[env_path.]name` |
| USER | `!:prod.myuser` | `[env_path.]name` |
| ROLE | `!:prod.myrole` | `[env_path.]name` |
| GROUP | `!:prod.mygroup` | `[env_path.]name` |
| SERVER | `!:prod.myserver` | `[env_path.]name` |
| EXTENSION | `!:prod.myext` | `[env_path.]name` |
| JOB | `!:prod.myjob` | `[env_path.]name` |
| SCHEDULE | `!:prod.myschedule` | `[env_path.]name` |
| TOKEN | `!:prod.mytoken` | `[env_path.]name` |
| RULE | `!:prod.myrule` | `[env_path.]name` |
| PROFILE | `!:prod.myprofile` | `[env_path.]name` |
| CHANNEL | `!:prod.mychannel` | `[env_path.]name` |

```sql
-- Environment-scoped: absolute path
CREATE USER !:prod.emulated_pg.john WITH PASSWORD 'secret';

-- Environment-scoped: relative to current
CREATE USER .:john WITH PASSWORD 'secret';

-- Environment-scoped: no path (current environment)
CREATE USER john WITH PASSWORD 'secret';
```

### Schema-Scoped Objects

Require full schema path resolution.

| Object Type | Example | Path Required |
|-------------|---------|---------------|
| TABLE | `!:prod.mydb.public.users` | Full schema path |
| VIEW | `!:prod.mydb.public.active_users` | Full schema path |
| INDEX | `!:prod.mydb.public.idx_users_email` | Full schema path |
| SEQUENCE | `!:prod.mydb.public.user_seq` | Full schema path |
| FUNCTION | `!:prod.mydb.public.calculate_tax` | Full schema path |
| PROCEDURE | `!:prod.mydb.public.process_order` | Full schema path |
| TRIGGER | `!:prod.mydb.public.trg_update` | Full schema path |
| PACKAGE | `!:prod.mydb.public.utils_pkg` | Full schema path |
| TYPE | `!:prod.mydb.public.point_type` | Full schema path |
| SYNONYM | `!:prod.mydb.public.user_syn` | Full schema path |
| POLICY | `!:prod.mydb.public.user_policy` | Full schema path |
| STATISTICS | `!:prod.mydb.public.user_stats` | Full schema path |

```sql
-- Schema-scoped: absolute path
CREATE TABLE !:prod.mydb.public.users (
    id UUID PRIMARY KEY,
    email TEXT NOT NULL
);

-- Schema-scoped: relative to current environment
CREATE TABLE .:mydb.public.orders (
    id UUID PRIMARY KEY
);

-- Schema-scoped: local (current database.schema)
CREATE TABLE public.users (id UUID PRIMARY KEY);
```

### Database-Global Objects

Simple names resolved within current database only.

| Object Type | Example | Scope |
|-------------|---------|-------|
| DOMAIN | `email_type` | Current database only |
| EXCEPTION | `division_by_zero` | Current database only |
| COLLATION | `en_US` | Current database only |
| LANGUAGE | `plpgsql` | Current database only |

```sql
-- Database-global: simple name, current database only
CREATE DOMAIN email_type AS TEXT 
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');
```

## Path Resolution Algorithm

When the parser encounters an identifier:

1. **If prefixed with `!:`**
   - Resolve from environment root
   - Format: `!:environment.database.schema.object`

2. **If prefixed with `.:`**
   - Resolve from current environment
   - Format: `.:database.schema.object`

3. **If prefixed with `..:`**
   - Resolve from parent environment
   - Format: `..:database.schema.object`

4. **If prefixed with `...:`**
   - Resolve from grandparent environment
   - Format: `...:database.schema.object`

5. **If containing `.` but no prefix**
   - First component is checked:
     - If matches a database name → `database.schema.object`
     - If matches a schema name → `schema.object`

6. **Simple name (no dots)**
   - Resolved in current schema context

## Emulated Database Paths

Emulated databases are schemas within the SB environment. The parser auto-qualifies paths for emulated clients.

```sql
-- Client perspective (PostgreSQL emulation)
SELECT * FROM users;

-- SB internal resolution
SELECT * FROM !:sb_main.emulated_pg.mydb.users;

-- Client perspective (with schema)
SELECT * FROM public.users;

-- SB internal resolution
SELECT * FROM !:sb_main.emulated_pg.mydb.public.users;
```

### Sandboxing

Emulated parsers are sandboxed to their schema path. Cross-database queries are rejected at parse time.

```sql
-- Client connected to emulated_pg.mydb attempts:
SELECT * FROM other_db.table;

-- Parser auto-qualifies:
SELECT * FROM !:sb_main.emulated_pg.other_db.table;

-- Validation fails: sandbox is !:sb_main.emulated_pg.mydb
-- Result: PERMISSION DENIED (outside sandbox)
```

## Examples by Use Case

### Cross-Database Query (Native SB)

```sql
-- Absolute paths allow cross-database queries in native SB
SELECT 
    u.id,
    u.email,
    o.order_id
FROM !:prod.crm.public.users u
JOIN !:prod.orders.public.orders o ON u.id = o.user_id
WHERE o.created_at > NOW() - INTERVAL '30 days';
```

### Environment Migration

```sql
-- Copy schema from dev to prod using relative paths
CREATE SCHEMA !:prod.main.public AS 
    SELECT * FROM !:dev.main.public;
```

### Shared Configuration

```sql
-- Access shared config in parent environment
SELECT * FROM ..:shared.public.config WHERE key = 'mail_server';
```

### Multi-Tenant Isolation

```sql
-- Each tenant sees only their schema
CREATE TABLE !:prod.tenant_123.public.data (id UUID PRIMARY KEY);

-- Tenant 123 can only access their path
-- Tenant 456 cannot reference tenant_123 objects
```

## Error Conditions

| Error | Cause | Example |
|-------|-------|---------|
| `invalid_path_syntax` | Malformed path | `!:..invalid` |
| `unknown_environment` | Environment doesn't exist | `!:nonexistent.db.table` |
| `unknown_database` | Database doesn't exist | `!:prod.nonexistent.table` |
| `unknown_schema` | Schema doesn't exist | `!:prod.db.nonexistent` |
| `outside_sandbox` | Emulated client crossing sandbox | See sandboxing section |
| `insufficient_scope` | Wrong scope for object type | `!:global.tablespace_name` |

## Notes

- Path resolution is case-sensitive for identifiers
- Environment names follow DNS naming conventions
- Maximum path depth: 4 levels (environment.database.schema.object)
- Reserved environment names: `sb`, `sys`, `pg_catalog`, `information_schema`
- Path prefixes are part of the canonical SQL dialect only

## See Also

- [DDL - Database and Schema](ddl/database_and_schema/README.md)
- [System Catalog](developers_guide/system_catalog/README.md)
- [Multi-Tenant Architecture](developers_guide/architecture/09_group_and_cluster_trust_models.md)
- [CREATE DATABASE](ddl/database_and_schema/02_create_database.md)
- [CREATE SCHEMA](ddl/database_and_schema/01_create_schema.md)
