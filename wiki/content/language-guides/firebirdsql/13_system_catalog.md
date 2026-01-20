[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - System Catalog Surface

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`

---

## Overview

Firebird stores database metadata in system tables that all begin with specific prefixes:

- **RDB$** - Core metadata (tables, columns, constraints, indexes)
- **MON$** - Monitoring tables (connections, statements, performance)
- **SEC$** - Security tables (users, roles in security database)

These system tables can be queried like regular tables to discover database structure and monitor performance.

**Important**: In ScratchBird's Firebird emulation, these catalogs are implemented as views over ScratchBird's internal metadata. Coverage is partial and under ongoing parity audit.

---

## Catalog Namespaces

### RDB$ Tables (Core Metadata)

Core database metadata including tables, columns, indexes, procedures, and constraints.

### MON$ Tables (Monitoring)

Runtime information about connections, transactions, statements, and resource usage.

### SEC$ Tables (Security)

User and role information stored in the security database. These are typically accessed through a separate security database connection.

---

## RDB$ System Tables

### RDB$DATABASE

Contains database-wide settings.

```sql
SELECT * FROM RDB$DATABASE;
```

| Column | Description |
|--------|-------------|
| RDB$DESCRIPTION | Database description blob |
| RDB$RELATION_ID | Next relation ID |
| RDB$SECURITY_CLASS | Security class for the database |
| RDB$CHARACTER_SET_NAME | Default character set |

### RDB$RELATIONS (Tables and Views)

Information about all tables and views.

```sql
-- List all user tables
SELECT
    RDB$RELATION_NAME AS table_name,
    RDB$RELATION_TYPE AS type,
    RDB$DESCRIPTION AS description
FROM RDB$RELATIONS
WHERE RDB$SYSTEM_FLAG = 0
  AND RDB$RELATION_TYPE = 0
ORDER BY RDB$RELATION_NAME;
```

| Column | Description |
|--------|-------------|
| RDB$RELATION_NAME | Table/view name |
| RDB$RELATION_ID | Internal relation ID |
| RDB$SYSTEM_FLAG | 0 = user, 1 = system |
| RDB$RELATION_TYPE | 0 = table, 1 = view, 2 = external, 3 = virtual, 4 = GTT preserve, 5 = GTT delete |
| RDB$VIEW_BLR | View definition in BLR |
| RDB$VIEW_SOURCE | View SQL source text |
| RDB$DESCRIPTION | Table/view description |
| RDB$EXTERNAL_FILE | External table file path |
| RDB$OWNER_NAME | Owner username |

```sql
-- List all views
SELECT
    RDB$RELATION_NAME AS view_name,
    RDB$VIEW_SOURCE AS definition
FROM RDB$RELATIONS
WHERE RDB$SYSTEM_FLAG = 0
  AND RDB$VIEW_SOURCE IS NOT NULL
ORDER BY RDB$RELATION_NAME;
```

### RDB$RELATION_FIELDS (Columns)

Information about columns in tables and views.

```sql
-- List columns for a specific table
SELECT
    RDB$FIELD_NAME AS column_name,
    RDB$FIELD_POSITION AS position,
    RDB$NULL_FLAG AS nullable,
    RDB$DEFAULT_SOURCE AS default_value
FROM RDB$RELATION_FIELDS
WHERE RDB$RELATION_NAME = 'EMPLOYEES'
ORDER BY RDB$FIELD_POSITION;
```

| Column | Description |
|--------|-------------|
| RDB$RELATION_NAME | Table/view name |
| RDB$FIELD_NAME | Column name |
| RDB$FIELD_SOURCE | Domain name (or auto-generated for inline types) |
| RDB$FIELD_POSITION | Column ordinal position (0-based) |
| RDB$QUERY_NAME | Alias for queries |
| RDB$BASE_FIELD | Base field for views |
| RDB$UPDATE_FLAG | Column is updateable |
| RDB$FIELD_ID | Internal field ID |
| RDB$DESCRIPTION | Column description |
| RDB$DEFAULT_VALUE | Default value in BLR |
| RDB$DEFAULT_SOURCE | Default value SQL source |
| RDB$NULL_FLAG | 1 = NOT NULL, NULL = nullable |
| RDB$COLLATION_ID | Collation ID |

### RDB$FIELDS (Domains and Field Types)

Definitions of domains and field types.

```sql
-- List all user-defined domains
SELECT
    RDB$FIELD_NAME AS domain_name,
    RDB$FIELD_TYPE AS type_code,
    RDB$FIELD_LENGTH AS length,
    RDB$FIELD_SCALE AS scale,
    RDB$NULL_FLAG AS not_null,
    RDB$DEFAULT_SOURCE AS default_value,
    RDB$VALIDATION_SOURCE AS check_constraint
FROM RDB$FIELDS
WHERE RDB$FIELD_NAME NOT STARTING WITH 'RDB$'
  AND RDB$FIELD_NAME NOT STARTING WITH 'SEC$'
  AND RDB$FIELD_NAME NOT STARTING WITH 'MON$'
ORDER BY RDB$FIELD_NAME;
```

| Column | Description |
|--------|-------------|
| RDB$FIELD_NAME | Domain or auto-generated field name |
| RDB$FIELD_TYPE | Data type code |
| RDB$FIELD_SUB_TYPE | Blob subtype or numeric subtype |
| RDB$FIELD_LENGTH | Length in bytes |
| RDB$FIELD_SCALE | Decimal scale (negative for decimals) |
| RDB$FIELD_PRECISION | Numeric precision |
| RDB$CHARACTER_SET_ID | Character set ID |
| RDB$COLLATION_ID | Collation ID |
| RDB$SEGMENT_LENGTH | Blob segment length |
| RDB$DEFAULT_VALUE | Default value in BLR |
| RDB$DEFAULT_SOURCE | Default value SQL source |
| RDB$VALIDATION_BLR | CHECK constraint in BLR |
| RDB$VALIDATION_SOURCE | CHECK constraint SQL source |
| RDB$NULL_FLAG | 1 = NOT NULL |

**Field Type Codes:**

| Code | Type |
|------|------|
| 7 | SMALLINT |
| 8 | INTEGER |
| 10 | FLOAT |
| 12 | DATE |
| 13 | TIME |
| 14 | CHAR |
| 16 | BIGINT |
| 27 | DOUBLE PRECISION |
| 35 | TIMESTAMP |
| 37 | VARCHAR |
| 261 | BLOB |
| 23 | BOOLEAN |

### RDB$INDICES (Indexes)

Index definitions.

```sql
-- List all user indexes
SELECT
    RDB$INDEX_NAME AS index_name,
    RDB$RELATION_NAME AS table_name,
    RDB$UNIQUE_FLAG AS is_unique,
    RDB$INDEX_TYPE AS is_descending,
    RDB$INDEX_INACTIVE AS is_inactive
FROM RDB$INDICES
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$RELATION_NAME, RDB$INDEX_NAME;
```

| Column | Description |
|--------|-------------|
| RDB$INDEX_NAME | Index name |
| RDB$RELATION_NAME | Table name |
| RDB$INDEX_ID | Internal index ID |
| RDB$UNIQUE_FLAG | 1 = unique |
| RDB$DESCRIPTION | Index description |
| RDB$SEGMENT_COUNT | Number of columns |
| RDB$INDEX_INACTIVE | 1 = inactive |
| RDB$INDEX_TYPE | 0 = ascending, 1 = descending |
| RDB$FOREIGN_KEY | Foreign key constraint name |
| RDB$EXPRESSION_BLR | Expression index in BLR |
| RDB$EXPRESSION_SOURCE | Expression index SQL source |
| RDB$STATISTICS | Index selectivity |

### RDB$INDEX_SEGMENTS (Index Columns)

Columns that make up an index.

```sql
-- List columns in an index
SELECT
    RDB$FIELD_NAME AS column_name,
    RDB$FIELD_POSITION AS position
FROM RDB$INDEX_SEGMENTS
WHERE RDB$INDEX_NAME = 'PK_EMPLOYEES'
ORDER BY RDB$FIELD_POSITION;
```

| Column | Description |
|--------|-------------|
| RDB$INDEX_NAME | Index name |
| RDB$FIELD_NAME | Column name |
| RDB$FIELD_POSITION | Position in index (0-based) |

### RDB$RELATION_CONSTRAINTS (Constraints)

Table constraints (PK, FK, UNIQUE, CHECK).

```sql
-- List all constraints on a table
SELECT
    RDB$CONSTRAINT_NAME AS constraint_name,
    RDB$CONSTRAINT_TYPE AS constraint_type,
    RDB$INDEX_NAME AS index_name
FROM RDB$RELATION_CONSTRAINTS
WHERE RDB$RELATION_NAME = 'EMPLOYEES'
ORDER BY RDB$CONSTRAINT_NAME;
```

| Column | Description |
|--------|-------------|
| RDB$CONSTRAINT_NAME | Constraint name |
| RDB$CONSTRAINT_TYPE | PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK, NOT NULL |
| RDB$RELATION_NAME | Table name |
| RDB$INDEX_NAME | Associated index (for PK, FK, UNIQUE) |
| RDB$DEFERRABLE | YES/NO |
| RDB$INITIALLY_DEFERRED | YES/NO |

### RDB$REF_CONSTRAINTS (Foreign Key Details)

Foreign key references.

```sql
-- Get foreign key details
SELECT
    rc.RDB$CONSTRAINT_NAME AS fk_name,
    rc.RDB$RELATION_NAME AS table_name,
    ref.RDB$CONST_NAME_UQ AS referenced_constraint,
    ref.RDB$UPDATE_RULE AS on_update,
    ref.RDB$DELETE_RULE AS on_delete
FROM RDB$RELATION_CONSTRAINTS rc
JOIN RDB$REF_CONSTRAINTS ref
    ON rc.RDB$CONSTRAINT_NAME = ref.RDB$CONSTRAINT_NAME
WHERE rc.RDB$CONSTRAINT_TYPE = 'FOREIGN KEY';
```

| Column | Description |
|--------|-------------|
| RDB$CONSTRAINT_NAME | Foreign key constraint name |
| RDB$CONST_NAME_UQ | Referenced constraint (usually PK) |
| RDB$UPDATE_RULE | CASCADE, SET NULL, SET DEFAULT, NO ACTION, RESTRICT |
| RDB$DELETE_RULE | CASCADE, SET NULL, SET DEFAULT, NO ACTION, RESTRICT |

### RDB$CHECK_CONSTRAINTS (Check Constraint Sources)

CHECK and NOT NULL constraint expressions.

```sql
-- Get CHECK constraint expressions
SELECT
    RDB$CONSTRAINT_NAME AS constraint_name,
    RDB$TRIGGER_NAME AS trigger_name
FROM RDB$CHECK_CONSTRAINTS
WHERE RDB$CONSTRAINT_NAME = 'CHK_SALARY_POSITIVE';
```

### RDB$GENERATORS (Sequences)

Sequence/generator definitions.

```sql
-- List all user sequences
SELECT
    RDB$GENERATOR_NAME AS sequence_name,
    RDB$INITIAL_VALUE AS initial_value,
    RDB$GENERATOR_INCREMENT AS increment
FROM RDB$GENERATORS
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$GENERATOR_NAME;
```

| Column | Description |
|--------|-------------|
| RDB$GENERATOR_NAME | Sequence/generator name |
| RDB$GENERATOR_ID | Internal ID |
| RDB$SYSTEM_FLAG | 0 = user, 1 = system |
| RDB$DESCRIPTION | Description |
| RDB$INITIAL_VALUE | Starting value |
| RDB$GENERATOR_INCREMENT | Increment value |

### RDB$PROCEDURES (Stored Procedures)

Stored procedure definitions.

```sql
-- List all stored procedures
SELECT
    RDB$PROCEDURE_NAME AS procedure_name,
    RDB$PROCEDURE_TYPE AS type,
    RDB$PROCEDURE_SOURCE AS source
FROM RDB$PROCEDURES
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$PROCEDURE_NAME;
```

| Column | Description |
|--------|-------------|
| RDB$PROCEDURE_NAME | Procedure name |
| RDB$PROCEDURE_ID | Internal ID |
| RDB$PROCEDURE_INPUTS | Number of input parameters |
| RDB$PROCEDURE_OUTPUTS | Number of output parameters |
| RDB$PROCEDURE_TYPE | 1 = legacy, 2 = selectable |
| RDB$PROCEDURE_BLR | Compiled procedure in BLR |
| RDB$PROCEDURE_SOURCE | Procedure source code |
| RDB$DESCRIPTION | Description |
| RDB$OWNER_NAME | Owner username |

### RDB$PROCEDURE_PARAMETERS (Procedure Parameters)

```sql
-- List procedure parameters
SELECT
    RDB$PARAMETER_NAME AS param_name,
    RDB$PARAMETER_NUMBER AS position,
    RDB$PARAMETER_TYPE AS direction
FROM RDB$PROCEDURE_PARAMETERS
WHERE RDB$PROCEDURE_NAME = 'MY_PROCEDURE'
ORDER BY RDB$PARAMETER_TYPE, RDB$PARAMETER_NUMBER;
```

| Column | Description |
|--------|-------------|
| RDB$PARAMETER_NAME | Parameter name |
| RDB$PROCEDURE_NAME | Procedure name |
| RDB$PARAMETER_NUMBER | Position (0-based) |
| RDB$PARAMETER_TYPE | 0 = input, 1 = output |
| RDB$FIELD_SOURCE | Domain/field type |
| RDB$DESCRIPTION | Description |
| RDB$NULL_FLAG | 1 = NOT NULL |
| RDB$DEFAULT_SOURCE | Default value SQL |

### RDB$FUNCTIONS (User-Defined Functions)

```sql
-- List all UDFs
SELECT
    RDB$FUNCTION_NAME AS function_name,
    RDB$FUNCTION_TYPE AS type,
    RDB$MODULE_NAME AS module
FROM RDB$FUNCTIONS
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$FUNCTION_NAME;
```

### RDB$TRIGGERS (Triggers)

```sql
-- List all triggers
SELECT
    RDB$TRIGGER_NAME AS trigger_name,
    RDB$RELATION_NAME AS table_name,
    RDB$TRIGGER_TYPE AS trigger_type,
    RDB$TRIGGER_SEQUENCE AS sequence,
    RDB$TRIGGER_INACTIVE AS is_inactive
FROM RDB$TRIGGERS
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$RELATION_NAME, RDB$TRIGGER_SEQUENCE;
```

| Column | Description |
|--------|-------------|
| RDB$TRIGGER_NAME | Trigger name |
| RDB$RELATION_NAME | Table name |
| RDB$TRIGGER_SEQUENCE | Firing order (lower = earlier) |
| RDB$TRIGGER_TYPE | Encoded trigger timing and event |
| RDB$TRIGGER_SOURCE | Trigger source code |
| RDB$TRIGGER_BLR | Compiled trigger in BLR |
| RDB$TRIGGER_INACTIVE | 1 = inactive |
| RDB$FLAGS | Trigger flags |

**Trigger Type Encoding:**
- Phase: 1 = BEFORE, 2 = AFTER, 3 = DATABASE
- Event: 1 = INSERT, 2 = UPDATE, 3 = DELETE
- Formula: phase + (event - 1) * 2 - 1

### RDB$DEPENDENCIES (Object Dependencies)

```sql
-- Find what depends on a table
SELECT
    RDB$DEPENDENT_NAME AS dependent_object,
    RDB$DEPENDENT_TYPE AS object_type,
    RDB$FIELD_NAME AS column_name
FROM RDB$DEPENDENCIES
WHERE RDB$DEPENDED_ON_NAME = 'EMPLOYEES'
ORDER BY RDB$DEPENDENT_NAME;
```

### RDB$EXCEPTIONS (User-Defined Exceptions)

```sql
-- List all user exceptions
SELECT
    RDB$EXCEPTION_NAME AS exception_name,
    RDB$EXCEPTION_NUMBER AS number,
    RDB$MESSAGE AS message
FROM RDB$EXCEPTIONS
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$EXCEPTION_NAME;
```

### RDB$CHARACTER_SETS

```sql
-- List available character sets
SELECT
    RDB$CHARACTER_SET_NAME AS charset_name,
    RDB$CHARACTER_SET_ID AS id,
    RDB$BYTES_PER_CHARACTER AS bytes_per_char
FROM RDB$CHARACTER_SETS
ORDER BY RDB$CHARACTER_SET_NAME;
```

### RDB$COLLATIONS

```sql
-- List collations for a character set
SELECT
    RDB$COLLATION_NAME AS collation_name,
    RDB$CHARACTER_SET_ID AS charset_id
FROM RDB$COLLATIONS
WHERE RDB$CHARACTER_SET_ID = (
    SELECT RDB$CHARACTER_SET_ID FROM RDB$CHARACTER_SETS
    WHERE RDB$CHARACTER_SET_NAME = 'UTF8'
)
ORDER BY RDB$COLLATION_NAME;
```

---

## MON$ Monitoring Tables

### MON$DATABASE

Database-wide runtime information.

```sql
SELECT
    MON$DATABASE_NAME AS db_name,
    MON$PAGE_SIZE AS page_size,
    MON$ODS_MAJOR || '.' || MON$ODS_MINOR AS ods_version,
    MON$OLDEST_TRANSACTION AS oldest_trans,
    MON$NEXT_TRANSACTION AS next_trans,
    MON$PAGE_BUFFERS AS cache_pages,
    MON$SQL_DIALECT AS sql_dialect,
    MON$CREATION_DATE AS created
FROM MON$DATABASE;
```

### MON$ATTACHMENTS (Connections)

Information about active connections.

```sql
SELECT
    MON$ATTACHMENT_ID AS id,
    MON$ATTACHMENT_NAME AS connection_string,
    MON$USER AS username,
    MON$ROLE AS role,
    MON$REMOTE_ADDRESS AS remote_address,
    MON$REMOTE_PROCESS AS remote_process,
    MON$TIMESTAMP AS connected_at,
    MON$STATE AS state
FROM MON$ATTACHMENTS;
```

| Column | Description |
|--------|-------------|
| MON$ATTACHMENT_ID | Connection ID |
| MON$ATTACHMENT_NAME | Connection string/database path |
| MON$USER | Username |
| MON$ROLE | Active role |
| MON$REMOTE_ADDRESS | Client IP address |
| MON$REMOTE_PID | Client process ID |
| MON$REMOTE_PROCESS | Client process name |
| MON$TIMESTAMP | Connection time |
| MON$STATE | 0 = idle, 1 = active |

### MON$TRANSACTIONS

Active transactions.

```sql
SELECT
    MON$TRANSACTION_ID AS trans_id,
    MON$ATTACHMENT_ID AS connection_id,
    MON$STATE AS state,
    MON$TIMESTAMP AS started,
    MON$ISOLATION_MODE AS isolation
FROM MON$TRANSACTIONS
ORDER BY MON$TRANSACTION_ID;
```

| Column | Description |
|--------|-------------|
| MON$TRANSACTION_ID | Transaction ID |
| MON$ATTACHMENT_ID | Connection ID |
| MON$STATE | 0 = idle, 1 = active |
| MON$TIMESTAMP | Start time |
| MON$TOP_TRANSACTION | Top transaction for snapshot |
| MON$OLDEST_TRANSACTION | Oldest interesting transaction |
| MON$OLDEST_ACTIVE | Oldest active transaction |
| MON$ISOLATION_MODE | 0 = consistency, 1 = concurrency, 2 = read committed rec ver, 3 = read committed no rec ver |
| MON$LOCK_TIMEOUT | Lock timeout in seconds |
| MON$READ_ONLY | 1 = read-only |
| MON$AUTO_COMMIT | 1 = auto-commit |
| MON$AUTO_UNDO | 1 = auto-undo |

### MON$STATEMENTS

Currently executing statements.

```sql
SELECT
    MON$STATEMENT_ID AS stmt_id,
    MON$ATTACHMENT_ID AS connection_id,
    MON$TRANSACTION_ID AS trans_id,
    MON$STATE AS state,
    MON$TIMESTAMP AS started,
    MON$SQL_TEXT AS sql_text
FROM MON$STATEMENTS
WHERE MON$STATE = 1;  -- Active statements
```

| Column | Description |
|--------|-------------|
| MON$STATEMENT_ID | Statement ID |
| MON$ATTACHMENT_ID | Connection ID |
| MON$TRANSACTION_ID | Transaction ID |
| MON$STATE | 0 = idle, 1 = active, 2 = stalled |
| MON$TIMESTAMP | Start time |
| MON$SQL_TEXT | SQL text |

### MON$CALL_STACK

Procedure/trigger call stack.

```sql
SELECT
    MON$CALL_ID AS call_id,
    MON$STATEMENT_ID AS stmt_id,
    MON$OBJECT_NAME AS object_name,
    MON$OBJECT_TYPE AS object_type,
    MON$TIMESTAMP AS started,
    MON$SOURCE_LINE AS line,
    MON$SOURCE_COLUMN AS column
FROM MON$CALL_STACK
ORDER BY MON$CALL_ID;
```

### MON$IO_STATS

I/O statistics by attachment, transaction, or statement.

```sql
SELECT
    MON$STAT_ID AS stat_id,
    MON$STAT_GROUP AS group_type,
    MON$PAGE_READS AS pages_read,
    MON$PAGE_WRITES AS pages_written,
    MON$PAGE_FETCHES AS page_fetches,
    MON$PAGE_MARKS AS page_marks
FROM MON$IO_STATS;
```

### MON$RECORD_STATS

Record-level statistics.

```sql
SELECT
    MON$STAT_ID AS stat_id,
    MON$STAT_GROUP AS group_type,
    MON$RECORD_SEQ_READS AS seq_reads,
    MON$RECORD_IDX_READS AS idx_reads,
    MON$RECORD_INSERTS AS inserts,
    MON$RECORD_UPDATES AS updates,
    MON$RECORD_DELETES AS deletes,
    MON$RECORD_BACKOUTS AS backouts,
    MON$RECORD_PURGES AS purges,
    MON$RECORD_EXPUNGES AS expunges
FROM MON$RECORD_STATS;
```

### MON$MEMORY_USAGE

Memory usage statistics.

```sql
SELECT
    MON$STAT_ID AS stat_id,
    MON$STAT_GROUP AS group_type,
    MON$MEMORY_USED AS memory_used,
    MON$MEMORY_ALLOCATED AS memory_allocated,
    MON$MAX_MEMORY_USED AS max_memory_used,
    MON$MAX_MEMORY_ALLOCATED AS max_memory_allocated
FROM MON$MEMORY_USAGE;
```

---

## SEC$ Security Tables

Security tables are typically stored in the security database (security4.fdb or similar) and accessed through a separate connection.

### SEC$USERS

```sql
SELECT
    SEC$USER_NAME AS username,
    SEC$FIRST_NAME AS first_name,
    SEC$MIDDLE_NAME AS middle_name,
    SEC$LAST_NAME AS last_name,
    SEC$ACTIVE AS is_active,
    SEC$ADMIN AS is_admin
FROM SEC$USERS
ORDER BY SEC$USER_NAME;
```

### SEC$USER_ATTRIBUTES

Extended user attributes.

```sql
SELECT
    SEC$USER_NAME AS username,
    SEC$KEY AS attribute_key,
    SEC$VALUE AS attribute_value
FROM SEC$USER_ATTRIBUTES
WHERE SEC$USER_NAME = 'JSMITH';
```

### SEC$DB_CREATORS

Users allowed to create databases.

```sql
SELECT SEC$USER AS username
FROM SEC$DB_CREATORS;
```

### SEC$GLOBAL_AUTH_MAPPING

Authentication mappings.

---

## Common Metadata Queries

### List All Tables with Column Counts

```sql
SELECT
    r.RDB$RELATION_NAME AS table_name,
    COUNT(rf.RDB$FIELD_NAME) AS column_count
FROM RDB$RELATIONS r
LEFT JOIN RDB$RELATION_FIELDS rf
    ON r.RDB$RELATION_NAME = rf.RDB$RELATION_NAME
WHERE r.RDB$SYSTEM_FLAG = 0
  AND r.RDB$VIEW_BLR IS NULL
GROUP BY r.RDB$RELATION_NAME
ORDER BY r.RDB$RELATION_NAME;
```

### Get Table DDL (Simplified)

```sql
-- Get CREATE TABLE information
SELECT
    rf.RDB$FIELD_NAME AS column_name,
    CASE f.RDB$FIELD_TYPE
        WHEN 7 THEN 'SMALLINT'
        WHEN 8 THEN 'INTEGER'
        WHEN 10 THEN 'FLOAT'
        WHEN 14 THEN 'CHAR(' || f.RDB$FIELD_LENGTH || ')'
        WHEN 16 THEN 'BIGINT'
        WHEN 27 THEN 'DOUBLE PRECISION'
        WHEN 37 THEN 'VARCHAR(' || f.RDB$FIELD_LENGTH || ')'
        WHEN 12 THEN 'DATE'
        WHEN 13 THEN 'TIME'
        WHEN 35 THEN 'TIMESTAMP'
        WHEN 261 THEN 'BLOB'
        WHEN 23 THEN 'BOOLEAN'
        ELSE 'UNKNOWN'
    END AS data_type,
    CASE WHEN rf.RDB$NULL_FLAG = 1 THEN 'NOT NULL' ELSE 'NULL' END AS nullable,
    rf.RDB$DEFAULT_SOURCE AS default_value
FROM RDB$RELATION_FIELDS rf
JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
WHERE rf.RDB$RELATION_NAME = 'EMPLOYEES'
ORDER BY rf.RDB$FIELD_POSITION;
```

### List Foreign Key Relationships

```sql
SELECT
    rc.RDB$RELATION_NAME AS table_name,
    rc.RDB$CONSTRAINT_NAME AS fk_name,
    seg.RDB$FIELD_NAME AS column_name,
    ref.RDB$CONST_NAME_UQ AS references_constraint,
    rc2.RDB$RELATION_NAME AS references_table
FROM RDB$RELATION_CONSTRAINTS rc
JOIN RDB$REF_CONSTRAINTS ref
    ON rc.RDB$CONSTRAINT_NAME = ref.RDB$CONSTRAINT_NAME
JOIN RDB$INDICES idx
    ON rc.RDB$INDEX_NAME = idx.RDB$INDEX_NAME
JOIN RDB$INDEX_SEGMENTS seg
    ON idx.RDB$INDEX_NAME = seg.RDB$INDEX_NAME
JOIN RDB$RELATION_CONSTRAINTS rc2
    ON ref.RDB$CONST_NAME_UQ = rc2.RDB$CONSTRAINT_NAME
WHERE rc.RDB$CONSTRAINT_TYPE = 'FOREIGN KEY'
ORDER BY rc.RDB$RELATION_NAME, rc.RDB$CONSTRAINT_NAME;
```

### Find Tables Without Primary Key

```sql
SELECT r.RDB$RELATION_NAME AS table_name
FROM RDB$RELATIONS r
WHERE r.RDB$SYSTEM_FLAG = 0
  AND r.RDB$VIEW_BLR IS NULL
  AND NOT EXISTS (
      SELECT 1 FROM RDB$RELATION_CONSTRAINTS rc
      WHERE rc.RDB$RELATION_NAME = r.RDB$RELATION_NAME
        AND rc.RDB$CONSTRAINT_TYPE = 'PRIMARY KEY'
  )
ORDER BY r.RDB$RELATION_NAME;
```

### Active Long-Running Queries

```sql
SELECT
    s.MON$SQL_TEXT AS query,
    a.MON$USER AS username,
    a.MON$REMOTE_ADDRESS AS client_ip,
    s.MON$TIMESTAMP AS started,
    CURRENT_TIMESTAMP - s.MON$TIMESTAMP AS duration
FROM MON$STATEMENTS s
JOIN MON$ATTACHMENTS a ON s.MON$ATTACHMENT_ID = a.MON$ATTACHMENT_ID
WHERE s.MON$STATE = 1
ORDER BY s.MON$TIMESTAMP;
```

### Database Statistics Summary

```sql
SELECT
    d.MON$DATABASE_NAME AS database,
    d.MON$PAGE_SIZE AS page_size,
    d.MON$PAGES AS total_pages,
    d.MON$PAGE_SIZE * d.MON$PAGES / 1024 / 1024 AS size_mb,
    (SELECT COUNT(*) FROM MON$ATTACHMENTS) AS connections,
    (SELECT COUNT(*) FROM MON$TRANSACTIONS) AS transactions,
    (SELECT COUNT(*) FROM MON$STATEMENTS WHERE MON$STATE = 1) AS active_queries
FROM MON$DATABASE d;
```

---

## Known Limitations

### Partial Implementation

**RDB$ Tables**
- Emulated Firebird catalogs are implemented as views over ScratchBird metadata
- Coverage is under ongoing parity audit
- Some columns may return NULL or placeholder values
- Status: Partial

**MON$ Tables**
- Monitoring tables may return limited or dummy data
- Real-time statistics may not be fully populated
- Status: Partial

**SEC$ Tables**
- Security tables require security database access
- May not be implemented in emulation mode
- Status: Partial/Missing

### Specification Deltas

**Column Trimming**
- Firebird system tables use CHAR columns with trailing spaces
- You may need to TRIM() column values:
```sql
SELECT TRIM(RDB$RELATION_NAME) AS table_name FROM RDB$RELATIONS;
```

**System Flag**
- Always filter by `RDB$SYSTEM_FLAG = 0` to exclude system objects
- System objects have RDB$SYSTEM_FLAG = 1

**BLR Columns**
- Columns ending in `_BLR` contain compiled binary representations
- These are not human-readable; use `_SOURCE` columns instead

### Gaps Under Audit

The `FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` document tracks remaining RDB$/MON$/SEC$ gaps. Check this specification for the current state of catalog emulation.

---

## Quick Reference

### Common RDB$ Tables

| Table | Contents |
|-------|----------|
| RDB$RELATIONS | Tables and views |
| RDB$RELATION_FIELDS | Columns |
| RDB$FIELDS | Domains and field definitions |
| RDB$INDICES | Indexes |
| RDB$INDEX_SEGMENTS | Index columns |
| RDB$RELATION_CONSTRAINTS | Constraints |
| RDB$REF_CONSTRAINTS | Foreign key details |
| RDB$GENERATORS | Sequences |
| RDB$PROCEDURES | Stored procedures |
| RDB$TRIGGERS | Triggers |

### Common MON$ Tables

| Table | Contents |
|-------|----------|
| MON$DATABASE | Database info |
| MON$ATTACHMENTS | Connections |
| MON$TRANSACTIONS | Active transactions |
| MON$STATEMENTS | Executing statements |
| MON$IO_STATS | I/O statistics |
| MON$RECORD_STATS | Record statistics |

### Key Filters

| Filter | Purpose |
|--------|---------|
| `RDB$SYSTEM_FLAG = 0` | User objects only |
| `RDB$VIEW_BLR IS NULL` | Tables only (exclude views) |
| `RDB$VIEW_BLR IS NOT NULL` | Views only |
| `MON$STATE = 1` | Active items |

---

## See Also

- [Database Management](01_databases_and_schemas.md)
- [Tables and Constraints](02_tables_and_constraints.md)
- [Indexes, Views, Sequences](03_indexes_views_sequences.md)
- [Programmable SQL](05_programmable_sql.md)

