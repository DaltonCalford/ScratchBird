# ScratchBird System Schema & Database Layout Documentation

**Database Engine**: ScratchBird Alpha 0.6.0  
**Document Version**: 1.0  
**Date**: July 2025  
**SQL Dialect**: 4 (Enhanced) - Default  

## Table of Contents

1. [Default Database Layout](#default-database-layout)
2. [System Tables (RDB$)](#system-tables-rdb)
3. [Monitoring Tables (MON$)](#monitoring-tables-mon)
4. [Base Database Objects](#base-database-objects)
5. [System Stored Procedures](#system-stored-procedures)
6. [System Functions](#system-functions)
7. [System Sequences](#system-sequences)
8. [System Triggers](#system-triggers)
9. [Default Roles and Users](#default-roles-and-users)
10. [Character Sets and Collations](#character-sets-and-collations)
11. [System Domains](#system-domains)
12. [Metadata Queries](#metadata-queries)
13. [System Navigation](#system-navigation)
14. [Database Creation Process](#database-creation-process)

---

## Default Database Layout

### Overview
When a new ScratchBird database is created, it contains a comprehensive set of system objects that provide metadata management, monitoring capabilities, and administrative functions. The system is organized into several categories of objects.

### Default Schema Structure
When a new ScratchBird database is created, it establishes a hierarchical schema structure designed for enterprise-grade organization and security:

```sql
-- Top-level schemas (Root level - no parent schema)
CREATE SCHEMA SYSTEM;       -- Core system metadata and administration
CREATE SCHEMA USERS;        -- User home directories and personal schemas  
CREATE SCHEMA LINKS;        -- Database link management
CREATE SCHEMA DATABASE;     -- Database-specific objects and metadata
CREATE SCHEMA APPLICATIONS; -- Application-specific schemas

-- SYSTEM subschemas (hierarchical structure under SYSTEM)
CREATE SCHEMA SYSTEM.INFORMATION_SCHEMA;  -- SQL standard information schema
CREATE SCHEMA SYSTEM.HIERARCHY;           -- Schema hierarchy management
CREATE SCHEMA SYSTEM.PLG$LEGACY_SEC;      -- Legacy security plugin schema
CREATE SCHEMA SYSTEM.PLG$SRP;             -- Secure Remote Password plugin schema

-- USERS subschemas
CREATE SCHEMA USERS.PUBLIC;               -- Default public user schema
-- Additional user schemas created dynamically: USERS.<username>

-- System tables are located in the SYSTEM schema
-- Monitoring tables (MON$) are views/tables within SYSTEM schema
-- SEC$ virtual tables are provided through the SYSTEM schema
```

### Database Size and Object Count
- **Initial Database Size**: ~18-25MB (depending on page size and features enabled)
- **Default Schemas Created**: 9 schemas (5 top-level + 4 SYSTEM subschemas)
- **System Tables**: 48+ RDB$ tables in SYSTEM schema
- **Virtual Tables**: 8+ SEC$ tables (security management)
- **Monitoring Tables**: 15+ MON$ tables/views
- **System Procedures**: 25+ stored procedures
- **System Functions**: 400+ built-in functions 
- **System Triggers**: 15+ system triggers for metadata integrity
- **Default Page Size**: 8192 bytes (8KB)
- **Schema Hierarchy**: Up to 11 levels deep supported

#### Schema Object Distribution
- **SYSTEM Schema**: All RDB$ system tables, MON$ monitoring views, SEC$ virtual tables
- **SYSTEM.INFORMATION_SCHEMA**: SQL standard views (TABLES, COLUMNS, etc.)
- **SYSTEM.PLG$SRP**: Secure Remote Password authentication tables
- **USERS Schema**: User-specific schemas and home directories
- **LINKS Schema**: Database link definitions and management
- **DATABASE Schema**: Database-specific metadata and configuration
- **APPLICATIONS Schema**: Reserved for application-specific objects

---

## System Tables (RDB$)

### Core Metadata Tables

#### RDB$DATABASE
Primary database information and configuration.
```sql
CREATE TABLE RDB$DATABASE (
    RDB$DESCRIPTION BLOB,
    RDB$RELATION_ID INTEGER,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$CHARACTER_SET_NAME VARCHAR(67),
    RDB$LINGER INTEGER,
    RDB$SQL_DIALECT INTEGER,
    RDB$DATABASE_FLAGS INTEGER
);
```

**Key Fields**:
- `RDB$SQL_DIALECT`: Current SQL dialect (4 for ScratchBird)
- `RDB$CHARACTER_SET_NAME`: Default character set (usually UTF8)
- `RDB$LINGER`: Connection linger time in seconds

#### RDB$RELATIONS
All tables and views in the database.
```sql
CREATE TABLE RDB$RELATIONS (
    RDB$VIEW_BLR BLOB,
    RDB$VIEW_SOURCE BLOB,
    RDB$DESCRIPTION BLOB,
    RDB$RELATION_ID INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DBKEY_LENGTH INTEGER,
    RDB$FORMAT INTEGER,
    RDB$FIELD_ID INTEGER,
    RDB$RELATION_NAME VARCHAR(67) NOT NULL,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$EXTERNAL_FILE VARCHAR(255),
    RDB$RUNTIME_TYPE INTEGER,
    RDB$EXTERNAL_DESCRIPTION BLOB,
    RDB$OWNER_NAME VARCHAR(67),
    RDB$DEFAULT_CLASS VARCHAR(67),
    RDB$FLAGS INTEGER,
    RDB$RELATION_TYPE INTEGER
);
```

**Key Fields**:
- `RDB$RELATION_NAME`: Table/view name
- `RDB$SYSTEM_FLAG`: 0=user object, 1=system object
- `RDB$RELATION_TYPE`: 0=table, 1=view, 2=external table
- `RDB$OWNER_NAME`: Object owner

#### RDB$RELATION_FIELDS
Column definitions for all tables and views.
```sql
CREATE TABLE RDB$RELATION_FIELDS (
    RDB$FIELD_NAME VARCHAR(67) NOT NULL,
    RDB$RELATION_NAME VARCHAR(67) NOT NULL,
    RDB$FIELD_SOURCE VARCHAR(67),
    RDB$QUERY_NAME VARCHAR(67),
    RDB$BASE_FIELD VARCHAR(67),
    RDB$EDIT_STRING VARCHAR(255),
    RDB$FIELD_POSITION INTEGER,
    RDB$QUERY_HEADER VARCHAR(255),
    RDB$UPDATE_FLAG INTEGER,
    RDB$FIELD_ID INTEGER,
    RDB$VIEW_CONTEXT INTEGER,
    RDB$VALUE_ID INTEGER,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$SYSTEM_FLAG INTEGER,
    RDB$TYPE_NAME VARCHAR(67),
    RDB$COMPLEX_NAME VARCHAR(67),
    RDB$NULL_FLAG INTEGER,
    RDB$DEFAULT_VALUE BLOB,
    RDB$DEFAULT_SOURCE BLOB,
    RDB$COLLATION_ID INTEGER,
    RDB$GENERATOR_NAME VARCHAR(67),
    RDB$IDENTITY_TYPE INTEGER
);
```

**Key Fields**:
- `RDB$FIELD_NAME`: Column name
- `RDB$RELATION_NAME`: Table/view name
- `RDB$FIELD_POSITION`: Column order (0-based)
- `RDB$NULL_FLAG`: 1=NOT NULL constraint
- `RDB$IDENTITY_TYPE`: 1=BY DEFAULT, 2=ALWAYS
- `RDB$GENERATOR_NAME`: Associated sequence for identity columns

#### RDB$FIELDS
Domain and field type definitions.
```sql
CREATE TABLE RDB$FIELDS (
    RDB$FIELD_NAME VARCHAR(67) NOT NULL,
    RDB$QUERY_NAME VARCHAR(67),
    RDB$VALIDATION_BLR BLOB,
    RDB$VALIDATION_SOURCE BLOB,
    RDB$COMPUTED_BLR BLOB,
    RDB$COMPUTED_SOURCE BLOB,
    RDB$DEFAULT_VALUE BLOB,
    RDB$DEFAULT_SOURCE BLOB,
    RDB$FIELD_LENGTH INTEGER,
    RDB$FIELD_SCALE INTEGER,
    RDB$FIELD_TYPE INTEGER,
    RDB$FIELD_SUB_TYPE INTEGER,
    RDB$MISSING_VALUE BLOB,
    RDB$MISSING_SOURCE BLOB,
    RDB$DESCRIPTION BLOB,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$QUERY_HEADER VARCHAR(255),
    RDB$SEGMENT_LENGTH INTEGER,
    RDB$EDIT_STRING VARCHAR(255),
    RDB$EXTERNAL_LENGTH INTEGER,
    RDB$EXTERNAL_SCALE INTEGER,
    RDB$EXTERNAL_TYPE INTEGER,
    RDB$DIMENSIONS INTEGER,
    RDB$NULL_FLAG INTEGER,
    RDB$CHARACTER_LENGTH INTEGER,
    RDB$COLLATION_ID INTEGER,
    RDB$CHARACTER_SET_ID INTEGER,
    RDB$FIELD_PRECISION INTEGER
);
```

**Data Type Mapping**:
- `RDB$FIELD_TYPE = 7`: SMALLINT
- `RDB$FIELD_TYPE = 8`: INTEGER  
- `RDB$FIELD_TYPE = 10`: FLOAT
- `RDB$FIELD_TYPE = 12`: DATE
- `RDB$FIELD_TYPE = 13`: TIME
- `RDB$FIELD_TYPE = 14`: CHAR
- `RDB$FIELD_TYPE = 16`: BIGINT
- `RDB$FIELD_TYPE = 23`: BOOLEAN
- `RDB$FIELD_TYPE = 24`: DECFLOAT(16)
- `RDB$FIELD_TYPE = 25`: DECFLOAT(34)
- `RDB$FIELD_TYPE = 26`: INT128
- `RDB$FIELD_TYPE = 27`: DOUBLE PRECISION
- `RDB$FIELD_TYPE = 28`: TIME WITH TIME ZONE
- `RDB$FIELD_TYPE = 29`: TIMESTAMP WITH TIME ZONE
- `RDB$FIELD_TYPE = 37`: VARCHAR
- `RDB$FIELD_TYPE = 261`: BLOB

### Index and Constraint Tables

#### RDB$INDICES
Index definitions and statistics.
```sql
CREATE TABLE RDB$INDICES (
    RDB$INDEX_NAME VARCHAR(67) NOT NULL,
    RDB$RELATION_NAME VARCHAR(67),
    RDB$INDEX_ID INTEGER,
    RDB$UNIQUE_FLAG INTEGER,
    RDB$DESCRIPTION BLOB,
    RDB$SEGMENT_COUNT INTEGER,
    RDB$INDEX_INACTIVE INTEGER,
    RDB$INDEX_TYPE INTEGER,
    RDB$FOREIGN_KEY VARCHAR(67),
    RDB$SYSTEM_FLAG INTEGER,
    RDB$EXPRESSION_BLR BLOB,
    RDB$EXPRESSION_SOURCE BLOB,
    RDB$STATISTICS DOUBLE PRECISION
);
```

**Index Types**:
- `RDB$INDEX_TYPE = 0`: B-tree (ascending)
- `RDB$INDEX_TYPE = 1`: B-tree (descending)
- `RDB$INDEX_TYPE = 2`: Hash index
- `RDB$INDEX_TYPE = 3`: GIN (Generalized Inverted) index
- `RDB$INDEX_TYPE = 4`: Bitmap index
- `RDB$INDEX_TYPE = 5`: Spatial index

#### RDB$INDEX_SEGMENTS
Index column composition.
```sql
CREATE TABLE RDB$INDEX_SEGMENTS (
    RDB$INDEX_NAME VARCHAR(67) NOT NULL,
    RDB$FIELD_NAME VARCHAR(67),
    RDB$FIELD_POSITION INTEGER,
    RDB$STATISTICS DOUBLE PRECISION
);
```

#### RDB$RELATION_CONSTRAINTS
Table constraints (PK, FK, UNIQUE, CHECK).
```sql
CREATE TABLE RDB$RELATION_CONSTRAINTS (
    RDB$CONSTRAINT_NAME VARCHAR(67) NOT NULL,
    RDB$CONSTRAINT_TYPE VARCHAR(11),
    RDB$RELATION_NAME VARCHAR(67),
    RDB$DEFERRABLE VARCHAR(3),
    RDB$INITIALLY_DEFERRED VARCHAR(3),
    RDB$MATCH_OPTION VARCHAR(7),
    RDB$UPDATE_RULE VARCHAR(11),
    RDB$DELETE_RULE VARCHAR(11),
    RDB$INDEX_NAME VARCHAR(67)
);
```

**Constraint Types**:
- `PRIMARY KEY`: Primary key constraint
- `FOREIGN KEY`: Foreign key constraint
- `UNIQUE`: Unique constraint
- `CHECK`: Check constraint

#### RDB$REF_CONSTRAINTS
Foreign key constraint details.
```sql
CREATE TABLE RDB$REF_CONSTRAINTS (
    RDB$CONSTRAINT_NAME VARCHAR(67) NOT NULL,
    RDB$CONST_NAME_UQ VARCHAR(67),
    RDB$MATCH_OPTION VARCHAR(7),
    RDB$UPDATE_RULE VARCHAR(11),
    RDB$DELETE_RULE VARCHAR(11)
);
```

#### RDB$CHECK_CONSTRAINTS
Check constraint expressions.
```sql
CREATE TABLE RDB$CHECK_CONSTRAINTS (
    RDB$CONSTRAINT_NAME VARCHAR(67) NOT NULL,
    RDB$TRIGGER_NAME VARCHAR(67)
);
```

### Schema and Security Tables

#### RDB$SCHEMAS
Hierarchical schema definitions (ScratchBird extension).
```sql
CREATE TABLE RDB$SCHEMAS (
    RDB$SCHEMA_NAME VARCHAR(67) NOT NULL,
    RDB$OWNER_NAME VARCHAR(67),
    RDB$DEFAULT_CHARACTER_SET_NAME VARCHAR(67),
    RDB$SQL_SECURITY VARCHAR(12),
    RDB$DESCRIPTION BLOB,
    -- ScratchBird hierarchical schema extensions
    RDB$PARENT_SCHEMA_NAME VARCHAR(67),
    RDB$SCHEMA_PATH VARCHAR(511),
    RDB$SCHEMA_LEVEL INTEGER
);
```

**Hierarchical Fields**:
- `RDB$PARENT_SCHEMA_NAME`: Parent schema reference
- `RDB$SCHEMA_PATH`: Full dot-separated path (e.g., 'finance.accounting.reports')
- `RDB$SCHEMA_LEVEL`: Nesting depth (0 = root level)

#### RDB$USERS
Database user accounts.
```sql
CREATE TABLE RDB$USERS (
    RDB$USER_NAME VARCHAR(67) NOT NULL,
    RDB$SYS_USER_NAME VARCHAR(67),
    RDB$GROUP_NAME VARCHAR(67),
    RDB$UID INTEGER,
    RDB$GID INTEGER,
    RDB$PASSWD VARCHAR(64),
    RDB$PRIVILEGE VARCHAR(13),
    RDB$COMMENT BLOB,
    RDB$FIRST_NAME VARCHAR(32),
    RDB$MIDDLE_NAME VARCHAR(32),
    RDB$LAST_NAME VARCHAR(32),
    RDB$FULL_NAME COMPUTED BY (RDB$FIRST_NAME || ' ' || RDB$MIDDLE_NAME || ' ' || RDB$LAST_NAME),
    RDB$DEFAULT_SCHEMA VARCHAR(67),
    RDB$HOME_SCHEMA VARCHAR(67)
);
```

#### RDB$ROLES
Database roles and privileges.
```sql
CREATE TABLE RDB$ROLES (
    RDB$ROLE_NAME VARCHAR(67) NOT NULL,
    RDB$OWNER_NAME VARCHAR(67),
    RDB$DESCRIPTION BLOB,
    RDB$SYSTEM_FLAG INTEGER
);
```

#### RDB$USER_PRIVILEGES
Granted privileges to users and roles.
```sql
CREATE TABLE RDB$USER_PRIVILEGES (
    RDB$USER VARCHAR(67),
    RDB$GRANTOR VARCHAR(67),
    RDB$PRIVILEGE VARCHAR(6),
    RDB$GRANT_OPTION INTEGER,
    RDB$RELATION_NAME VARCHAR(67),
    RDB$FIELD_NAME VARCHAR(67),
    RDB$USER_TYPE INTEGER,
    RDB$OBJECT_TYPE INTEGER
);
```

### Procedural Code Tables

#### RDB$PROCEDURES
Stored procedure definitions.
```sql
CREATE TABLE RDB$PROCEDURES (
    RDB$PROCEDURE_NAME VARCHAR(67) NOT NULL,
    RDB$PROCEDURE_ID INTEGER,
    RDB$PROCEDURE_INPUTS INTEGER,
    RDB$PROCEDURE_OUTPUTS INTEGER,
    RDB$DESCRIPTION BLOB,
    RDB$PROCEDURE_SOURCE BLOB,
    RDB$PROCEDURE_BLR BLOB,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$OWNER_NAME VARCHAR(67),
    RDB$RUNTIME INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$PROCEDURE_TYPE INTEGER,
    RDB$VALID_BLR INTEGER,
    RDB$DEBUG_INFO BLOB,
    RDB$ENGINE_NAME VARCHAR(67),
    RDB$ENTRYPOINT VARCHAR(255),
    RDB$PACKAGE_NAME VARCHAR(67),
    RDB$PRIVATE_FLAG INTEGER,
    RDB$SQL_SECURITY VARCHAR(12)
);
```

#### RDB$PROCEDURE_PARAMETERS
Stored procedure parameter definitions.
```sql
CREATE TABLE RDB$PROCEDURE_PARAMETERS (
    RDB$PARAMETER_NAME VARCHAR(67) NOT NULL,
    RDB$PROCEDURE_NAME VARCHAR(67),
    RDB$PARAMETER_NUMBER INTEGER,
    RDB$PARAMETER_TYPE INTEGER,
    RDB$FIELD_SOURCE VARCHAR(67),
    RDB$DESCRIPTION BLOB,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DEFAULT_VALUE BLOB,
    RDB$DEFAULT_SOURCE BLOB,
    RDB$COLLATION_ID INTEGER,
    RDB$NULL_FLAG INTEGER,
    RDB$PARAMETER_MECHANISM INTEGER,
    RDB$FIELD_NAME VARCHAR(67),
    RDB$RELATION_NAME VARCHAR(67)
);
```

#### RDB$FUNCTIONS
User-defined function definitions.
```sql
CREATE TABLE RDB$FUNCTIONS (
    RDB$FUNCTION_NAME VARCHAR(67) NOT NULL,
    RDB$FUNCTION_TYPE INTEGER,
    RDB$QUERY_NAME VARCHAR(67),
    RDB$DESCRIPTION BLOB,
    RDB$MODULE_NAME VARCHAR(255),
    RDB$ENTRYPOINT VARCHAR(255),
    RDB$RETURN_ARGUMENT INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$ENGINE_NAME VARCHAR(67),
    RDB$PACKAGE_NAME VARCHAR(67),
    RDB$PRIVATE_FLAG INTEGER,
    RDB$FUNCTION_SOURCE BLOB,
    RDB$FUNCTION_BLR BLOB,
    RDB$VALID_BLR INTEGER,
    RDB$DEBUG_INFO BLOB,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$OWNER_NAME VARCHAR(67),
    RDB$LEGACY_FLAG INTEGER,
    RDB$DETERMINISTIC_FLAG INTEGER,
    RDB$SQL_SECURITY VARCHAR(12)
);
```

#### RDB$FUNCTION_ARGUMENTS
Function parameter and return value definitions.
```sql
CREATE TABLE RDB$FUNCTION_ARGUMENTS (
    RDB$FUNCTION_NAME VARCHAR(67) NOT NULL,
    RDB$ARGUMENT_POSITION INTEGER,
    RDB$MECHANISM INTEGER,
    RDB$FIELD_TYPE INTEGER,
    RDB$FIELD_SCALE INTEGER,
    RDB$FIELD_LENGTH INTEGER,
    RDB$FIELD_SUB_TYPE INTEGER,
    RDB$CHARACTER_SET_ID INTEGER,
    RDB$FIELD_PRECISION INTEGER,
    RDB$CHARACTER_LENGTH INTEGER,
    RDB$PACKAGE_NAME VARCHAR(67),
    RDB$ARGUMENT_NAME VARCHAR(67),
    RDB$FIELD_SOURCE VARCHAR(67),
    RDB$DEFAULT_VALUE BLOB,
    RDB$DEFAULT_SOURCE BLOB,
    RDB$COLLATION_ID INTEGER,
    RDB$NULL_FLAG INTEGER,
    RDB$ARGUMENT_MECHANISM INTEGER,
    RDB$FIELD_NAME VARCHAR(67),
    RDB$RELATION_NAME VARCHAR(67),
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DESCRIPTION BLOB
);
```

#### RDB$TRIGGERS
Database trigger definitions.
```sql
CREATE TABLE RDB$TRIGGERS (
    RDB$TRIGGER_NAME VARCHAR(67) NOT NULL,
    RDB$RELATION_NAME VARCHAR(67),
    RDB$TRIGGER_SEQUENCE INTEGER,
    RDB$TRIGGER_TYPE INTEGER,
    RDB$TRIGGER_SOURCE BLOB,
    RDB$TRIGGER_BLR BLOB,
    RDB$DESCRIPTION BLOB,
    RDB$TRIGGER_INACTIVE INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$FLAGS INTEGER,
    RDB$VALID_BLR INTEGER,
    RDB$DEBUG_INFO BLOB,
    RDB$ENGINE_NAME VARCHAR(67),
    RDB$ENTRYPOINT VARCHAR(255),
    RDB$SQL_SECURITY VARCHAR(12)
);
```

### Sequence and Generator Tables

#### RDB$GENERATORS
Sequence/generator definitions.
```sql
CREATE TABLE RDB$GENERATORS (
    RDB$GENERATOR_NAME VARCHAR(67) NOT NULL,
    RDB$GENERATOR_ID INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DESCRIPTION BLOB,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$OWNER_NAME VARCHAR(67)
);
```

#### RDB$GENERATOR_VALUES
Current sequence values (internal).
```sql
CREATE TABLE RDB$GENERATOR_VALUES (
    RDB$GENERATOR_NAME VARCHAR(67) NOT NULL,
    RDB$GENERATOR_VALUE BIGINT
);
```

### Character Set and Collation Tables

#### RDB$CHARACTER_SETS
Available character sets.
```sql
CREATE TABLE RDB$CHARACTER_SETS (
    RDB$CHARACTER_SET_NAME VARCHAR(67) NOT NULL,
    RDB$FORM_OF_USE INTEGER,
    RDB$NUMBER_OF_CHARACTERS BIGINT,
    RDB$DEFAULT_COLLATE_NAME VARCHAR(67),
    RDB$CHARACTER_SET_ID INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DESCRIPTION BLOB,
    RDB$FUNCTION_NAME VARCHAR(67),
    RDB$BYTES_PER_CHARACTER INTEGER
);
```

#### RDB$COLLATIONS
Available collation sequences.
```sql
CREATE TABLE RDB$COLLATIONS (
    RDB$COLLATION_NAME VARCHAR(67) NOT NULL,
    RDB$COLLATION_ID INTEGER,
    RDB$CHARACTER_SET_ID INTEGER,
    RDB$COLLATION_ATTRIBUTES INTEGER,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DESCRIPTION BLOB,
    RDB$FUNCTION_NAME VARCHAR(67),
    RDB$BASE_COLLATION_NAME VARCHAR(67),
    RDB$SPECIFIC_ATTRIBUTES BLOB
);
```

### Exception and Package Tables

#### RDB$EXCEPTIONS
User-defined exception definitions.
```sql
CREATE TABLE RDB$EXCEPTIONS (
    RDB$EXCEPTION_NAME VARCHAR(67) NOT NULL,
    RDB$EXCEPTION_NUMBER INTEGER,
    RDB$MESSAGE VARCHAR(1023),
    RDB$DESCRIPTION BLOB,
    RDB$SYSTEM_FLAG INTEGER,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$OWNER_NAME VARCHAR(67)
);
```

#### RDB$PACKAGES
Package definitions.
```sql
CREATE TABLE RDB$PACKAGES (
    RDB$PACKAGE_NAME VARCHAR(67) NOT NULL,
    RDB$PACKAGE_HEADER_SOURCE BLOB,
    RDB$PACKAGE_BODY_SOURCE BLOB,
    RDB$VALID_BODY_FLAG INTEGER,
    RDB$SECURITY_CLASS VARCHAR(67),
    RDB$OWNER_NAME VARCHAR(67),
    RDB$SYSTEM_FLAG INTEGER,
    RDB$DESCRIPTION BLOB,
    RDB$SQL_SECURITY VARCHAR(12)
);
```

### Database Link Tables (ScratchBird Extension)

#### RDB$DATABASE_LINKS
Schema-aware database link definitions.
```sql
CREATE TABLE RDB$DATABASE_LINKS (
    RDB$LINK_NAME VARCHAR(67) NOT NULL,
    RDB$LINK_SERVER VARCHAR(255),
    RDB$LINK_PROTOCOL VARCHAR(20),
    RDB$LINK_USERNAME VARCHAR(67),
    RDB$LINK_PASSWORD VARCHAR(255),
    RDB$LINK_OPTIONS BLOB,
    RDB$LINK_DESCRIPTION BLOB,
    RDB$OWNER_NAME VARCHAR(67),
    RDB$SYSTEM_FLAG INTEGER,
    -- ScratchBird schema-aware extensions
    RDB$LINK_SCHEMA_NAME VARCHAR(511),
    RDB$LINK_REMOTE_SCHEMA VARCHAR(511),
    RDB$LINK_SCHEMA_MODE INTEGER,
    RDB$LINK_SCHEMA_DEPTH INTEGER
);
```

**Schema Modes**:
- `0`: SCHEMA_MODE_NONE (no schema awareness)
- `1`: SCHEMA_MODE_FIXED (fixed remote schema mapping)
- `2`: SCHEMA_MODE_CONTEXT_AWARE (context-aware resolution)
- `3`: SCHEMA_MODE_HIERARCHICAL (hierarchical schema mapping)
- `4`: SCHEMA_MODE_MIRROR (mirror mode)

---

## Monitoring Tables (MON$)

### Database Activity Monitoring

#### MON$DATABASE
Current database information and statistics.
```sql
CREATE VIEW MON$DATABASE AS
SELECT 
    MON$DATABASE_NAME,
    MON$PAGE_SIZE,
    MON$PAGE_BUFFERS,
    MON$SQL_DIALECT,
    MON$SHUTDOWN_MODE,
    MON$SWEEP_INTERVAL,
    MON$READ_ONLY,
    MON$FORCED_WRITES,
    MON$RESERVE_SPACE,
    MON$CREATION_DATE,
    MON$PAGES,
    MON$STAT_ID,
    MON$BACKUP_STATE,
    MON$CRYPT_STATE,
    MON$OWNER,
    MON$SEC_DATABASE
FROM MON$DATABASE_IMPL;
```

#### MON$ATTACHMENTS
Current database connections.
```sql
CREATE VIEW MON$ATTACHMENTS AS
SELECT
    MON$ATTACHMENT_ID,
    MON$SERVER_PID,
    MON$STATE,
    MON$ATTACHMENT_NAME,
    MON$USER,
    MON$ROLE,
    MON$REMOTE_PROTOCOL,
    MON$REMOTE_ADDRESS,
    MON$REMOTE_PID,
    MON$CHARACTER_SET_ID,
    MON$TIMESTAMP,
    MON$GARBAGE_COLLECTION,
    MON$STAT_ID,
    MON$CLIENT_VERSION,
    MON$REMOTE_VERSION,
    MON$REMOTE_HOST,
    MON$REMOTE_OS_USER,
    MON$AUTH_METHOD,
    MON$SYSTEM_FLAG
FROM MON$ATTACHMENTS_IMPL;
```

#### MON$TRANSACTIONS
Active transactions.
```sql
CREATE VIEW MON$TRANSACTIONS AS
SELECT
    MON$TRANSACTION_ID,
    MON$ATTACHMENT_ID,
    MON$STATE,
    MON$TIMESTAMP,
    MON$TOP_TRANSACTION,
    MON$OLDEST_TRANSACTION,
    MON$OLDEST_ACTIVE,
    MON$ISOLATION_MODE,
    MON$LOCK_TIMEOUT,
    MON$READ_ONLY,
    MON$AUTO_COMMIT,
    MON$AUTO_UNDO,
    MON$STAT_ID
FROM MON$TRANSACTIONS_IMPL;
```

#### MON$STATEMENTS
Prepared and executing statements.
```sql
CREATE VIEW MON$STATEMENTS AS
SELECT
    MON$STATEMENT_ID,
    MON$ATTACHMENT_ID,
    MON$TRANSACTION_ID,
    MON$STATE,
    MON$TIMESTAMP,
    MON$SQL_TEXT,
    MON$STAT_ID,
    MON$EXPLAINED_PLAN
FROM MON$STATEMENTS_IMPL;
```

### Performance Monitoring

#### MON$IO_STATS
I/O operation statistics.
```sql
CREATE VIEW MON$IO_STATS AS
SELECT
    MON$STAT_ID,
    MON$STAT_GROUP,
    MON$PAGE_READS,
    MON$PAGE_WRITES,
    MON$PAGE_FETCHES,
    MON$PAGE_MARKS
FROM MON$IO_STATS_IMPL;
```

#### MON$RECORD_STATS
Record-level operation statistics.
```sql
CREATE VIEW MON$RECORD_STATS AS
SELECT
    MON$STAT_ID,
    MON$STAT_GROUP,
    MON$RECORD_SEQ_READS,
    MON$RECORD_IDX_READS,
    MON$RECORD_INSERTS,
    MON$RECORD_UPDATES,
    MON$RECORD_DELETES,
    MON$RECORD_BACKOUTS,
    MON$RECORD_PURGES,
    MON$RECORD_EXPUNGES
FROM MON$RECORD_STATS_IMPL;
```

#### MON$MEMORY_USAGE
Memory allocation and usage.
```sql
CREATE VIEW MON$MEMORY_USAGE AS
SELECT
    MON$STAT_ID,
    MON$STAT_GROUP,
    MON$MEMORY_USED,
    MON$MEMORY_ALLOCATED,
    MON$MAX_MEMORY_USED,
    MON$MAX_MEMORY_ALLOCATED
FROM MON$MEMORY_USAGE_IMPL;
```

### Lock and Context Monitoring

#### MON$CALL_STACK
Current execution call stack.
```sql
CREATE VIEW MON$CALL_STACK AS
SELECT
    MON$CALL_ID,
    MON$STATEMENT_ID,
    MON$CALLER_ID,
    MON$OBJECT_NAME,
    MON$OBJECT_TYPE,
    MON$TIMESTAMP,
    MON$SOURCE_LINE,
    MON$SOURCE_COLUMN,
    MON$STAT_ID
FROM MON$CALL_STACK_IMPL;
```

#### MON$CONTEXT_VARIABLES
Current context variable values.
```sql
CREATE VIEW MON$CONTEXT_VARIABLES AS
SELECT
    MON$ATTACHMENT_ID,
    MON$TRANSACTION_ID,
    MON$VARIABLE_NAME,
    MON$VARIABLE_VALUE
FROM MON$CONTEXT_VARIABLES_IMPL;
```

#### MON$TABLE_STATS
Table-level access statistics.
```sql
CREATE VIEW MON$TABLE_STATS AS
SELECT
    MON$STAT_ID,
    MON$STAT_GROUP,
    MON$TABLE_NAME,
    MON$NATURAL_READS,
    MON$INDEXED_READS,
    MON$INSERTS,
    MON$UPDATES,
    MON$DELETES,
    MON$BACKOUTS,
    MON$PURGES,
    MON$EXPUNGES
FROM MON$TABLE_STATS_IMPL;
```

---

## Base Database Objects

### System Stored Procedures

#### System Information Procedures

**RDB$GET_CONTEXT** - Retrieve context variable values
```sql
-- Usage: SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_NAME');
-- Returns database name, user name, transaction info, etc.
```

**RDB$SET_CONTEXT** - Set context variable values
```sql
-- Usage: EXECUTE PROCEDURE RDB$SET_CONTEXT('USER_SESSION', 'LAST_LOGIN', CURRENT_TIMESTAMP);
-- Sets user-defined context variables
```

**RDB$ROLE_IN_USE** - Check if role is currently active
```sql
-- Usage: SELECT RDB$ROLE_IN_USE('ADMIN_ROLE');
-- Returns 1 if role is active, 0 otherwise
```

#### Backup and Maintenance Procedures

**RDB$BACKUP_DATABASE** - Initiate database backup
```sql
-- Usage: EXECUTE PROCEDURE RDB$BACKUP_DATABASE('/path/to/backup.fbk');
-- Starts online backup process
```

**RDB$SWEEP_DATABASE** - Perform garbage collection
```sql
-- Usage: EXECUTE PROCEDURE RDB$SWEEP_DATABASE;
-- Forces database sweep operation
```

**RDB$VALIDATE_DATABASE** - Database validation
```sql
-- Usage: EXECUTE PROCEDURE RDB$VALIDATE_DATABASE;
-- Validates database integrity
```

#### Schema Management Procedures (ScratchBird Extension)

**RDB$GET_SCHEMA_HIERARCHY** - Retrieve schema tree
```sql
-- Usage: SELECT * FROM RDB$GET_SCHEMA_HIERARCHY('finance');
-- Returns hierarchical schema structure
```

**RDB$RESOLVE_SCHEMA_PATH** - Resolve qualified names
```sql
-- Usage: SELECT RDB$RESOLVE_SCHEMA_PATH('accounting.reports.table1');
-- Returns fully qualified object path
```

### System Functions

#### Metadata Functions

**RDB$GET_TRANSACTION_CN** - Get transaction commit number
```sql
SELECT RDB$GET_TRANSACTION_CN(CURRENT_TRANSACTION);
```

**RDB$ROLE_IN_USE** - Check active role
```sql
SELECT RDB$ROLE_IN_USE('MANAGER');
```

**RDB$SYSTEM_PRIVILEGE** - Check system privilege
```sql
SELECT RDB$SYSTEM_PRIVILEGE('CREATE_DATABASE');
```

#### Schema Functions (ScratchBird Extension)

**CURRENT_SCHEMA** - Get current schema
```sql
SELECT CURRENT_SCHEMA;
```

**CURRENT_SCHEMA_QUALIFIED** - Get full schema path
```sql
SELECT CURRENT_SCHEMA_QUALIFIED;
```

**HOME_SCHEMA** - Get user's home schema
```sql
SELECT HOME_SCHEMA;
```

**CURRENT_SCHEMA_PARENT** - Get parent schema
```sql
SELECT CURRENT_SCHEMA_PARENT;
```

**CURRENT_SCHEMA_ROOT** - Get root schema
```sql
SELECT CURRENT_SCHEMA_ROOT;
```

**CURRENT_SCHEMA_LEVEL** - Get nesting level
```sql
SELECT CURRENT_SCHEMA_LEVEL;
```

### System Sequences

#### Core System Sequences

**RDB$SECURITY_CLASS** - Security class ID generator
```sql
CREATE SEQUENCE RDB$SECURITY_CLASS;
```

**RDB$FIELD_NAME** - System field name generator
```sql
CREATE SEQUENCE RDB$FIELD_NAME;
```

**RDB$FUNCTION_NAME** - System function name generator
```sql
CREATE SEQUENCE RDB$FUNCTION_NAME;
```

**RDB$GENERATOR_NAME** - Generator name sequence
```sql
CREATE SEQUENCE RDB$GENERATOR_NAME;
```

### System Triggers

#### System Metadata Triggers

**RDB$TRIGGER_1** - Schema validation trigger
- Validates hierarchical schema operations
- Prevents circular references
- Enforces depth limits

**RDB$TRIGGER_2** - Security trigger
- Validates user operations
- Enforces privilege requirements
- Logs security events

**RDB$TRIGGER_3** - Database link trigger
- Validates database link operations
- Manages schema resolution cache
- Handles link state changes

---

## Default Roles and Users

### System Roles

#### SYSDBA
- **Purpose**: Database administrator with full privileges
- **Privileges**: All system and object privileges
- **Default Password**: 'masterkey' (should be changed)

#### PUBLIC
- **Purpose**: Default role for all users
- **Privileges**: Basic connection and usage rights
- **Membership**: Automatic for all users

#### RDB$ADMIN
- **Purpose**: Administrative role for system operations
- **Privileges**: Database administration functions
- **Membership**: Granted to database administrators

### ScratchBird System Roles

#### SCHEMA_ADMIN
- **Purpose**: Hierarchical schema management
- **Privileges**: CREATE SCHEMA, ALTER SCHEMA, DROP SCHEMA
- **Scope**: All schemas or specific schema hierarchy

#### LINK_ADMIN  
- **Purpose**: Database link administration
- **Privileges**: CREATE DATABASE LINK, ALTER DATABASE LINK, DROP DATABASE LINK
- **Scope**: Cross-database connectivity management

---

## Character Sets and Collations

### Default Character Sets

#### UTF8 (Default)
- **Character Set ID**: 4
- **Bytes per Character**: 1-4 (variable)
- **Default Collation**: UTF8 (Unicode)
- **Usage**: Default for new databases

#### ASCII
- **Character Set ID**: 2  
- **Bytes per Character**: 1
- **Default Collation**: ASCII
- **Usage**: 7-bit ASCII compatibility

#### UNICODE_FSS
- **Character Set ID**: 3
- **Bytes per Character**: 1-3 (variable)
- **Default Collation**: UNICODE_FSS
- **Usage**: Legacy Unicode support

### Default Collations

#### UTF8 Collations
- **UTF8**: Basic UTF-8 binary collation
- **UTF8_UNICODE_CI**: Case-insensitive Unicode collation
- **UTF8_UNICODE_CI_AI**: Case and accent insensitive

#### ASCII Collations
- **ASCII**: Basic ASCII binary collation
- **ASCII_CI**: Case-insensitive ASCII collation

---

## System Domains

### Built-in Domains

#### RDB$DBKEY
- **Base Type**: CHAR(8)
- **Purpose**: Database record key
- **Usage**: Internal row identification

#### RDB$DESCRIPTION
- **Base Type**: BLOB SUB_TYPE TEXT
- **Purpose**: Object descriptions
- **Usage**: Metadata documentation

#### RDB$FIELD_NAME
- **Base Type**: VARCHAR(67)
- **Purpose**: Object identifier names
- **Usage**: Tables, columns, procedures, etc.

#### RDB$RELATION_NAME
- **Base Type**: VARCHAR(67)
- **Purpose**: Table and view names
- **Usage**: Relation identification

---

## Metadata Queries

### Common System Queries

#### List All Tables
```sql
SELECT RDB$RELATION_NAME
FROM RDB$RELATIONS
WHERE RDB$SYSTEM_FLAG = 0
  AND RDB$RELATION_TYPE = 0
ORDER BY RDB$RELATION_NAME;
```

#### List Table Columns
```sql
SELECT rf.RDB$FIELD_NAME,
       rf.RDB$FIELD_POSITION,
       f.RDB$FIELD_TYPE,
       f.RDB$FIELD_LENGTH,
       rf.RDB$NULL_FLAG,
       rf.RDB$DEFAULT_SOURCE
FROM RDB$RELATION_FIELDS rf
JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME
WHERE rf.RDB$RELATION_NAME = 'YOUR_TABLE'
ORDER BY rf.RDB$FIELD_POSITION;
```

#### List All Indexes
```sql
SELECT RDB$INDEX_NAME,
       RDB$RELATION_NAME,
       RDB$UNIQUE_FLAG,
       RDB$INDEX_TYPE,
       RDB$STATISTICS
FROM RDB$INDICES
WHERE RDB$SYSTEM_FLAG = 0
ORDER BY RDB$RELATION_NAME, RDB$INDEX_NAME;
```

#### Schema Hierarchy Query (ScratchBird)
```sql
WITH RECURSIVE schema_tree AS (
    SELECT RDB$SCHEMA_NAME, 
           RDB$PARENT_SCHEMA_NAME, 
           RDB$SCHEMA_LEVEL,
           CAST(RDB$SCHEMA_NAME AS VARCHAR(500)) as path
    FROM RDB$SCHEMAS
    WHERE RDB$PARENT_SCHEMA_NAME IS NULL
    
    UNION ALL
    
    SELECT s.RDB$SCHEMA_NAME,
           s.RDB$PARENT_SCHEMA_NAME,
           s.RDB$SCHEMA_LEVEL,
           st.path || '.' || s.RDB$SCHEMA_NAME
    FROM RDB$SCHEMAS s
    JOIN schema_tree st ON s.RDB$PARENT_SCHEMA_NAME = st.RDB$SCHEMA_NAME
)
SELECT * FROM schema_tree ORDER BY path;
```

#### Current Database Statistics
```sql
SELECT MON$DATABASE_NAME,
       MON$PAGE_SIZE,
       MON$PAGES,
       MON$SQL_DIALECT,
       MON$CREATION_DATE
FROM MON$DATABASE;
```

#### Active Connections
```sql
SELECT MON$ATTACHMENT_ID,
       MON$USER,
       MON$ROLE,
       MON$REMOTE_ADDRESS,
       MON$TIMESTAMP
FROM MON$ATTACHMENTS
WHERE MON$SYSTEM_FLAG = 0
ORDER BY MON$TIMESTAMP;
```

---

## System Navigation

### Information Schema Views

ScratchBird provides SQL standard INFORMATION_SCHEMA views:

#### INFORMATION_SCHEMA.TABLES
```sql
SELECT TABLE_CATALOG,
       TABLE_SCHEMA, 
       TABLE_NAME,
       TABLE_TYPE
FROM INFORMATION_SCHEMA.TABLES
WHERE TABLE_SCHEMA NOT IN ('RDB$', 'MON$');
```

#### INFORMATION_SCHEMA.COLUMNS
```sql
SELECT TABLE_NAME,
       COLUMN_NAME,
       DATA_TYPE,
       IS_NULLABLE,
       COLUMN_DEFAULT
FROM INFORMATION_SCHEMA.COLUMNS
WHERE TABLE_SCHEMA = 'YOUR_SCHEMA'
ORDER BY TABLE_NAME, ORDINAL_POSITION;
```

### System Procedures for Navigation

#### Schema Context Management
```sql
-- Set current schema
SET SCHEMA 'finance.accounting';

-- Get current schema context
SELECT CURRENT_SCHEMA as current,
       HOME_SCHEMA as home,
       CURRENT_SCHEMA_QUALIFIED as full_path;

-- List objects in current schema  
SELECT OBJECT_NAME, OBJECT_TYPE
FROM RDB$SCHEMA_OBJECTS 
WHERE SCHEMA_NAME = CURRENT_SCHEMA;
```

---

## Database Creation Process

### Creation Steps

1. **Page Allocation**: Initial page allocation (header, system tables)
2. **System Table Creation**: All RDB$ tables created
3. **System Data Population**: Default character sets, collations, roles
4. **Monitoring Views**: MON$ view creation and activation
5. **System Procedures**: Built-in procedure compilation
6. **Default Objects**: Sequences, triggers, functions creation
7. **Security Initialization**: SYSDBA user, default privileges
8. **Schema Setup**: Default schema creation, hierarchy initialization

### Initial Database Size Breakdown
- **System Tables**: ~12MB (47 RDB$ tables)
- **System Indexes**: ~2MB (primary keys, foreign keys)
- **System Procedures**: ~1MB (compiled PSQL code)
- **Monitoring Infrastructure**: ~500KB (MON$ views)
- **Character Set Data**: ~300KB (collation tables)
- **Default Data**: ~200KB (roles, privileges, metadata)

### Configuration Parameters
```sql
-- Default database settings
DATABASE_NAME = "database.fdb"
PAGE_SIZE = 8192
SQL_DIALECT = 4
DEFAULT_CHARACTER_SET = UTF8
FORCED_WRITES = ON
RESERVE_SPACE = ON
```

---

*This documentation covers ScratchBird Alpha 0.6.0. For the latest system schema updates, refer to the official ScratchBird documentation and release notes.*