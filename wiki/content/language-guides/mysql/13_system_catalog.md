[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - System Catalog Surface

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

---

## Overview

This document covers system catalog access in MySQL emulation mode. MySQL applications query `information_schema` and the `mysql` system database for metadata. ScratchBird emulates these catalogs by providing views over its native metadata.

**Spec refs:**
- `ScratchBird/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`

---

## Catalog Namespaces

MySQL provides several system databases:

| Database | Description |
|----------|-------------|
| `information_schema` | SQL standard metadata views |
| `mysql` | System tables (users, privileges, etc.) |
| `performance_schema` | Performance metrics (optional) |
| `sys` | Views over performance_schema |

---

## information_schema Tables

### Databases and Tables

#### information_schema.SCHEMATA

List all databases.

```sql
SELECT
    SCHEMA_NAME AS database_name,
    DEFAULT_CHARACTER_SET_NAME AS charset,
    DEFAULT_COLLATION_NAME AS collation
FROM information_schema.SCHEMATA
WHERE SCHEMA_NAME NOT IN ('information_schema', 'mysql', 'performance_schema', 'sys');
```

#### information_schema.TABLES

List all tables.

```sql
SELECT
    TABLE_SCHEMA AS database_name,
    TABLE_NAME,
    TABLE_TYPE,
    ENGINE,
    TABLE_ROWS AS row_count,
    AVG_ROW_LENGTH,
    DATA_LENGTH,
    INDEX_LENGTH,
    AUTO_INCREMENT,
    CREATE_TIME,
    UPDATE_TIME,
    TABLE_COLLATION
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'mydb'
AND TABLE_TYPE = 'BASE TABLE'
ORDER BY TABLE_NAME;
```

**TABLE_TYPE values:**
- `BASE TABLE` - Regular table
- `VIEW` - View
- `SYSTEM VIEW` - System view

#### information_schema.COLUMNS

List table columns.

```sql
SELECT
    TABLE_NAME,
    COLUMN_NAME,
    ORDINAL_POSITION,
    COLUMN_DEFAULT,
    IS_NULLABLE,
    DATA_TYPE,
    CHARACTER_MAXIMUM_LENGTH,
    CHARACTER_OCTET_LENGTH,
    NUMERIC_PRECISION,
    NUMERIC_SCALE,
    DATETIME_PRECISION,
    CHARACTER_SET_NAME,
    COLLATION_NAME,
    COLUMN_TYPE,
    COLUMN_KEY,
    EXTRA,
    COLUMN_COMMENT
FROM information_schema.COLUMNS
WHERE TABLE_SCHEMA = 'mydb'
AND TABLE_NAME = 'users'
ORDER BY ORDINAL_POSITION;
```

**COLUMN_KEY values:**
- `PRI` - Primary key
- `UNI` - Unique index
- `MUL` - Non-unique index
- Empty - Not indexed

### Constraints

#### information_schema.TABLE_CONSTRAINTS

List all constraints.

```sql
SELECT
    CONSTRAINT_NAME,
    TABLE_SCHEMA,
    TABLE_NAME,
    CONSTRAINT_TYPE
FROM information_schema.TABLE_CONSTRAINTS
WHERE TABLE_SCHEMA = 'mydb'
ORDER BY TABLE_NAME, CONSTRAINT_TYPE;
```

**CONSTRAINT_TYPE values:**
- `PRIMARY KEY`
- `UNIQUE`
- `FOREIGN KEY`
- `CHECK`

#### information_schema.KEY_COLUMN_USAGE

Columns in constraints.

```sql
SELECT
    CONSTRAINT_NAME,
    TABLE_NAME,
    COLUMN_NAME,
    ORDINAL_POSITION,
    POSITION_IN_UNIQUE_CONSTRAINT,
    REFERENCED_TABLE_SCHEMA,
    REFERENCED_TABLE_NAME,
    REFERENCED_COLUMN_NAME
FROM information_schema.KEY_COLUMN_USAGE
WHERE TABLE_SCHEMA = 'mydb'
ORDER BY TABLE_NAME, CONSTRAINT_NAME, ORDINAL_POSITION;
```

#### information_schema.REFERENTIAL_CONSTRAINTS

Foreign key details.

```sql
SELECT
    CONSTRAINT_NAME,
    TABLE_NAME,
    REFERENCED_TABLE_NAME,
    UPDATE_RULE,
    DELETE_RULE,
    MATCH_OPTION
FROM information_schema.REFERENTIAL_CONSTRAINTS
WHERE CONSTRAINT_SCHEMA = 'mydb';
```

#### information_schema.CHECK_CONSTRAINTS

Check constraint definitions (MySQL 8.0+).

```sql
SELECT
    CONSTRAINT_SCHEMA,
    CONSTRAINT_NAME,
    CHECK_CLAUSE
FROM information_schema.CHECK_CONSTRAINTS
WHERE CONSTRAINT_SCHEMA = 'mydb';
```

### Indexes

#### information_schema.STATISTICS

Index information.

```sql
SELECT
    TABLE_NAME,
    INDEX_NAME,
    NON_UNIQUE,
    SEQ_IN_INDEX,
    COLUMN_NAME,
    COLLATION AS sort_order,
    CARDINALITY,
    SUB_PART AS prefix_length,
    NULLABLE,
    INDEX_TYPE,
    COMMENT,
    INDEX_COMMENT
FROM information_schema.STATISTICS
WHERE TABLE_SCHEMA = 'mydb'
ORDER BY TABLE_NAME, INDEX_NAME, SEQ_IN_INDEX;
```

### Views

#### information_schema.VIEWS

View definitions.

```sql
SELECT
    TABLE_SCHEMA,
    TABLE_NAME AS view_name,
    VIEW_DEFINITION,
    CHECK_OPTION,
    IS_UPDATABLE,
    DEFINER,
    SECURITY_TYPE
FROM information_schema.VIEWS
WHERE TABLE_SCHEMA = 'mydb';
```

### Routines

#### information_schema.ROUTINES

Stored procedures and functions.

```sql
SELECT
    ROUTINE_SCHEMA,
    ROUTINE_NAME,
    ROUTINE_TYPE,  -- 'PROCEDURE' or 'FUNCTION'
    DATA_TYPE AS return_type,
    ROUTINE_DEFINITION,
    IS_DETERMINISTIC,
    SQL_DATA_ACCESS,
    SECURITY_TYPE,
    DEFINER,
    CREATED,
    LAST_ALTERED
FROM information_schema.ROUTINES
WHERE ROUTINE_SCHEMA = 'mydb';
```

#### information_schema.PARAMETERS

Routine parameters.

```sql
SELECT
    SPECIFIC_SCHEMA,
    SPECIFIC_NAME,
    ORDINAL_POSITION,
    PARAMETER_MODE,  -- 'IN', 'OUT', 'INOUT'
    PARAMETER_NAME,
    DATA_TYPE,
    CHARACTER_MAXIMUM_LENGTH,
    NUMERIC_PRECISION,
    NUMERIC_SCALE
FROM information_schema.PARAMETERS
WHERE SPECIFIC_SCHEMA = 'mydb'
ORDER BY SPECIFIC_NAME, ORDINAL_POSITION;
```

### Triggers

#### information_schema.TRIGGERS

Trigger definitions.

```sql
SELECT
    TRIGGER_SCHEMA,
    TRIGGER_NAME,
    EVENT_MANIPULATION,  -- 'INSERT', 'UPDATE', 'DELETE'
    EVENT_OBJECT_TABLE AS table_name,
    ACTION_ORDER,
    ACTION_CONDITION,
    ACTION_STATEMENT,
    ACTION_TIMING,  -- 'BEFORE' or 'AFTER'
    CREATED,
    SQL_MODE,
    DEFINER
FROM information_schema.TRIGGERS
WHERE TRIGGER_SCHEMA = 'mydb';
```

### Privileges

#### information_schema.USER_PRIVILEGES

Global privileges.

```sql
SELECT
    GRANTEE,
    TABLE_CATALOG,
    PRIVILEGE_TYPE,
    IS_GRANTABLE
FROM information_schema.USER_PRIVILEGES
ORDER BY GRANTEE, PRIVILEGE_TYPE;
```

#### information_schema.SCHEMA_PRIVILEGES

Database-level privileges.

```sql
SELECT
    GRANTEE,
    TABLE_SCHEMA,
    PRIVILEGE_TYPE,
    IS_GRANTABLE
FROM information_schema.SCHEMA_PRIVILEGES
ORDER BY GRANTEE, TABLE_SCHEMA;
```

#### information_schema.TABLE_PRIVILEGES

Table-level privileges.

```sql
SELECT
    GRANTEE,
    TABLE_SCHEMA,
    TABLE_NAME,
    PRIVILEGE_TYPE,
    IS_GRANTABLE
FROM information_schema.TABLE_PRIVILEGES
WHERE TABLE_SCHEMA = 'mydb';
```

#### information_schema.COLUMN_PRIVILEGES

Column-level privileges.

```sql
SELECT
    GRANTEE,
    TABLE_SCHEMA,
    TABLE_NAME,
    COLUMN_NAME,
    PRIVILEGE_TYPE,
    IS_GRANTABLE
FROM information_schema.COLUMN_PRIVILEGES
WHERE TABLE_SCHEMA = 'mydb';
```

### Events

#### information_schema.EVENTS

Scheduled events.

```sql
SELECT
    EVENT_SCHEMA,
    EVENT_NAME,
    DEFINER,
    EVENT_TYPE,
    EXECUTE_AT,
    INTERVAL_VALUE,
    INTERVAL_FIELD,
    STATUS,
    ON_COMPLETION,
    CREATED,
    LAST_ALTERED,
    EVENT_DEFINITION
FROM information_schema.EVENTS
WHERE EVENT_SCHEMA = 'mydb';
```

### Partitions

#### information_schema.PARTITIONS

Table partition information.

```sql
SELECT
    TABLE_SCHEMA,
    TABLE_NAME,
    PARTITION_NAME,
    SUBPARTITION_NAME,
    PARTITION_ORDINAL_POSITION,
    PARTITION_METHOD,
    SUBPARTITION_METHOD,
    PARTITION_EXPRESSION,
    TABLE_ROWS,
    DATA_LENGTH,
    INDEX_LENGTH
FROM information_schema.PARTITIONS
WHERE TABLE_SCHEMA = 'mydb'
AND PARTITION_NAME IS NOT NULL;
```

### Character Sets

#### information_schema.CHARACTER_SETS

Available character sets.

```sql
SELECT
    CHARACTER_SET_NAME AS charset,
    DEFAULT_COLLATE_NAME AS default_collation,
    DESCRIPTION,
    MAXLEN AS max_bytes
FROM information_schema.CHARACTER_SETS
ORDER BY CHARACTER_SET_NAME;
```

#### information_schema.COLLATIONS

Available collations.

```sql
SELECT
    COLLATION_NAME,
    CHARACTER_SET_NAME AS charset,
    ID,
    IS_DEFAULT,
    IS_COMPILED,
    SORTLEN,
    PAD_ATTRIBUTE
FROM information_schema.COLLATIONS
WHERE CHARACTER_SET_NAME = 'utf8mb4'
ORDER BY COLLATION_NAME;
```

---

## mysql Database Tables

The `mysql` database contains system tables for users and privileges.

### mysql.user

User accounts and global privileges.

```sql
SELECT
    User,
    Host,
    authentication_string,
    Select_priv,
    Insert_priv,
    Update_priv,
    Delete_priv,
    Create_priv,
    Drop_priv,
    Reload_priv,
    Shutdown_priv,
    Process_priv,
    File_priv,
    Grant_priv,
    References_priv,
    Index_priv,
    Alter_priv,
    Super_priv,
    Create_tmp_table_priv,
    Lock_tables_priv,
    Execute_priv,
    Create_view_priv,
    Show_view_priv,
    Create_routine_priv,
    Alter_routine_priv,
    Create_user_priv,
    Event_priv,
    Trigger_priv,
    account_locked,
    password_expired
FROM mysql.user
WHERE User NOT LIKE 'mysql.%'
ORDER BY User, Host;
```

### mysql.db

Database-level privileges.

```sql
SELECT
    Host,
    Db,
    User,
    Select_priv,
    Insert_priv,
    Update_priv,
    Delete_priv,
    Create_priv,
    Drop_priv,
    Grant_priv,
    References_priv,
    Index_priv,
    Alter_priv
FROM mysql.db
ORDER BY Db, User;
```

### mysql.tables_priv

Table-level privileges.

```sql
SELECT
    Host,
    Db,
    User,
    Table_name,
    Grantor,
    Table_priv,
    Column_priv
FROM mysql.tables_priv
ORDER BY Db, Table_name, User;
```

### mysql.columns_priv

Column-level privileges.

```sql
SELECT
    Host,
    Db,
    User,
    Table_name,
    Column_name,
    Column_priv
FROM mysql.columns_priv
ORDER BY Db, Table_name, Column_name;
```

### mysql.procs_priv

Routine privileges.

```sql
SELECT
    Host,
    Db,
    User,
    Routine_name,
    Routine_type,
    Grantor,
    Proc_priv
FROM mysql.procs_priv
ORDER BY Db, Routine_name;
```

### mysql.role_edges

Role membership (MySQL 8.0+).

```sql
SELECT
    FROM_HOST,
    FROM_USER AS role_name,
    TO_HOST,
    TO_USER AS member_name,
    WITH_ADMIN_OPTION
FROM mysql.role_edges;
```

### mysql.default_roles

Default roles for users (MySQL 8.0+).

```sql
SELECT
    HOST,
    USER,
    DEFAULT_ROLE_HOST,
    DEFAULT_ROLE_USER AS default_role
FROM mysql.default_roles;
```

---

## Common Queries

### List All Tables with Column Count

```sql
SELECT
    t.TABLE_NAME,
    t.TABLE_TYPE,
    t.ENGINE,
    t.TABLE_ROWS,
    COUNT(c.COLUMN_NAME) AS column_count
FROM information_schema.TABLES t
JOIN information_schema.COLUMNS c USING (TABLE_SCHEMA, TABLE_NAME)
WHERE t.TABLE_SCHEMA = 'mydb'
GROUP BY t.TABLE_NAME, t.TABLE_TYPE, t.ENGINE, t.TABLE_ROWS
ORDER BY t.TABLE_NAME;
```

### List Foreign Key Relationships

```sql
SELECT
    tc.TABLE_NAME AS from_table,
    kcu.COLUMN_NAME AS from_column,
    kcu.REFERENCED_TABLE_NAME AS to_table,
    kcu.REFERENCED_COLUMN_NAME AS to_column,
    rc.UPDATE_RULE,
    rc.DELETE_RULE
FROM information_schema.TABLE_CONSTRAINTS tc
JOIN information_schema.KEY_COLUMN_USAGE kcu USING (CONSTRAINT_SCHEMA, CONSTRAINT_NAME, TABLE_NAME)
JOIN information_schema.REFERENTIAL_CONSTRAINTS rc USING (CONSTRAINT_SCHEMA, CONSTRAINT_NAME)
WHERE tc.TABLE_SCHEMA = 'mydb'
AND tc.CONSTRAINT_TYPE = 'FOREIGN KEY';
```

### List Indexes with Columns

```sql
SELECT
    TABLE_NAME,
    INDEX_NAME,
    GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) AS columns,
    CASE WHEN NON_UNIQUE = 0 THEN 'YES' ELSE 'NO' END AS is_unique,
    INDEX_TYPE
FROM information_schema.STATISTICS
WHERE TABLE_SCHEMA = 'mydb'
GROUP BY TABLE_NAME, INDEX_NAME, NON_UNIQUE, INDEX_TYPE
ORDER BY TABLE_NAME, INDEX_NAME;
```

### Find Tables Without Primary Key

```sql
SELECT t.TABLE_NAME
FROM information_schema.TABLES t
LEFT JOIN information_schema.TABLE_CONSTRAINTS tc
    ON t.TABLE_SCHEMA = tc.TABLE_SCHEMA
    AND t.TABLE_NAME = tc.TABLE_NAME
    AND tc.CONSTRAINT_TYPE = 'PRIMARY KEY'
WHERE t.TABLE_SCHEMA = 'mydb'
AND t.TABLE_TYPE = 'BASE TABLE'
AND tc.CONSTRAINT_NAME IS NULL;
```

### Find Unused Indexes

```sql
-- Note: Requires performance_schema
SELECT
    s.TABLE_SCHEMA,
    s.TABLE_NAME,
    s.INDEX_NAME,
    s.COLUMN_NAME
FROM information_schema.STATISTICS s
LEFT JOIN performance_schema.table_io_waits_summary_by_index_usage p
    ON s.TABLE_SCHEMA = p.OBJECT_SCHEMA
    AND s.TABLE_NAME = p.OBJECT_NAME
    AND s.INDEX_NAME = p.INDEX_NAME
WHERE s.TABLE_SCHEMA = 'mydb'
AND (p.COUNT_STAR IS NULL OR p.COUNT_STAR = 0)
AND s.INDEX_NAME != 'PRIMARY';
```

### Check User Privileges

```sql
-- All privileges for user
SELECT
    PRIVILEGE_TYPE,
    IS_GRANTABLE
FROM information_schema.USER_PRIVILEGES
WHERE GRANTEE = "'john'@'localhost'"

UNION ALL

SELECT
    CONCAT(TABLE_SCHEMA, '.*: ', PRIVILEGE_TYPE),
    IS_GRANTABLE
FROM information_schema.SCHEMA_PRIVILEGES
WHERE GRANTEE = "'john'@'localhost'"

UNION ALL

SELECT
    CONCAT(TABLE_SCHEMA, '.', TABLE_NAME, ': ', PRIVILEGE_TYPE),
    IS_GRANTABLE
FROM information_schema.TABLE_PRIVILEGES
WHERE GRANTEE = "'john'@'localhost'";
```

### Database Size

```sql
SELECT
    TABLE_SCHEMA AS database_name,
    COUNT(*) AS table_count,
    SUM(TABLE_ROWS) AS total_rows,
    SUM(DATA_LENGTH) / 1024 / 1024 AS data_size_mb,
    SUM(INDEX_LENGTH) / 1024 / 1024 AS index_size_mb,
    SUM(DATA_LENGTH + INDEX_LENGTH) / 1024 / 1024 AS total_size_mb
FROM information_schema.TABLES
WHERE TABLE_SCHEMA NOT IN ('information_schema', 'mysql', 'performance_schema', 'sys')
GROUP BY TABLE_SCHEMA
ORDER BY total_size_mb DESC;
```

### Table Size

```sql
SELECT
    TABLE_NAME,
    TABLE_ROWS AS row_count,
    ROUND(DATA_LENGTH / 1024 / 1024, 2) AS data_mb,
    ROUND(INDEX_LENGTH / 1024 / 1024, 2) AS index_mb,
    ROUND((DATA_LENGTH + INDEX_LENGTH) / 1024 / 1024, 2) AS total_mb
FROM information_schema.TABLES
WHERE TABLE_SCHEMA = 'mydb'
AND TABLE_TYPE = 'BASE TABLE'
ORDER BY (DATA_LENGTH + INDEX_LENGTH) DESC;
```

---

## performance_schema

Performance metrics (optional, may have limited support).

### Threads

```sql
SELECT
    THREAD_ID,
    NAME,
    TYPE,
    PROCESSLIST_ID,
    PROCESSLIST_USER,
    PROCESSLIST_HOST,
    PROCESSLIST_DB,
    PROCESSLIST_COMMAND,
    PROCESSLIST_TIME,
    PROCESSLIST_STATE,
    PROCESSLIST_INFO
FROM performance_schema.threads
WHERE TYPE = 'FOREGROUND';
```

### Wait Events

```sql
SELECT
    EVENT_NAME,
    COUNT_STAR AS count,
    SUM_TIMER_WAIT / 1000000000 AS total_wait_ms
FROM performance_schema.events_waits_summary_global_by_event_name
WHERE COUNT_STAR > 0
ORDER BY SUM_TIMER_WAIT DESC
LIMIT 20;
```

### Table I/O

```sql
SELECT
    OBJECT_SCHEMA,
    OBJECT_NAME,
    COUNT_READ,
    COUNT_WRITE,
    COUNT_FETCH,
    COUNT_INSERT,
    COUNT_UPDATE,
    COUNT_DELETE
FROM performance_schema.table_io_waits_summary_by_table
WHERE OBJECT_SCHEMA = 'mydb'
ORDER BY COUNT_READ + COUNT_WRITE DESC;
```

---

## Known Limitations

### Current Implementation Status

| Catalog | Status | Notes |
|---------|--------|-------|
| information_schema.SCHEMATA | Implemented | Works correctly |
| information_schema.TABLES | Implemented | Works correctly |
| information_schema.COLUMNS | Implemented | Works correctly |
| information_schema.STATISTICS | Partial | Basic index info |
| information_schema.TABLE_CONSTRAINTS | Partial | PK/FK/UNIQUE |
| information_schema.KEY_COLUMN_USAGE | Partial | Basic support |
| information_schema.REFERENTIAL_CONSTRAINTS | Partial | Basic FK info |
| information_schema.VIEWS | Partial | View definitions |
| information_schema.ROUTINES | Stubbed | Limited support |
| information_schema.TRIGGERS | Stubbed | Limited support |
| information_schema.*_PRIVILEGES | Stubbed | Not populated |
| mysql.user | Missing | Not emulated |
| mysql.db | Missing | Not emulated |
| mysql.* privilege tables | Missing | Not emulated |
| performance_schema.* | Stubbed | Not collecting metrics |

### Specific Issues

**Missing or Incomplete:**
- Privilege tables in mysql.* database not emulated
- performance_schema returns empty or dummy data
- Statistics columns (CARDINALITY, etc.) may be inaccurate
- ENGINE always shows 'ScratchBird' instead of InnoDB/MyISAM
- Some metadata columns return NULL

### Workarounds

**For missing mysql.* tables:** Use SHOW GRANTS instead of querying mysql.user:
```sql
-- Instead of querying mysql.user
SHOW GRANTS FOR 'app_user'@'%';
```

**For missing statistics:** Use native ScratchBird system tables:
```sql
-- Native table statistics
SELECT * FROM sb_catalog.table_stats WHERE table_name = 'users';
```

---

## See Also

- [Utilities](11_utilities.md) - SHOW commands
- [Security DCL](09_security_dcl.md) - User management
- [PostgreSQL System Catalog](../postgresql/13_system_catalog.md) - PostgreSQL equivalent
- [Tables and Constraints](02_tables_and_constraints.md) - DDL

