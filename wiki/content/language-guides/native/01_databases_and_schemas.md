[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native V2 SQL - Databases and Schemas

## Overview

This document describes the database and schema management features available in ScratchBird's Native V2 SQL dialect. Databases are the top-level containers for all schemas, tables, and other objects in a ScratchBird instance. Schemas provide logical organization within a database.

ScratchBird supports both native ScratchBird databases and emulated database connections to external database systems (Firebird, MySQL, PostgreSQL, etc.).

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## CREATE DATABASE

### Description

Creates a new ScratchBird database. This initializes the physical database files, default tablespaces, and system catalogs required for the database to operate.

### Syntax

```sql
CREATE DATABASE [IF NOT EXISTS] <database_name>
```

### Parameters

- **IF NOT EXISTS**: Optional. Prevents an error if the database already exists. Without this clause, attempting to create an existing database results in an error.
- **database_name**: The name of the database to create. Must be a valid identifier.

### Examples

**Example 1: Create a simple database**
```sql
CREATE DATABASE mydb;
```

**Example 2: Safely create a database (no error if it exists)**
```sql
CREATE DATABASE IF NOT EXISTS application_data;
```

**Example 3: Create databases for different environments**
```sql
CREATE DATABASE dev_db;
CREATE DATABASE staging_db;
CREATE DATABASE production_db;
```

### Notes

- The database name becomes part of the schema path when accessing objects.
- Database creation requires appropriate privileges (typically superuser or database creator role).
- Once created, the database exists on disk and persists across server restarts.

---

## CREATE DATABASE EMULATED

### Description

Creates a metadata-only emulated database connection to an external database system. This allows ScratchBird to connect to and query Firebird, MySQL, PostgreSQL, and other database systems using their native protocols. The emulated database appears in the catalog under `emulation.<dialect>.<server>.<path>.<database>`.

No physical ScratchBird database file is created - this is purely a connection metadata record.

### Syntax

```sql
CREATE DATABASE [IF NOT EXISTS] EMULATED <dialect>
  [ON SERVER <server_name>] <source_spec>
  [ALIAS <alias_name> [, <alias_name> ...]]
  [WITH [OPTIONS] ( <option_key> = <option_value> [, ...] )]
```

### Parameters

- **EMULATED**: Marks this as an emulated database connection.
- **dialect**: The target database engine. Supported values:
  - `firebird` - Firebird database
  - `mysql` - MySQL database
  - `postgresql` - PostgreSQL database
- **ON SERVER**: Optional. Specifies the server name (defaults to `localhost`).
- **source_spec**: Connection specification. Can be:
  - An identifier (e.g., `mydb`)
  - A string literal with path (e.g., `'srv:/data/employee.fdb'`)
  - String literals may include OS paths or `server:/path` specifications
  - Path components become schema path components; file extension is stripped from database name
- **ALIAS**: Optional. One or more alias names that can be used as shortcuts to access the emulated database.
- **WITH OPTIONS**: Optional. Key-value pairs for connection parameters such as credentials, connection pooling settings, etc.

### Examples

**Example 1: Connect to a local MySQL database**
```sql
CREATE DATABASE EMULATED mysql mydb;
```

**Example 2: Connect to a remote PostgreSQL database with credentials**
```sql
CREATE DATABASE EMULATED postgresql 'dbserver:/var/lib/postgresql/data/warehouse'
  WITH (user = 'readonly', password = 'secret123', port = 5432);
```

**Example 3: Connect to a Firebird database with aliases**
```sql
CREATE DATABASE EMULATED firebird 'srv:/var/db/employee.fdb'
  ALIAS legacy, emp, employees
  WITH (user = 'SYSDBA', password = 'masterkey');
```

**Example 4: Connect with connection pool settings**
```sql
CREATE DATABASE EMULATED mysql 'prod-db:/company/sales'
  ALIAS sales
  WITH (
    user = 'app_user',
    password = 'app_pass',
    max_connections = 20,
    connection_timeout = 30000
  );
```

### Notes

- Emulated databases appear in the catalog but don't consume local storage (beyond metadata).
- Queries against emulated databases are translated to the target dialect and executed remotely.
- Aliases provide convenient shorthand names for accessing emulated databases.
- Connection options are stored securely but should still use secure credential management.
- The full emulated database path follows the pattern: `emulation.<dialect>.<server>.<path_components>.<database_name>`

---

## ALTER DATABASE

### Description

Modifies properties of an existing database. For native databases, you can rename the database or change its owner. For emulated databases, you can manage aliases.

### Syntax

```sql
ALTER DATABASE <database_path> RENAME TO <new_name>
ALTER DATABASE <database_path> OWNER TO <owner_name>
ALTER DATABASE <database_path> ALIAS ADD <alias_name>
ALTER DATABASE <database_path> ALIAS DROP <alias_name>
```

### Parameters

- **database_path**: The full path or name of the database to alter.
- **RENAME TO**: Changes the name of the database.
- **OWNER TO**: Transfers ownership to a different user.
- **ALIAS ADD**: Adds an alias to an emulated database.
- **ALIAS DROP**: Removes an alias from an emulated database.

### Examples

**Example 1: Rename a database**
```sql
ALTER DATABASE mydb RENAME TO mydb_v2;
```

**Example 2: Change database ownership**
```sql
ALTER DATABASE application_data OWNER TO app_admin;
```

**Example 3: Add an alias to an emulated database**
```sql
ALTER DATABASE emulation.firebird.localhost.employee ALIAS ADD emp;
```

**Example 4: Remove an alias**
```sql
ALTER DATABASE emulation.mysql.localhost.warehouse ALIAS DROP wh;
```

**Example 5: Transfer ownership to a new DBA**
```sql
ALTER DATABASE production_db OWNER TO new_dba_user;
```

### Notes

- Renaming a database does not affect its physical location on disk.
- You must have appropriate privileges to alter a database (typically ownership or superuser).
- Alias management only applies to emulated databases.
- Existing connections to the database are not interrupted by ALTER DATABASE operations.

---

## DROP DATABASE

### Description

Removes a database entirely. For native databases, this deletes all schemas, tables, and physical files. For emulated databases, this removes the connection metadata record.

**WARNING:** This operation is irreversible. All data in native databases will be permanently lost.

### Syntax

```sql
DROP DATABASE [IF EXISTS] <database_path> [CASCADE | RESTRICT | FORCE]
```

### Parameters

- **IF EXISTS**: Optional. Prevents an error if the database doesn't exist.
- **database_path**: The full path or name of the database to drop.
- **CASCADE**: Drops the database even if it contains objects. (Default behavior)
- **RESTRICT**: Prevents dropping if the database contains objects.
- **FORCE**: Forces disconnection of active sessions before dropping.

### Examples

**Example 1: Drop a database**
```sql
DROP DATABASE old_test_db;
```

**Example 2: Safely drop a database that might not exist**
```sql
DROP DATABASE IF EXISTS temp_db;
```

**Example 3: Drop an emulated database connection**
```sql
DROP DATABASE IF EXISTS emulation.mysql.localhost.mydb CASCADE;
```

**Example 4: Force drop a database with active connections**
```sql
DROP DATABASE legacy_app FORCE;
```

**Example 5: Prevent dropping if database has objects**
```sql
DROP DATABASE staging_db RESTRICT;
```

### Notes

- For native databases, all physical files are deleted from disk.
- For emulated databases, only the connection metadata is removed - the remote database is unaffected.
- The FORCE option terminates all active sessions connected to the database.
- Always backup important data before dropping databases.
- Requires superuser privileges or database ownership.

---

## CREATE SCHEMA

### Description

Creates a new schema within the current database. Schemas provide logical namespaces for organizing tables, views, and other database objects.

### Syntax

```sql
CREATE SCHEMA [IF NOT EXISTS] <schema_name>
CREATE SCHEMA [IF NOT EXISTS] AUTHORIZATION <owner_name>
```

### Parameters

- **IF NOT EXISTS**: Optional. Prevents an error if the schema already exists.
- **schema_name**: The name of the schema to create.
- **AUTHORIZATION**: Optional. Specifies the owner of the schema. If provided without a schema name, the schema is named after the owner.

### Examples

**Example 1: Create a simple schema**
```sql
CREATE SCHEMA app;
```

**Example 2: Create a schema with ownership**
```sql
CREATE SCHEMA hr AUTHORIZATION hr_manager;
```

**Example 3: Create a schema named after the owner**
```sql
CREATE SCHEMA AUTHORIZATION finance_team;
```

**Example 4: Safely create multiple schemas**
```sql
CREATE SCHEMA IF NOT EXISTS public;
CREATE SCHEMA IF NOT EXISTS staging;
CREATE SCHEMA IF NOT EXISTS archive;
```

**Example 5: Create schemas for multi-tenant application**
```sql
CREATE SCHEMA tenant_1001 AUTHORIZATION tenant_admin;
CREATE SCHEMA tenant_1002 AUTHORIZATION tenant_admin;
CREATE SCHEMA shared AUTHORIZATION system_admin;
```

### Notes

- Schemas help organize database objects and avoid naming conflicts.
- Multiple schemas can exist in a single database.
- Object names must be unique within a schema but can be reused across schemas.
- If no owner is specified, the current user becomes the owner.
- Schema names become part of the fully qualified object path (e.g., `app.users`).

---

## ALTER SCHEMA

### Description

Modifies an existing schema's properties including name, owner, or search path.

### Syntax

```sql
ALTER SCHEMA <schema_name> RENAME TO <new_name>
ALTER SCHEMA <schema_name> OWNER TO <owner_name>
ALTER SCHEMA <schema_name> SET PATH <schema_path>
```

### Parameters

- **schema_name**: The name of the schema to alter.
- **RENAME TO**: Changes the name of the schema.
- **OWNER TO**: Transfers ownership to a different user.
- **SET PATH**: Sets a search path for the schema (used for object resolution).

### Examples

**Example 1: Rename a schema**
```sql
ALTER SCHEMA app RENAME TO app_v2;
```

**Example 2: Change schema ownership**
```sql
ALTER SCHEMA hr OWNER TO new_hr_manager;
```

**Example 3: Set a schema search path**
```sql
ALTER SCHEMA app SET PATH public;
```

**Example 4: Transfer ownership for maintenance**
```sql
ALTER SCHEMA production OWNER TO maintenance_user;
```

### Notes

- Renaming a schema updates all references in the catalog but does not affect object definitions.
- You must have appropriate privileges to alter a schema (typically ownership or superuser).
- SET PATH functionality may have limited usage in current implementation.
- All objects within the schema retain their properties when the schema is renamed.

---

## DROP SCHEMA

### Description

Removes a schema from the database. By default, the schema must be empty. Use CASCADE to remove the schema and all contained objects.

### Syntax

```sql
DROP SCHEMA [IF EXISTS] <schema_name> [CASCADE | RESTRICT]
```

### Parameters

- **IF EXISTS**: Optional. Prevents an error if the schema doesn't exist.
- **schema_name**: The name of the schema to drop.
- **CASCADE**: Removes the schema and all objects it contains (tables, views, etc.).
- **RESTRICT**: Default. Prevents dropping if the schema contains any objects.

### Examples

**Example 1: Drop an empty schema**
```sql
DROP SCHEMA old_app;
```

**Example 2: Safely drop a schema that might not exist**
```sql
DROP SCHEMA IF EXISTS temp_schema;
```

**Example 3: Drop a schema and all its contents**
```sql
DROP SCHEMA IF EXISTS legacy CASCADE;
```

**Example 4: Prevent accidental deletion of non-empty schema**
```sql
DROP SCHEMA staging RESTRICT;
```

**Example 5: Clean up test schemas**
```sql
DROP SCHEMA IF EXISTS test_run_001 CASCADE;
DROP SCHEMA IF EXISTS test_run_002 CASCADE;
```

### Notes

- RESTRICT is the default behavior and provides safety against accidental data loss.
- CASCADE will drop all tables, views, sequences, and other objects in the schema.
- Dropping a schema cannot be undone - always backup important data first.
- Requires appropriate privileges (schema ownership or superuser).
- Dependencies from objects in other schemas will prevent dropping even with RESTRICT.

---

## CREATE TABLESPACE

### Description

Creates a new tablespace file and registers it in the catalog. Tablespaces
store table and index data outside the primary database file.

### Syntax (Spec)

```sql
CREATE TABLESPACE <tablespace_name>
  LOCATION '<path/to/tablespace/file.sbts>'
  [ AUTOEXTEND { ON | OFF } ]
  [ AUTOEXTEND_SIZE <size_in_mb> ]
  [ MAXSIZE { <size_in_mb> | UNLIMITED } ]
  [ PREALLOC <size_in_mb> ]
```

### Examples (Spec)

```sql
CREATE TABLESPACE ts_hot
  LOCATION '/mnt/ssd/scratchbird/ts_hot.sbts'
  AUTOEXTEND ON
  AUTOEXTEND_SIZE 100
  MAXSIZE 50000;
```

### Status

**Implemented in V2 Parser** - `parseCreateTablespace()` handles CREATE TABLESPACE with LOCATION, AUTOEXTEND, AUTOEXTEND_SIZE, and MAXSIZE options.

---

## ALTER TABLESPACE

### Description

Modifies tablespace properties (autoextend, size limits, rename) or performs
attach/detach operations.

### Syntax (Spec)

```sql
ALTER TABLESPACE <tablespace_name>
  { AUTOEXTEND { ON | OFF } }
  | { AUTOEXTEND_SIZE <size_in_mb> }
  | { MAXSIZE { <size_in_mb> | UNLIMITED } }
  | { RENAME TO <new_tablespace_name> }
  | { ATTACH | DETACH }
```

### Examples (Spec)

```sql
ALTER TABLESPACE ts_hot AUTOEXTEND_SIZE 200;
ALTER TABLESPACE ts_archive RENAME TO ts_archive_readonly;
```

### Status

**Implemented in V2 Parser** - `parseAlterTablespace()` supports AUTOEXTEND ON/OFF, AUTOEXTEND_SIZE, MAXSIZE, RENAME TO, SET SCHEMA, ATTACH, and DETACH operations.

---

## DROP TABLESPACE

### Description

Removes a tablespace from the catalog and optionally forces removal when
objects still reside in it.

### Syntax (Spec)

```sql
DROP TABLESPACE <tablespace_name> [FORCE]
```

### Examples (Spec)

```sql
DROP TABLESPACE ts_temp;
DROP TABLESPACE ts_archive FORCE;
```

### Status

**Implemented in V2 Parser** - `parseDropTablespace()` supports DROP TABLESPACE with optional FORCE flag and IF EXISTS.

---

## Foreign Data Wrappers

### CREATE SERVER

#### Description

Creates a foreign server definition that references an external data source via a foreign data wrapper (FDW).

#### Syntax

```sql
CREATE SERVER <server_name>
    [TYPE '<server_type>']
    [VERSION '<server_version>']
    FOREIGN DATA WRAPPER <fdw_name>
    [OPTIONS (<key> <value> [, ...])]
```

#### Examples

```sql
CREATE SERVER remote_pg
    TYPE 'postgresql'
    VERSION '15'
    FOREIGN DATA WRAPPER postgres_fdw
    OPTIONS (host 'db.example.com', port '5432', dbname 'analytics');

CREATE SERVER file_server
    FOREIGN DATA WRAPPER file_fdw;
```

#### Implementation Status

- V2 parser: `parseCreateForeignServer()` handles TYPE, VERSION, FOREIGN DATA WRAPPER, and OPTIONS

---

### CREATE FOREIGN TABLE

#### Description

Creates a table whose data resides on an external server accessed through a foreign data wrapper.

#### Syntax

```sql
CREATE FOREIGN TABLE [IF NOT EXISTS] <table_name> (
    <column_name> <data_type> [OPTIONS (<key> <value> [, ...])] [, ...]
)
SERVER <server_name>
[OPTIONS (<key> <value> [, ...])]
```

#### Examples

```sql
CREATE FOREIGN TABLE remote_users (
    id INTEGER,
    name VARCHAR(100),
    email VARCHAR(255) OPTIONS (column_name 'user_email')
)
SERVER remote_pg
OPTIONS (schema_name 'public', table_name 'users');

CREATE FOREIGN TABLE IF NOT EXISTS csv_data (
    col1 TEXT,
    col2 INTEGER
)
SERVER file_server
OPTIONS (filename '/data/import.csv', format 'csv');
```

#### Implementation Status

- V2 parser: `parseCreateForeignTable()` handles IF NOT EXISTS, column definitions with per-column OPTIONS, table constraints, SERVER clause, and table-level OPTIONS

---

### CREATE USER MAPPING

#### Description

Creates a mapping between a local user and a remote user on a foreign server, storing credentials for authenticated access.

#### Syntax

```sql
CREATE USER MAPPING FOR { <user_name> | CURRENT_USER | SESSION_USER | PUBLIC }
    SERVER <server_name>
    [OPTIONS (<key> <value> [, ...])]
```

#### Examples

```sql
CREATE USER MAPPING FOR alice
    SERVER remote_pg
    OPTIONS (user 'remote_alice', password 'remote_pass');

CREATE USER MAPPING FOR PUBLIC
    SERVER file_server;

CREATE USER MAPPING FOR CURRENT_USER
    SERVER remote_pg
    OPTIONS (user 'readonly');
```

#### Implementation Status

- V2 parser: `parseCreateUserMapping()` handles user targets (USER name, CURRENT_USER, SESSION_USER, PUBLIC), SERVER clause, and OPTIONS

---

### DROP SERVER / DROP FOREIGN TABLE / DROP USER MAPPING

#### Description

Removes foreign data wrapper objects.

#### Syntax

```sql
DROP SERVER <server_name>
DROP FOREIGN TABLE <table_name>
DROP USER MAPPING FOR { <user_name> | CURRENT_USER | SESSION_USER | PUBLIC } SERVER <server_name>
```

#### Implementation Status

- V2 parser: `parseDropForeignServer()`, `parseDropForeignTable()`, and `parseDropUserMapping()` are all implemented

---

## Synonyms

### CREATE SYNONYM

#### Description

Creates a synonym (alias) for a database object. Synonyms can be public (visible to all schemas) or private (schema-scoped). Synonyms can reference tables, views, sequences, functions, procedures, domains, types, packages, schemas, databases, UDRs, and foreign tables.

#### Syntax

```sql
CREATE [PUBLIC] SYNONYM <synonym_name>
    FOR { TABLE | VIEW | SEQUENCE | FUNCTION | PROCEDURE | DOMAIN | TYPE
        | PACKAGE | SCHEMA | DATABASE | UDR | FOREIGN TABLE } <target_name>
```

#### Examples

```sql
CREATE SYNONYM users FOR TABLE app.users;
CREATE PUBLIC SYNONYM orders FOR TABLE sales.orders;
CREATE SYNONYM get_total FOR FUNCTION analytics.calculate_total;
CREATE SYNONYM remote_data FOR FOREIGN TABLE remote_users;
```

#### Implementation Status

- V2 parser: `parseCreateSynonym()` handles PUBLIC/private distinction, 12 target object types, and schema-qualified paths

---

### DROP SYNONYM

#### Description

Removes a synonym.

#### Syntax

```sql
DROP [PUBLIC] SYNONYM <synonym_name>
```

#### Example

```sql
DROP SYNONYM users;
DROP PUBLIC SYNONYM orders;
```

#### Implementation Status

- V2 parser: `parseDropSynonym()` handles PUBLIC/private and schema-qualified paths

---

## User-Defined Routines (UDR)

### CREATE UDR

#### Description

Registers an external User-Defined Routine implemented in a shared library (e.g., C/C++). UDRs can be functions, procedures, or triggers backed by native code.

#### Syntax

```sql
CREATE UDR { FUNCTION | PROCEDURE | TRIGGER } <udr_name>
    AS '<library_path>'
    ENTRY '<entry_point>'
    [SIGNATURE '<signature>']
```

#### Examples

```sql
CREATE UDR FUNCTION fast_hash
    AS '/usr/lib/scratchbird/udr_hash.so'
    ENTRY 'compute_hash'
    SIGNATURE 'INTEGER (VARCHAR)';

CREATE UDR PROCEDURE bulk_import
    AS '/opt/scratchbird/plugins/import.so'
    ENTRY 'run_import';
```

#### Implementation Status

- V2 parser: `parseCreateUdr()` handles FUNCTION/PROCEDURE/TRIGGER type, library path, entry point, and optional signature

---

### DROP UDR

#### Description

Removes a UDR registration.

#### Syntax

```sql
DROP UDR <udr_name>
```

#### Implementation Status

- V2 parser: `parseDropUdr()` is implemented

---

## Known Limitations

### Spec Deltas (Implementation differs from specification)

**CREATE DATABASE:**
- Spec defines additional options not parsed in V2:
  - `PAGE_SIZE` (8K|16K|32K|64K|128K)
  - `DEFAULT CHARACTER SET`
  - `DEFAULT COLLATE`
  - `ENCRYPTED [WITH PASSWORD]`
  - `OWNER`
- Current implementation only supports basic syntax without these options
- Spec reference: `/docs/specifications/ddl/DDL_DATABASES.md`

**ALTER DATABASE:**
- Spec supports additional operations not parsed in V2:
  - `SET DEFAULT CHARACTER SET <charset>`
  - `SET DEFAULT COLLATE <collation>`
  - `SET SWEEP INTERVAL <integer>`
- Only RENAME TO, OWNER TO, and ALIAS operations are currently supported
- Spec reference: `/docs/specifications/ddl/DDL_DATABASES.md`

**DROP DATABASE:**
- CASCADE/RESTRICT modifiers are accepted by parser but executor uses FORCE semantics
- Active sessions are forcibly disconnected regardless of CASCADE/RESTRICT setting
- May not fully align with spec-defined behavior
- Spec reference: `/docs/specifications/ddl/CASCADE_DROP_SPECIFICATION.md`

**ALTER SCHEMA:**
- SET PATH is parsed but functionality is not fully implemented
- Behavior and use cases need clarification in specification
- May not be commonly used in practice
- Spec reference: `/docs/specifications/ddl/DDL_SCHEMAS.md`

### General Notes

- All database and schema DDL operations are fully transactional
- Operations are persisted using ScratchBird's Multi-Generational Architecture (MGA)
- Full implementation status documented in `/docs/audit/parsers/V2/SUMMARY.md`
- Critical findings documented in `/docs/audit/parsers/CRITICAL_FINDINGS.md`
