<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# DROP DATABASE

[Prev](./02_alter_database.md) | [Next](./04_create_schema.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1

- Test anchor: /home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_database_bootstrap.cpp:1

## Synopsis

Removes a database and all its contents from the ScratchBird environment.

## Syntax

```sql
DROP DATABASE [ IF EXISTS ] database_name [ WITH ( option [, ...] ) ]

where option can be:
    FORCE
```

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `IF EXISTS` | keyword | No | Suppress error if database does not exist |
| `database_name` | identifier | Yes | Name of database to drop. Supports path syntax. |
| `FORCE` | keyword | No | Terminate active connections and drop |

## Description

`DROP DATABASE` permanently removes a database and all objects contained within it (schemas, tables, views, functions, data). This action is irreversible.

Databases can only be dropped if:
- No active connections exist (unless `FORCE` is specified)
- The database is not a template with dependent databases
- The user has DROP permission on the database
- The database is not the current database of the session

### Drop Process

1. Validate database exists and user has DROP permission
2. Check for active connections (unless FORCE)
3. Verify no dependent templates reference this database
4. Acquire exclusive lock on database catalog entry
5. Mark database as "dropping" (prevents new connections)
6. Terminate active connections (if FORCE)
7. Drop all schemas and contained objects
8. Remove database files from storage
9. Clean up catalog entries and UUID registry
10. Release lock

## Examples

### Basic Database Drop

```sql
-- Drop database (fails if active connections exist)
DROP DATABASE myapp;

-- Drop with IF EXISTS (no error if missing)
DROP DATABASE IF EXISTS myapp;

-- Drop with explicit path
DROP DATABASE !:prod.myapp;
```

### Force Drop

```sql
-- Force drop: terminate connections and remove
DROP DATABASE myapp WITH (FORCE);

-- Combined with IF EXISTS
DROP DATABASE IF EXISTS myapp WITH (FORCE);
```

### Conditional Drop

```sql
-- Safe drop pattern for scripts
DROP DATABASE IF EXISTS temp_migration_db;
CREATE DATABASE temp_migration_db;
-- ... migration work ...
DROP DATABASE IF EXISTS temp_migration_db;
```

### Multi-Environment Cleanup

```sql
-- Clean up databases across environments
DROP DATABASE IF EXISTS !:dev.myapp;
DROP DATABASE IF EXISTS !:staging.myapp;
-- Keep prod: DROP DATABASE !:prod.myapp; -- Require explicit confirmation
```

## Parser Acceptance Cases

```sql
DROP DATABASE db1;
DROP DATABASE IF EXISTS db1;
DROP DATABASE !:prod.db1;
DROP DATABASE .:db1;
DROP DATABASE db1 WITH (FORCE);
DROP DATABASE IF EXISTS db1 WITH (FORCE);
```

## Parser Rejection Cases

```sql
-- Missing database name
DROP DATABASE;

-- Cannot drop current database
-- (Connected to 'mydb')
DROP DATABASE mydb;  -- Error: cannot drop current database

-- Cannot drop reserved databases
DROP DATABASE template0;  -- Error: reserved system database
DROP DATABASE template1;  -- Error: reserved system database
DROP DATABASE postgres;   -- Error: reserved system database

-- Cannot drop with active connections (without FORCE)
-- (Other sessions connected to 'mydb')
DROP DATABASE mydb;  -- Error: database has active connections

-- Cannot drop template with dependents
DROP DATABASE app_template;  -- Error: template has dependent databases
```

## Error Conditions

| Error Code | Condition | Resolution |
|------------|-----------|------------|
| `undefined_database` | Database does not exist (no IF EXISTS) | Use IF EXISTS or check name |
| `cannot_drop_current_database` | Attempting to drop connected database | Connect to different database first |
| `database_has_connections` | Active sessions exist (no FORCE) | Use FORCE or terminate sessions |
| `dependent_objects` | Template has dependent databases | Drop dependent databases first |
| `insufficient_privilege` | User lacks DROP permission | Use superuser or grant permission |
| `reserved_database` | Attempting to drop template0/template1/postgres | Not allowed - system databases |

## Notes

- DROP DATABASE is transactional in ScratchBird (can be rolled back if transaction aborts)
- Physical file deletion occurs at commit, not during statement execution
- FORCE option sends termination signal to backends; graceful termination attempted first
- Active transactions in target database are rolled back before termination
- System databases (`template0`, `template1`, `postgres`) cannot be dropped
- If database files are corrupted, FORCE may be required to clean up catalog entries
- Consider [BACKUP](../../backup_restore_and_admin/01_backup_commands.md) before dropping production databases

## Completion Checklist

- [x] Canonical forms documented
- [x] Clause matrix completed
- [x] Positive and negative parser cases listed
- [x] Examples validated against v3 parser behavior
- [x] Error conditions documented
- [x] Cross-references added

## See Also

- [CREATE DATABASE](01_create_database.md)
- [ALTER DATABASE](02_alter_database.md)
- [Path Resolution and Scoping](../../03_path_resolution_and_scoping.md)
