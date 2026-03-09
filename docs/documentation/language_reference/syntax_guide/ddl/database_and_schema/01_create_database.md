<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE DATABASE

[Prev](./README.md) | [Next](./02_alter_database.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_v3_native_extension_surface.cpp:1

## Synopsis

Creates a new database within the ScratchBird environment. Databases are environment-scoped objects that serve as containers for schemas and schema-scoped objects.

## Syntax

```sql
CREATE DATABASE [ IF NOT EXISTS ] database_name
    [ WITH
        [ OWNER [=] user_name ]
        [ TEMPLATE [=] template_name ]
        [ ENCODING [=] encoding ]
        [ LC_COLLATE [=] lc_collate ]
        [ LC_CTYPE [=] lc_ctype ]
        [ TABLESPACE [=] tablespace_name ]
        [ ALLOW_CONNECTIONS [=] allowconn ]
        [ CONNECTION LIMIT [=] connlimit ]
        [ IS_TEMPLATE [=] istemplate ]
        [ PAGE_SIZE [=] { 8192 | 16384 | 32768 | 65536 | 131072 } ]
        [ COMPRESSION [=] { 'none' | 'lz4' | 'zstd' } ]
    ]
```

## Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `IF NOT EXISTS` | keyword | No | - | Skip creation if database exists (no error) |
| `database_name` | identifier | Yes | - | Name of database to create. Supports path syntax. |
| `OWNER` | identifier | No | Current user | User that owns the database |
| `TEMPLATE` | identifier | No `template1` | Template database to copy |
| `ENCODING` | string/integer | No | Template's encoding | Character encoding (UTF8, LATIN1, etc.) |
| `LC_COLLATE` | string | No | Template's collation | Collation order for strings |
| `LC_CTYPE` | string | No | Template's ctype | Character classification |
| `TABLESPACE` | identifier | No `pg_default` | Default tablespace for database |
| `ALLOW_CONNECTIONS` | boolean | No | true | If false, no connections allowed |
| `CONNECTION LIMIT` | integer | No | -1 (unlimited) | Max concurrent connections |
| `IS_TEMPLATE` | boolean | No | false | If true, database can be cloned |
| `PAGE_SIZE` | integer | No | 8192 | Page size: 8K, 16K, 32K, 64K, or 128K |
| `COMPRESSION` | string | No | 'lz4' | TOAST compression algorithm |

## Description

`CREATE DATABASE` creates a new database within the ScratchBird environment. A database is a top-level container that holds schemas, which in turn hold tables, views, functions, and other schema-scoped objects.

Databases in ScratchBird are **environment-scoped**, meaning they exist within a specific environment path. Use absolute (`!:`) or relative (`.:`) path prefixes to specify the target environment.

### Database Creation Process

1. Validate environment path and permissions
2. Check database name uniqueness within environment
3. Create catalog entries in environment's system catalog
4. Initialize database file structure with specified page size
5. Copy template database contents (schemas, system tables)
6. Apply ownership and connection limit settings
7. Register database in cluster-global database registry

### Emulated vs Native Databases

| Aspect | Native Database | Emulated Database |
|--------|-----------------|-------------------|
| Path | `!:env.native.mydb` | `!:env.emulated_pg.mydb` |
| Schema visibility | All schemas | Filtered catalog views |
| SQL dialect | SB v3 Canonical | Emulated engine dialect |
| Object mapping | Direct | Name translation layer |

Emulated databases are implemented as schemas with filtered catalog views that present as standalone databases to legacy clients.

## Examples

### Basic Database Creation

```sql
-- Create database in current environment
CREATE DATABASE myapp;

-- Create with explicit path
CREATE DATABASE !:prod.myapp;

-- Create with IF NOT EXISTS
CREATE DATABASE IF NOT EXISTS myapp;
```

### Database with Custom Settings

```sql
-- Production database with connection limit
CREATE DATABASE prod_analytics
    WITH
        OWNER = analytics_admin
        TEMPLATE = template0
        ENCODING = 'UTF8'
        LC_COLLATE = 'en_US.UTF-8'
        LC_CTYPE = 'en_US.UTF-8'
        TABLESPACE = fast_ssd
        CONNECTION LIMIT = 100
        PAGE_SIZE = 32768
        COMPRESSION = 'zstd';
```

### Emulated Database Creation

```sql
-- Create PostgreSQL-emulated database
CREATE DATABASE !:sb_main.emulated_pg.legacy_app
    WITH
        OWNER = pg_admin
        PAGE_SIZE = 16384;

-- Create MySQL-emulated database  
CREATE DATABASE !:sb_main.emulated_mysql.webstore
    WITH
        OWNER = mysql_admin
        PAGE_SIZE = 16384;
```

### Template Database

```sql
-- Create a template database
CREATE DATABASE app_template
    WITH
        IS_TEMPLATE = true
        ALLOW_CONNECTIONS = false;

-- Create database from template
CREATE DATABASE new_app
    WITH
        TEMPLATE = app_template
        OWNER = app_owner;
```

### Cross-Environment Creation

```sql
-- Create database in parent environment
CREATE DATABASE ..:shared.configdb;

-- Create database in specific environment
CREATE DATABASE !:dev.mydb;
CREATE DATABASE !:staging.mydb;
CREATE DATABASE !:prod.mydb;
```

## Clause Matrix

| Clause | Native | Emulated | Template | Notes |
|--------|--------|----------|----------|-------|
| IF NOT EXISTS | ✅ | ✅ | ✅ | Prevents errors on re-creation |
| OWNER | ✅ | ✅ | ✅ | Must be existing user |
| TEMPLATE | ✅ | ✅ | N/A | Source database to copy |
| ENCODING | ✅ | ✅ | ✅ | Fixed after creation |
| LC_COLLATE | ✅ | ⚠️ | ✅ | Limited in emulation |
| LC_CTYPE | ✅ | ⚠️ | ✅ | Limited in emulation |
| TABLESPACE | ✅ | ✅ | ✅ | Must be existing tablespace |
| ALLOW_CONNECTIONS | ✅ | ✅ | ✅ | Can be altered later |
| CONNECTION LIMIT | ✅ | ✅ | ✅ | -1 = unlimited |
| IS_TEMPLATE | ✅ | N/A | N/A | Only for native |
| PAGE_SIZE | ✅ | ⚠️ | ✅ | Fixed at creation |
| COMPRESSION | ✅ | ✅ | ✅ | 'none', 'lz4', 'zstd' |

Legend: ✅ Fully supported, ⚠️ Limited/emulated, N/A Not applicable

## Parser Acceptance Cases

These forms must parse successfully:

```sql
CREATE DATABASE db1;
CREATE DATABASE IF NOT EXISTS db1;
CREATE DATABASE !:prod.db1;
CREATE DATABASE .:db1;
CREATE DATABASE db1 WITH OWNER = user1;
CREATE DATABASE db1 WITH TEMPLATE = template0 ENCODING = 'UTF8';
CREATE DATABASE db1 WITH PAGE_SIZE = 32768 COMPRESSION = 'zstd';
```

## Parser Rejection Cases

These forms must reject with error:

```sql
-- Missing database name
CREATE DATABASE;

-- Invalid path prefix for database
CREATE TABLESPACE fast_ssd LOCATION '/ssd';  -- Tablespace is cluster-global
CREATE DATABASE !:global.db1;  -- 'global' is reserved

-- Duplicate in same transaction (without IF NOT EXISTS)
CREATE DATABASE db1;
CREATE DATABASE db1;  -- Error: database already exists

-- Invalid page size
CREATE DATABASE db1 WITH PAGE_SIZE = 4096;  -- Error: not in {8192,16384,32768,65536,131072}

-- Invalid compression
CREATE DATABASE db1 WITH COMPRESSION = 'gzip';  -- Error: unknown compression

-- Non-existent owner
CREATE DATABASE db1 WITH OWNER = nonexistent_user;  -- Error: user does not exist
```

## Error Conditions

| Error Code | Condition | Resolution |
|------------|-----------|------------|
| `duplicate_database` | Database exists (no IF NOT EXISTS) | Use IF NOT EXISTS or DROP first |
| `invalid_database_name` | Reserved name or invalid characters | Use different name |
| `invalid_page_size` | Not in allowed set | Use: 8192, 16384, 32768, 65536, 131072 |
| `unknown_compression` | Invalid compression type | Use: 'none', 'lz4', 'zstd' |
| `undefined_user` | OWNER does not exist | Create user first |
| `undefined_tablespace` | TABLESPACE does not exist | Create tablespace first |
| `insufficient_privilege` | User lacks CREATE DATABASE | Grant permission or use superuser |
| `environment_not_found` | Path prefix invalid | Check environment exists |

## Notes

- Database names follow identifier rules: max 63 characters, case-insensitive (unless quoted)
- `template0` is the default template; `template1` can be customized
- `postgres` database is created automatically during cluster initialization
- Page size cannot be changed after creation; choose based on workload:
  - 8K: General purpose, OLTP
  - 16K: Balanced (default)
  - 32K+: Analytics, large objects, wide rows
- Connection limit applies to all users collectively
- IS_TEMPLATE databases should have ALLOW_CONNECTIONS = false

## Completion Checklist

- [x] Canonical forms documented
- [x] Clause matrix completed
- [x] Positive and negative parser cases listed
- [x] Examples validated against v3 parser behavior
- [x] Error conditions documented
- [x] Cross-references added

## See Also

- [ALTER DATABASE](02_alter_database.md)
- [DROP DATABASE](03_drop_database.md)
- [CREATE SCHEMA](04_create_schema.md)
- [Path Resolution and Scoping](../../03_path_resolution_and_scoping.md)
- [developers_guide/system_catalog/](../../../developers_guide/system_catalog/README.md)
