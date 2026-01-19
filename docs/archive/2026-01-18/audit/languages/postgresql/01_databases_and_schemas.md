# PostgreSQL Databases and Schemas

**PostgreSQL Emulation Layer - Database and Schema DDL Reference**

This document covers PostgreSQL-compatible database and schema operations in ScratchBird's PostgreSQL emulation layer.

---

## Overview

The PostgreSQL emulation layer provides compatibility with PostgreSQL 16 syntax for database and schema management. When PostgreSQL clients connect to ScratchBird, they can use native PostgreSQL DDL commands to manage logical databases and schemas.

**Key Points:**
- Database operations create emulated schema metadata in the `/remote/emulated/postgresql/{server}/{database}/` path
- No physical database files are created (emulation only)
- Schema objects are organized hierarchically within databases
- Full support for ownership, authorization, and cascading drops

---

## CREATE DATABASE

Creates a new PostgreSQL-emulated database in ScratchBird.

### Description

`CREATE DATABASE` registers an emulated PostgreSQL database in ScratchBird's metadata catalog. Unlike native PostgreSQL, this does not create separate physical database files but instead creates a logical namespace for schemas and objects.

### Syntax

```sql
CREATE DATABASE database_name
    [ WITH ] [ OWNER [=] user_name ]
           [ TEMPLATE [=] template ]
           [ ENCODING [=] encoding ]
           [ STRATEGY [=] strategy ]
           [ LOCALE [=] locale ]
           [ LC_COLLATE [=] lc_collate ]
           [ LC_CTYPE [=] lc_ctype ]
           [ ICU_LOCALE [=] icu_locale ]
           [ ICU_RULES [=] icu_rules ]
           [ LOCALE_PROVIDER [=] locale_provider ]
           [ COLLATION_VERSION [=] collation_version ]
           [ TABLESPACE [=] tablespace_name ]
           [ ALLOW_CONNECTIONS [=] allowconn ]
           [ CONNECTION LIMIT [=] connlimit ]
           [ IS_TEMPLATE [=] istemplate ]
           [ OID [=] oid ]
```

### Parameters

- **database_name** - Name of the database to create
- **OWNER** - User who will own the database (optional)
- **TEMPLATE** - Template database to copy (optional, parsed but not enforced)
- **ENCODING** - Character set encoding (optional, parsed but not enforced)
- **LOCALE** - Default locale (optional, parsed but not enforced)
- **LC_COLLATE** - Collation order (optional, parsed but not enforced)
- **LC_CTYPE** - Character classification (optional, parsed but not enforced)
- **TABLESPACE** - Default tablespace (optional, parsed but not enforced)
- **CONNECTION LIMIT** - Maximum concurrent connections (optional, parsed but not enforced)

### Examples

**Basic database creation:**
```sql
CREATE DATABASE myapp;
```

**Database with owner:**
```sql
CREATE DATABASE myapp OWNER app_user;
```

**Database with encoding and locale:**
```sql
CREATE DATABASE myapp
    WITH ENCODING 'UTF8'
         LOCALE 'en_US.UTF-8'
         OWNER app_admin;
```

**Database with connection limit:**
```sql
CREATE DATABASE analytics
    WITH CONNECTION LIMIT 50
         OWNER analytics_admin;
```

### Notes

- Database names must be unique within the PostgreSQL emulation namespace
- The database is created in the path: `/remote/emulated/postgresql/{server}/{database_name}/`
- Most WITH options are parsed for compatibility but not fully enforced at the storage level
- The database will contain a default `public` schema automatically

### Related Statements

