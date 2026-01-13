# MySQL Databases and Schemas

## Overview

In MySQL emulation mode, ScratchBird provides database management capabilities that closely mirror MySQL 8.x syntax and behavior. MySQL treats `DATABASE` and `SCHEMA` as synonyms, and this implementation maintains that compatibility.

This document covers all database-level DDL operations including creation, modification, and deletion of databases. ScratchBird's MySQL emulation layer translates these commands into SBLR bytecode that manages database metadata within the catalog system.

## CREATE DATABASE

Creates a new database with optional character set and collation settings.

### Syntax

```sql
CREATE DATABASE [IF NOT EXISTS] database_name
    [[DEFAULT] CHARACTER SET charset_name]
    [[DEFAULT] COLLATE collation_name]
```

### Parameters

- `IF NOT EXISTS`: Optional clause to prevent errors if the database already exists
- `database_name`: Name of the database to create (must be a valid identifier)
- `CHARACTER SET charset_name`: Specifies the default character set for the database
- `COLLATE collation_name`: Specifies the default collation for the database

### Examples

**Basic database creation:**
```sql
CREATE DATABASE myapp;
```

**Create database with IF NOT EXISTS:**
```sql
CREATE DATABASE IF NOT EXISTS myapp;
```

**Create database with character set:**
```sql
CREATE DATABASE myapp CHARACTER SET utf8mb4;
```

**Create database with character set and collation:**
```sql
CREATE DATABASE myapp
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;
```

**Create database with specific collation:**
```sql
CREATE DATABASE myapp COLLATE utf8mb4_0900_ai_ci;
```

### Notes

- The `DEFAULT` keyword before `CHARACTER SET` and `COLLATE` is optional and has no effect
- Character sets and collations are stored in the database metadata
- When you create tables in this database, they inherit these defaults unless explicitly overridden
- Valid character set names include: `utf8`, `utf8mb4`, `latin1`, `ascii`, and others
- Valid collation names depend on the character set chosen

### Cross-References

