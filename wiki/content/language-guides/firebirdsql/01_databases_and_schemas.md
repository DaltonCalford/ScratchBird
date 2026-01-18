[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Firebird SQL - Database Management

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

Firebird SQL databases in ScratchBird are implemented as emulated database records within the ScratchBird catalog system. Unlike native Firebird installations where each database is a separate physical file, ScratchBird creates logical database namespaces that are stored in the unified catalog.

This approach allows multiple Firebird-compatible databases to coexist within a single ScratchBird instance, while maintaining Firebird SQL syntax compatibility for database operations.

**Key Concepts:**
- Each Firebird database is represented as an emulated schema path in the format: `remote.emulated.firebird.localhost.<database_name>`
- No physical `.fdb` or `.gdb` files are created
- Database-level properties (page size, character set, collation) are recorded in the catalog
- Traditional Firebird authentication (USER/PASSWORD) parameters are accepted but handled differently

---

## CREATE DATABASE

### Description

Creates a new Firebird-compatible database within the ScratchBird system. The database name can be specified as either a string literal (traditional Firebird style with file path) or as an identifier.

### Syntax

```sql
CREATE DATABASE database_path
    [ USER username ]
    [ PASSWORD password ]
    [ PAGE [ SIZE ] page_size ]
    [ DEFAULT CHARACTER SET charset_name ]
    [ DEFAULT COLLATION collation_name ]
```

### Parameters

- **database_path**: String literal or identifier specifying the database name. In traditional Firebird, this would be a file path like `'C:\data\employee.fdb'`. In ScratchBird, the path is converted to an emulated schema identifier.

- **USER username**: Optional. Specifies the database owner username. Can be an identifier or string literal. In ScratchBird, this is recorded but authentication is handled by the ScratchBird security system.

- **PASSWORD password**: Optional. Specifies the database owner password. Can be an identifier or string literal.

- **PAGE SIZE page_size**: Optional. Specifies the database page size in bytes. Traditional Firebird supports 4096, 8192, 16384, or 32768. ScratchBird records this setting but uses its own storage engine.

- **DEFAULT CHARACTER SET charset_name**: Optional. Sets the default character set for the database. Common values include `UTF8`, `ISO8859_1`, `WIN1252`, etc.

- **DEFAULT COLLATION collation_name**: Optional. Sets the default collation sequence for character comparisons.

### Examples

#### Basic Database Creation

```sql
CREATE DATABASE 'employee.fdb';
```

Creates a database named `employee.fdb` with default settings.

#### Database with Authentication

```sql
CREATE DATABASE 'employee.fdb'
    USER 'SYSDBA'
    PASSWORD 'masterkey';
```

Creates a database with specified user credentials. In traditional Firebird, `SYSDBA` is the administrative user with default password `masterkey`.

#### Database with Character Set

```sql
CREATE DATABASE 'myapp.fdb'
    USER 'SYSDBA'
    PASSWORD 'masterkey'
    DEFAULT CHARACTER SET UTF8;
```

Creates a database using UTF-8 character encoding by default.

#### Complete Configuration

```sql
CREATE DATABASE '/opt/firebird/data/production.fdb'
    USER 'SYSDBA'
    PASSWORD 'masterkey'
    PAGE SIZE 16384
    DEFAULT CHARACTER SET UTF8
    DEFAULT COLLATION UTF8;
```

Creates a database with all configuration options specified.

#### Using Identifier Syntax

```sql
CREATE DATABASE mydb
    USER admin
    PASSWORD secret123;
```

Creates a database using identifiers instead of string literals.

### Usage Notes

1. **Path Translation**: The database path (e.g., `'employee.fdb'`) is converted internally to the emulated schema path `remote.emulated.firebird.localhost.employee`.

2. **No Physical Files**: Unlike native Firebird, no `.fdb` file is created on the filesystem. All database metadata and data are stored in the ScratchBird catalog.

3. **Authentication**: While USER and PASSWORD are accepted for compatibility, ScratchBird uses its own security model for actual authentication and authorization.

4. **Page Size**: The PAGE SIZE parameter is recorded but doesn't affect physical storage layout in ScratchBird's storage engine.

5. **Character Set**: Setting a default character set affects how string literals and character data are interpreted in subsequent DDL and DML operations.

6. **Single Database Context**: After creation, you must explicitly connect to the database to perform operations within it.

### Related Features

- [DROP DATABASE](#drop-database) - Remove a database
- [ALTER DATABASE](#alter-database) - Modify database properties

---

## ALTER DATABASE

### Description

Modifies database-level properties. The Firebird parser in ScratchBird currently supports only a limited subset of ALTER DATABASE operations, specifically ALIAS management.

**Current Limitation**: Only ALIAS ADD and ALIAS DROP are implemented. Other ALTER DATABASE operations (OWNER, RENAME, SET options) are not supported and will generate parser errors.

### Syntax

```sql
ALTER DATABASE database_path ALIAS ADD alias_name

ALTER DATABASE database_path ALIAS DROP alias_name
```

### Parameters

- **database_path**: The path or name of the database to modify
- **alias_name**: The alias to add or remove

### Examples

#### Add Database Alias

```sql
ALTER DATABASE 'employee.fdb' ALIAS ADD employee;
```

Creates an alias named `employee` for the database file `employee.fdb`.

#### Remove Database Alias

```sql
ALTER DATABASE 'employee.fdb' ALIAS DROP employee;
```

Removes the alias `employee` from the database.

#### Multiple Aliases

```sql
-- Add multiple aliases (requires separate statements)
ALTER DATABASE 'production.fdb' ALIAS ADD prod;
ALTER DATABASE 'production.fdb' ALIAS ADD main_db;

-- Remove them later
ALTER DATABASE 'production.fdb' ALIAS DROP prod;
ALTER DATABASE 'production.fdb' ALIAS DROP main_db;
```

### Usage Notes

1. **Limited Functionality**: Unlike full Firebird, many ALTER DATABASE operations are not implemented:
   - Cannot change database owner
   - Cannot rename database
   - Cannot modify DEFAULT CHARACTER SET or other properties
   - Cannot add/drop difference files

2. **Parser Errors**: Attempting to use unsupported ALTER DATABASE clauses will result in a parser error.

3. **Alias Purpose**: Aliases allow you to reference a database by a simpler name rather than its full path.

### Related Features

- [CREATE DATABASE](#create-database) - Create a new database
- [DROP DATABASE](#drop-database) - Remove a database

---

## DROP DATABASE

### Description

Removes the current Firebird database from the ScratchBird catalog. This operation deletes all database metadata, tables, data, and other objects within the database.

**Important**: DROP DATABASE in Firebird operates on the currently connected database only. Unlike some SQL systems that allow you to drop a database by name while connected to a different database, Firebird requires you to be connected to the database you want to drop.

### Syntax

```sql
DROP DATABASE
```

### Parameters

None. The operation applies to the currently connected database.

### Examples

#### Drop Current Database

```sql
-- Connect to the database first
-- (connection method depends on your client)

-- Then drop it
DROP DATABASE;
```

#### Typical Usage Pattern

```sql
-- In an administrative script:
-- 1. Create a database
CREATE DATABASE 'test.fdb' USER 'SYSDBA' PASSWORD 'masterkey';

-- 2. Use the database (create tables, etc.)
CREATE TABLE test_table (id INT);

-- 3. When done, drop the database
DROP DATABASE;
```

### Usage Notes

1. **Current Database Only**: You must be connected to the database you want to drop. There is no syntax to drop a different database.

2. **Emulated Behavior**: In ScratchBird, this removes the emulated database record from the catalog. No physical file deletion occurs since databases are not stored as separate files.

3. **Permanent Operation**: This operation cannot be undone (unless you have backups or replication).

4. **Active Connections**: In traditional Firebird, you cannot drop a database if other connections are active. Check if ScratchBird enforces similar restrictions.

5. **Related Objects**: All tables, views, indexes, domains, and other objects within the database are removed.

6. **Transaction Context**: The DROP DATABASE operation typically commits any active transaction automatically.

### Safety Considerations

Always verify you're connected to the correct database before executing DROP DATABASE:

```sql
-- Check current database using system catalog
-- (exact query depends on ScratchBird implementation)

-- Only then drop
DROP DATABASE;
```

### Related Features

- [CREATE DATABASE](#create-database) - Create a new database
- [ALTER DATABASE](#alter-database) - Modify database properties

---

## SCHEMA Operations

### Description

Firebird SQL does not support schemas as a first-class DDL object. In traditional Firebird architecture, each database file represents a complete namespace, and there is no schema subdivision within a database.

**Status**: Not supported by dialect design.

### Background

Unlike PostgreSQL, Oracle, or SQL Server, which support multiple schemas within a single database, Firebird uses a single-namespace model:

- **Database = Namespace**: Each Firebird database is its own complete namespace
- **No CREATE SCHEMA**: There is no `CREATE SCHEMA` statement in Firebird SQL
- **No Schema Qualification**: Object names are not schema-qualified (no `schema.table` syntax)

### Alternative Approaches

In Firebird, to achieve schema-like organization:

1. **Multiple Databases**: Create separate databases for different logical schemas
2. **Naming Conventions**: Use prefixes in object names (e.g., `HR_EMPLOYEES`, `SALES_ORDERS`)
3. **Database Aliases**: Use aliases to make database names more manageable

### Example: Organization Without Schemas

```sql
-- Instead of schemas, use separate databases:

CREATE DATABASE 'hr.fdb' USER 'SYSDBA' PASSWORD 'masterkey';
-- Create HR tables here: EMPLOYEES, DEPARTMENTS, etc.

CREATE DATABASE 'sales.fdb' USER 'SYSDBA' PASSWORD 'masterkey';
-- Create Sales tables here: ORDERS, CUSTOMERS, etc.

-- Or use a single database with prefixed names:
CREATE DATABASE 'company.fdb' USER 'SYSDBA' PASSWORD 'masterkey';
CREATE TABLE HR_EMPLOYEES (id INT, name VARCHAR(100));
CREATE TABLE SALES_ORDERS (id INT, customer_id INT);
```

### ScratchBird-Native Schemas

Note that ScratchBird's native V2 parser does support `CREATE SCHEMA`, but this is not available when using Firebird emulation mode. Schema operations are ScratchBird-native only and not part of Firebird SQL compatibility.

---

## TABLESPACE Commands

### Description

Firebird does not expose tablespaces as a SQL DDL object. Storage is managed at
the database file level (single namespace per database).

### Status

**NOT APPLICABLE:** No CREATE/ALTER/DROP TABLESPACE syntax exists in Firebird,
and the ScratchBird Firebird emulation layer does not provide tablespace DDL.

---

## Known Limitations

### Partial Implementation

**ALTER DATABASE**
- Only ALIAS ADD and ALIAS DROP are supported
- Other Firebird ALTER DATABASE operations (OWNER, RENAME, SET DEFAULT CHARACTER SET, etc.) are not implemented
- Parser will generate errors for unsupported clauses

### Implementation Deltas

**CREATE DATABASE**
- **Emulated Path Mapping**: Database paths are converted to emulated schema identifiers (`remote.emulated.firebird.localhost.<dbname>`) rather than creating physical files
- **No Physical Files**: Unlike native Firebird, no `.fdb` or `.gdb` files are created
- **Authentication**: USER/PASSWORD parameters are recorded but authentication is handled by ScratchBird's security system
- **Page Size**: PAGE SIZE is recorded but doesn't affect actual storage layout (ScratchBird uses its own storage engine)

**DROP DATABASE**
- **Emulated Behavior**: Removes catalog entries rather than deleting physical files
- **Connection Requirements**: Verify whether ScratchBird enforces Firebird's restriction that no other connections can be active when dropping a database

### Missing Features

**Schema Support**
- CREATE SCHEMA, ALTER SCHEMA, DROP SCHEMA are not available (by dialect design - Firebird doesn't support schemas)
- Schema-qualified object names (schema.table) are not supported in Firebird emulation mode

**ALTER DATABASE Extended Operations**
- Cannot change database OWNER
- Cannot RENAME database
- Cannot modify DEFAULT CHARACTER SET after creation
- Cannot add/drop DIFFERENCE FILE
- Cannot BEGIN/END BACKUP operations
- Cannot modify database-level SET options

### Specification References

- `/home/dcalford/CliWork/ScratchBird/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`
- `/home/dcalford/CliWork/ScratchBird/docs/specifications/reference/firebird/`
- `/home/dcalford/CliWork/ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`
