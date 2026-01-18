# SQL Language Guides

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

---

## Overview

ScratchBird supports multiple SQL dialects, each accessible via a dedicated network port. This guide helps you choose the right dialect for your use case.

---

## Available Dialects

| Dialect | Port | CLI Tool | Use Case |
|---------|------|----------|----------|
| Native ScratchBird | 3092 | `sb-isql` | Full feature access |
| PostgreSQL | 5432 | `sb-pg-isql` | PostgreSQL client compatibility |
| MySQL | 3306 | `sb-my-isql` | MySQL client compatibility |
| Firebird | 3050 | `sb-fb-isql` | Firebird migration |

---

## Dialect Documentation

### Native ScratchBird SQL

ScratchBird's native SQL dialect with full feature access.

**Port:** 3092
**CLI Tool:** `sb-isql`

[Native SQL Guide](native/README.md)

**Topics:**
- [Databases and Schemas](native/01_databases_and_schemas.md)
- [Tables and Constraints](native/02_tables_and_constraints.md)
- [Indexes, Views, Sequences](native/03_indexes_views_sequences.md)
- [Types and Domains](native/04_types_and_domains.md)
- [Programmable SQL](native/05_programmable_sql.md)
- [DML: SELECT](native/06_dml_select.md)
- [DML: Modification](native/07_dml_modification.md)
- [Transactions](native/08_transactions.md)
- [Security (DCL)](native/09_security_dcl.md)
- [Session, SHOW, SET](native/10_session_show_set.md)
- [Utilities](native/11_utilities.md)
- [Operators](native/12_operators.md)
- [System Catalog](native/13_system_catalog.md)
- [Functions](native/14_functions.md)

---

### PostgreSQL Compatibility

Connect PostgreSQL clients (`psql`, JDBC, Python `psycopg2`) to ScratchBird.

**Port:** 5432
**CLI Tool:** `sb-pg-isql`

[PostgreSQL Guide](postgresql/README.md)

**Features:**
- PostgreSQL wire protocol
- `pg_catalog.*` function emulation
- Information schema
- PostgreSQL-style type casting (`::type`)
- Dollar-quoted strings
- Array syntax

**Example:**
```bash
psql -h localhost -p 5432 -U myuser -d mydb
```

---

### MySQL Compatibility

Connect MySQL clients (`mysql` CLI, JDBC, PHP mysqli) to ScratchBird.

**Port:** 3306
**CLI Tool:** `sb-my-isql`

[MySQL Guide](mysql/README.md)

**Features:**
- MySQL wire protocol
- SHOW commands (SHOW TABLES, SHOW DATABASES, etc.)
- Backtick identifier quoting
- AUTO_INCREMENT syntax
- MySQL-style comments

**Example:**
```bash
mysql -h localhost -P 3306 -u myuser -p mydb
```

---

### Firebird Compatibility

Connect Firebird clients (isql, JDBC, .NET driver) to ScratchBird.

**Port:** 3050
**CLI Tool:** `sb-fb-isql`

[Firebird Guide](firebirdsql/README.md)

**Features:**
- Firebird wire protocol
- RDB$* system table queries
- Generator (sequence) syntax
- EXECUTE BLOCK
- Firebird stored procedure syntax

**Example:**
```bash
isql -user SYSDBA -password masterkey localhost:3050:/path/to/database.sbd
```

---

## Choosing a Dialect

### Use Native ScratchBird When:

- Building new applications from scratch
- Need access to all ScratchBird features
- No existing client compatibility requirements
- Want the best performance

### Use PostgreSQL Emulation When:

- Migrating from PostgreSQL
- Using PostgreSQL-specific tools (pg_dump, pg_restore)
- Applications use PostgreSQL drivers/libraries
- Need pg_catalog views for tooling

### Use MySQL Emulation When:

- Migrating from MySQL/MariaDB
- Applications use MySQL drivers/libraries
- Need SHOW command compatibility
- Tools expect MySQL protocol

### Use Firebird Emulation When:

- Migrating from Firebird
- Leveraging existing Firebird applications
- Need RDB$ system table access
- Applications use Firebird drivers

---

## Feature Comparison

| Feature | Native | PostgreSQL | MySQL | Firebird |
|---------|--------|------------|-------|----------|
| Full DDL | Yes | Yes | Yes | Yes |
| Full DML | Yes | Yes | Yes | Yes |
| Transactions | Yes | Yes | Yes | Yes |
| MGA visibility | Yes | Yes | Yes | Yes |
| JSONB | Yes | Yes | Yes | Limited |
| Arrays | Yes | Yes | No | No |
| Spatial | Yes | Yes | No | No |
| Full-text search | Yes | Yes | Limited | No |
| System catalog | sb_catalog | pg_catalog | information_schema | RDB$ |
| Stored procedures | Yes | Partial | Partial | Partial |

---

## Common Operations by Dialect

### Create Table

```sql
-- Native / PostgreSQL / Firebird
CREATE TABLE users (
    id INT PRIMARY KEY,
    name VARCHAR(100)
);

-- MySQL (with AUTO_INCREMENT)
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100)
);
```

### List Tables

```sql
-- Native
SELECT * FROM sb_catalog.tables;

-- PostgreSQL
SELECT * FROM pg_catalog.pg_tables WHERE schemaname = 'public';

-- MySQL
SHOW TABLES;

-- Firebird
SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0;
```

### Current Database

```sql
-- Native / PostgreSQL
SELECT current_database();

-- MySQL
SELECT DATABASE();

-- Firebird
SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_NAME') FROM RDB$DATABASE;
```

---

## Switching Dialects

You can connect to the same database using different dialects simultaneously. Each connection uses the dialect's parser but all access the same underlying data.

```bash
# Three connections to the same database using different dialects
sb-isql -p 3092 -d mydb          # Native
psql -p 5432 mydb                 # PostgreSQL
mysql -P 3306 mydb                # MySQL
```

---

## Related Documents

- [SQL Syntax Reference](../reference/SQL-Syntax.md)
- [Functions Reference](../reference/Functions.md)
- [Data Types Reference](../reference/Data-Types.md)
- [Developer Guide: Parsers](../developer-guide/Parsers.md)