- See [ALTER DATABASE](#alter-database) for modifying database properties
- See [DROP DATABASE](#drop-database--drop-schema) for removing databases
- See [USE](10_session_show_set.md#use) for switching to a database
- See [SHOW DATABASES](10_session_show_set.md#show-databases) for listing databases

---

## ALTER DATABASE

Modifies the default character set or collation for an existing database.

### Syntax

```sql
ALTER DATABASE database_name
    [[DEFAULT] CHARACTER SET charset_name]
    [[DEFAULT] COLLATE collation_name]
```

### Parameters

- `database_name`: Name of the database to modify
- `CHARACTER SET charset_name`: New default character set for the database
- `COLLATE collation_name`: New default collation for the database

### Examples

**Change database character set:**
```sql
ALTER DATABASE myapp CHARACTER SET utf8mb4;
```

**Change database collation:**
```sql
ALTER DATABASE myapp COLLATE utf8mb4_0900_ai_ci;
```

**Change both character set and collation:**
```sql
ALTER DATABASE myapp
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;
```

### Notes

- Changing the database character set or collation does NOT alter existing tables
- Only newly created tables will use the new defaults
- To change existing tables, you must use `ALTER TABLE` statements
- The `DEFAULT` keyword is optional

### Limitations

- The `ALTER DATABASE ... RENAME TO ...` syntax is parsed but currently rejected with an error
- Other MySQL `ALTER DATABASE` options (encryption, read-only, etc.) are not supported

### Cross-References

- See [CREATE DATABASE](#create-database) for creating databases
- See [SHOW CREATE DATABASE](10_session_show_set.md#show-create-database) to view database definition

---

## DROP DATABASE / DROP SCHEMA

Drops (deletes) an existing database and all its contents.

### Syntax

```sql
DROP DATABASE [IF EXISTS] database_name
DROP SCHEMA [IF EXISTS] schema_name
```

### Parameters

- `IF EXISTS`: Optional clause to prevent errors if the database does not exist
- `database_name` / `schema_name`: Name of the database to drop

### Examples

**Drop a database:**
```sql
DROP DATABASE myapp;
```

**Drop database with IF EXISTS:**
```sql
DROP DATABASE IF EXISTS myapp;
```

**Using SCHEMA synonym:**
```sql
DROP SCHEMA IF EXISTS myapp;
```

### Notes

- `DATABASE` and `SCHEMA` are complete synonyms in MySQL
- Dropping a database removes all tables, views, and other objects within it
- This operation cannot be undone
- If the database does not exist and `IF EXISTS` is not specified, an error occurs
- Active connections to the database may prevent it from being dropped

### Safety Warning

**WARNING:** This operation permanently deletes all data in the database. Always ensure you have backups before dropping a database in production.

### Cross-References

- See [CREATE DATABASE](#create-database) for creating databases
- See [SHOW DATABASES](10_session_show_set.md#show-databases) for listing databases

---

## MySQL Schema Semantics

### DATABASE vs SCHEMA

In MySQL, `DATABASE` and `SCHEMA` are complete synonyms. Unlike PostgreSQL which has separate database and schema concepts, MySQL uses these terms interchangeably to refer to the same logical container for database objects.

**Examples of equivalence:**
```sql
-- These are identical:
CREATE DATABASE mydb;
CREATE SCHEMA mydb;

-- These are identical:
DROP DATABASE mydb;
DROP SCHEMA mydb;

-- These are identical:
SHOW DATABASES;
SHOW SCHEMAS;
```

### Notes

- There is no separate `CREATE SCHEMA` statement distinct from `CREATE DATABASE`
- All schema-related operations use the same underlying database mechanism
- When working with ScratchBird's MySQL emulation, think of schemas as databases

### Cross-References

- See [PostgreSQL documentation](../postgresql/01_databases_and_schemas.md) for PostgreSQL's different schema model

---

## CREATE TABLESPACE

Creates a tablespace (MySQL syntax).

### Syntax (MySQL)

```sql
CREATE TABLESPACE tablespace_name
    ADD DATAFILE 'file_name'
    [ENGINE = engine_name];
```

### Examples

```sql
CREATE TABLESPACE ts_fast ADD DATAFILE 'ts_fast.ibd' ENGINE=InnoDB;
```

### Status

**NOT IMPLEMENTED:** MySQL parser does not accept CREATE TABLESPACE.

---

## ALTER TABLESPACE

Alters a tablespace (MySQL syntax).

### Syntax (MySQL)

```sql
ALTER TABLESPACE tablespace_name
    ADD DATAFILE 'file_name';
```

### Examples

```sql
ALTER TABLESPACE ts_fast ADD DATAFILE 'ts_fast_02.ibd';
```

### Status

**NOT IMPLEMENTED:** MySQL parser does not accept ALTER TABLESPACE.

---

## DROP TABLESPACE

Drops a tablespace (MySQL syntax).

### Syntax (MySQL)

```sql
DROP TABLESPACE tablespace_name;
```

### Examples

```sql
DROP TABLESPACE ts_fast;
```

### Status

**NOT IMPLEMENTED:** MySQL parser does not accept DROP TABLESPACE.

---

## Known Limitations

### Partial Implementation

- **ALTER DATABASE RENAME**: The syntax `ALTER DATABASE old_name RENAME TO new_name` is parsed but currently rejected with an error message. This feature is not yet implemented.

### Missing Features

- **Database-level encryption options**: MySQL 8.0's `ENCRYPTION` clause is not supported
- **Read-only databases**: MySQL 8.0's `READ ONLY` option is not supported
- **Other ALTER DATABASE options**: Advanced MySQL 8.0 options like default table encryption are not supported

### Spec Deltas

- **Character set/collation handling**: While character sets and collations are stored in metadata, the full collation behavior may differ from MySQL in some edge cases
- **Database naming restrictions**: ScratchBird's identifier rules may differ slightly from MySQL's naming conventions