- [ALTER DATABASE](#alter-database)
- [DROP DATABASE](#drop-database)
- [CREATE SCHEMA](#create-schema)

---

## ALTER DATABASE

Modifies the properties of an existing database.

### Description

`ALTER DATABASE` allows you to change database properties such as owner, name, or configuration parameters.

### Syntax

```sql
-- Rename database
ALTER DATABASE database_name RENAME TO new_name

-- Change owner
ALTER DATABASE database_name OWNER TO new_owner

-- Set configuration parameter
ALTER DATABASE database_name SET configuration_parameter { TO | = } { value | DEFAULT }

-- Reset configuration parameter
ALTER DATABASE database_name RESET configuration_parameter
```

### Parameters

- **database_name** - Name of the database to alter
- **new_name** - New name for the database (for RENAME)
- **new_owner** - New owner role (for OWNER TO)
- **configuration_parameter** - Runtime configuration parameter name
- **value** - Value for the configuration parameter

### Examples

**Rename a database:**
```sql
ALTER DATABASE myapp RENAME TO myapp_production;
```

**Change database owner:**
```sql
ALTER DATABASE myapp OWNER TO new_admin;
```

**Set search path for database:**
```sql
ALTER DATABASE myapp SET search_path TO app_schema, public;
```

**Set default timezone:**
```sql
ALTER DATABASE myapp SET timezone TO 'UTC';
```

**Reset a parameter to default:**
```sql
ALTER DATABASE myapp RESET search_path;
```

### Notes

- You must have appropriate privileges to alter a database
- Renaming a database does not affect existing connections
- Configuration parameters set at the database level override server defaults
- Not all PostgreSQL configuration parameters are verified at parse time

### Related Statements

- [CREATE DATABASE](#create-database)
- [DROP DATABASE](#drop-database)

---

## DROP DATABASE

Removes a database and all its contents.

### Description

`DROP DATABASE` removes a database from the PostgreSQL emulation layer, including all schemas, tables, and other objects within it.

### Syntax

```sql
DROP DATABASE [ IF EXISTS ] database_name
```

### Parameters

- **IF EXISTS** - Do not throw an error if the database doesn't exist
- **database_name** - Name of the database to drop

### Examples

**Drop a database:**
```sql
DROP DATABASE myapp;
```

**Drop with IF EXISTS:**
```sql
DROP DATABASE IF EXISTS test_database;
```

**Drop multiple databases:**
```sql
DROP DATABASE IF EXISTS dev_db;
DROP DATABASE IF EXISTS staging_db;
DROP DATABASE IF EXISTS test_db;
```

### Notes

- This operation is irreversible - all data in the database is permanently deleted
- You cannot drop a database while connected to it (connect to a different database first)
- All active connections to the database should be terminated before dropping
- IF EXISTS prevents errors when scripting database cleanup

### Related Statements

- [CREATE DATABASE](#create-database)
- [ALTER DATABASE](#alter-database)

---

## CREATE SCHEMA

Creates a new schema within the current database.

### Description

`CREATE SCHEMA` creates a named schema (namespace) for organizing database objects. Schemas provide a way to organize tables, views, functions, and other objects into logical groups.

### Syntax

```sql
CREATE SCHEMA [ IF NOT EXISTS ] schema_name [ AUTHORIZATION role_specification ]

-- Alternative forms
CREATE SCHEMA schema_name [ AUTHORIZATION role_specification ] [ schema_element [ ... ] ]
CREATE SCHEMA AUTHORIZATION role_specification [ schema_element [ ... ] ]
CREATE SCHEMA IF NOT EXISTS AUTHORIZATION role_specification
```

### Parameters

- **IF NOT EXISTS** - Do not throw an error if the schema already exists
- **schema_name** - Name of the schema to create
- **AUTHORIZATION role_specification** - Owner of the schema (role or user name)
- **schema_element** - DDL statements to execute within the schema (CREATE TABLE, CREATE VIEW, etc.)

### Examples

**Basic schema creation:**
```sql
CREATE SCHEMA app;
```

**Schema with IF NOT EXISTS:**
```sql
CREATE SCHEMA IF NOT EXISTS app;
```

**Schema with owner:**
```sql
CREATE SCHEMA app AUTHORIZATION app_owner;
```

**Schema with authorization only (name derived from role):**
```sql
CREATE SCHEMA AUTHORIZATION app_user;
```

**Schema with initial objects:**
```sql
CREATE SCHEMA app AUTHORIZATION app_owner
    CREATE TABLE users (id INT PRIMARY KEY, name TEXT)
    CREATE VIEW active_users AS SELECT * FROM users WHERE active = true;
```

**Create multiple schemas:**
```sql
CREATE SCHEMA IF NOT EXISTS sales;
CREATE SCHEMA IF NOT EXISTS marketing;
CREATE SCHEMA IF NOT EXISTS operations;
```

### Notes

- Schema names must be unique within a database
- If no schema is specified when creating objects, the first schema in the `search_path` is used (typically `public`)
- Schema-qualified object names use the format: `schema_name.object_name`
- The `public` schema is created automatically in each database
- IF NOT EXISTS is useful for idempotent deployment scripts

### Related Statements

- [ALTER SCHEMA](#alter-schema)
- [DROP SCHEMA](#drop-schema)
- [SET search_path](10_session_show_set.md#set-search_path)

---

## ALTER SCHEMA

Modifies schema properties.

### Description

`ALTER SCHEMA` allows you to rename a schema or change its owner.

### Syntax

```sql
-- Rename schema
ALTER SCHEMA schema_name RENAME TO new_name

-- Change owner
ALTER SCHEMA schema_name OWNER TO new_owner
```

### Parameters

- **schema_name** - Name of the schema to alter
- **new_name** - New name for the schema
- **new_owner** - New owner role for the schema

### Examples

**Rename a schema:**
```sql
ALTER SCHEMA app RENAME TO application;
```

**Change schema owner:**
```sql
ALTER SCHEMA app OWNER TO admin;
```

**Transfer ownership to current user:**
```sql
ALTER SCHEMA legacy_schema OWNER TO CURRENT_USER;
```

### Notes

- You must own the schema or be a superuser to alter it
- Renaming a schema does not affect existing objects within it
- All objects in the schema maintain their relationship after renaming
- Applications using schema-qualified names must be updated after renaming

### Related Statements

- [CREATE SCHEMA](#create-schema)
- [DROP SCHEMA](#drop-schema)

---

## DROP SCHEMA

Removes a schema and optionally all its contents.

### Description

`DROP SCHEMA` removes a schema from the database. By default, the schema must be empty. Use CASCADE to automatically drop all contained objects.

### Syntax

```sql
DROP SCHEMA [ IF EXISTS ] schema_name [ CASCADE | RESTRICT ]
```

### Parameters

- **IF EXISTS** - Do not throw an error if the schema doesn't exist
- **schema_name** - Name of the schema to drop
- **CASCADE** - Automatically drop all objects contained in the schema, and all objects that depend on those objects
- **RESTRICT** - Refuse to drop if the schema contains any objects (default behavior)

### Examples

**Drop an empty schema:**
```sql
DROP SCHEMA app;
```

**Drop with IF EXISTS:**
```sql
DROP SCHEMA IF EXISTS temp_schema;
```

**Drop schema and all contents:**
```sql
DROP SCHEMA app CASCADE;
```

**Explicit RESTRICT (default):**
```sql
DROP SCHEMA app RESTRICT;
```

**Drop multiple schemas:**
```sql
DROP SCHEMA IF EXISTS dev_schema CASCADE;
DROP SCHEMA IF EXISTS test_schema CASCADE;
```

### Notes

- RESTRICT is the default if neither CASCADE nor RESTRICT is specified
- CASCADE will drop all tables, views, functions, types, and other objects in the schema
- Be extremely careful with CASCADE - it can delete substantial amounts of data
- The `public` schema cannot be dropped in standard PostgreSQL installations
- IF EXISTS is useful for cleanup scripts that may run multiple times

### Related Statements

- [CREATE SCHEMA](#create-schema)
- [ALTER SCHEMA](#alter-schema)

---

## CREATE TABLESPACE

Creates a tablespace object (PostgreSQL syntax).

### Syntax (PostgreSQL)

```sql
CREATE TABLESPACE tablespace_name
    [ OWNER user_name ]
    LOCATION 'directory_path';
```

### Examples

```sql
CREATE TABLESPACE fast_ssd LOCATION '/mnt/ssd/pg_ts_fast';
```

### Status

**NOT IMPLEMENTED:** PostgreSQL parser does not accept CREATE TABLESPACE.

---

## ALTER TABLESPACE

Renames or changes properties of a tablespace.

### Syntax (PostgreSQL)

```sql
ALTER TABLESPACE tablespace_name RENAME TO new_name;
```

### Examples

```sql
ALTER TABLESPACE fast_ssd RENAME TO fast_ssd_2025;
```

### Status

**NOT IMPLEMENTED:** PostgreSQL parser does not accept ALTER TABLESPACE.

---

## DROP TABLESPACE

Removes a tablespace object.

### Syntax (PostgreSQL)

```sql
DROP TABLESPACE tablespace_name;
```

### Examples

```sql
DROP TABLESPACE fast_ssd;
```

### Status

**NOT IMPLEMENTED:** PostgreSQL parser does not accept DROP TABLESPACE.

---

## Usage Patterns

### Schema-Qualified Names

Access objects in specific schemas using qualified names:

```sql
-- Reference table in specific schema
SELECT * FROM app.users;

-- Create table in specific schema
CREATE TABLE sales.orders (id INT, total NUMERIC);

-- Create function in specific schema
CREATE FUNCTION admin.cleanup_old_data() RETURNS VOID AS $$
BEGIN
    DELETE FROM app.logs WHERE created_at < NOW() - INTERVAL '90 days';
END;
$$ LANGUAGE plpgsql;
```

### Search Path

Control which schemas are searched for unqualified object names:

```sql
-- Set search path for session
SET search_path TO app, public;

-- Set search path for database
ALTER DATABASE myapp SET search_path TO app, shared, public;

-- Show current search path
SHOW search_path;
```

### Multi-Tenant Organization

Use schemas for multi-tenant applications:

```sql
-- Create database
CREATE DATABASE saas_app;

-- Create tenant schemas
CREATE SCHEMA tenant_acme AUTHORIZATION tenant_user;
CREATE SCHEMA tenant_globex AUTHORIZATION tenant_user;
CREATE SCHEMA shared AUTHORIZATION admin;

-- Set search path for tenant
SET search_path TO tenant_acme, shared, public;
```

### Development Workflow

Organize development, staging, and production schemas:

```sql
-- Development
CREATE SCHEMA dev;
CREATE SCHEMA dev_experiments;

-- Staging
CREATE SCHEMA staging;

-- Production
CREATE SCHEMA prod;

-- Shared/reference data
CREATE SCHEMA reference;
```

---

## Best Practices

### Naming Conventions

- Use lowercase names for schemas (PostgreSQL folds unquoted identifiers to lowercase)
- Use underscores to separate words: `sales_data`, not `SalesData`
- Keep names concise but descriptive
- Avoid SQL keywords as schema names

### Organization

- Group related objects in schemas by function: `sales`, `inventory`, `reporting`
- Use schemas to separate concerns: `api`, `internal`, `analytics`
- Keep the `public` schema for shared or common objects
- Document schema purposes in database documentation

### Security

- Grant schema-level permissions to control access:
  ```sql
  GRANT USAGE ON SCHEMA app TO app_reader;
  GRANT CREATE ON SCHEMA app TO app_writer;
  ```
- Use schemas to implement role-based access control
- Separate sensitive data into restricted schemas

### Migration Safety

- Always use IF EXISTS when dropping in scripts
- Use CASCADE carefully and only when appropriate
- Test schema changes in development before production
- Back up data before destructive operations

---

## Known Limitations

### Partial Implementation

⚠️ **WITH Options** - Most CREATE DATABASE WITH options (TEMPLATE, ENCODING, LOCALE, etc.) are parsed for compatibility but not fully enforced at the storage layer. The database is created as an emulated namespace regardless of these parameters.

⚠️ **ALTER DATABASE Configuration** - SET/RESET configuration parameters are parsed but parameter validation is limited. Not all PostgreSQL configuration parameters are supported.

### Spec Deltas

📝 **Physical Separation** - Unlike native PostgreSQL where each database is a separate physical entity, ScratchBird creates logical namespaces within the emulation layer. Databases share the same underlying storage engine.

📝 **Connection Model** - Database-level connection limits and other connection parameters are parsed but may not affect actual connection handling, which is managed at the ScratchBird server level.

📝 **Templates** - Template database copying is not implemented. The TEMPLATE option is accepted but ignored.

---

## See Also

### Related Documentation
- [Tables and Constraints](02_tables_and_constraints.md)
- [Session Management](10_session_show_set.md)
- [Security (DCL)](09_security_dcl.md)

### Specifications
- `/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `/docs/specifications/ddl/DDL_DATABASES.md`
- `/docs/specifications/ddl/DDL_SCHEMAS.md`
- `/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`

### Source Code
- Parser: `/src/parser/postgresql/pg_parser_ddl.cpp`
- Executor: `/src/sblr/executor.cpp` (EXT_CREATE_DATABASE, EXT_CREATE_SCHEMA handlers)
