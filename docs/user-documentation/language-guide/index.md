# SQL Language Guide

Complete SQL reference for ScratchBird.

[Back to Documentation Index](../index.md)

---

## Overview

ScratchBird uses standard SQL with PostgreSQL-style extensions. When connected via MySQL or Firebird protocols, their specific syntax is also accepted.

---

## Language Sections

### Data Definition Language (DDL)

| Topic | Description |
|-------|-------------|
| [CREATE DATABASE](ddl/create-database.md) | Create databases |
| [CREATE TABLE](ddl/create-table.md) | Create tables |
| [CREATE INDEX](ddl/create-index.md) | Create indexes |
| [CREATE VIEW](ddl/create-view.md) | Create views |
| [ALTER TABLE](ddl/alter-table.md) | Modify tables |
| [DROP Statements](ddl/drop-statements.md) | Remove objects |

### Data Manipulation Language (DML)

| Topic | Description |
|-------|-------------|
| [SELECT](dml/select.md) | Query data |
| [INSERT](dml/insert.md) | Add rows |
| [UPDATE](dml/update.md) | Modify rows |
| [DELETE](dml/delete.md) | Remove rows |
| [MERGE](dml/merge.md) | Upsert operations |

### Procedural SQL (PSQL)

| Topic | Description |
|-------|-------------|
| [Stored Procedures](psql/procedures.md) | Server-side procedures |
| [Functions](psql/functions.md) | User-defined functions |
| [Triggers](psql/triggers.md) | Event-driven code |
| [Exceptions](psql/exceptions.md) | Error handling |

### Built-in Functions

| Topic | Description |
|-------|-------------|
| [String Functions](functions/string-functions.md) | Text manipulation |
| [Numeric Functions](functions/numeric-functions.md) | Math operations |
| [Date Functions](functions/date-functions.md) | Date/time operations |
| [JSON Functions](functions/json-functions.md) | JSON manipulation |
| [Aggregate Functions](functions/aggregate-functions.md) | GROUP BY aggregates |
| [Window Functions](functions/window-functions.md) | Analytical functions |

### Data Types

| Topic | Description |
|-------|-------------|
| [Numeric Types](data-types/numeric-types.md) | INTEGER, DECIMAL, etc. |
| [String Types](data-types/string-types.md) | VARCHAR, TEXT, etc. |
| [Date/Time Types](data-types/date-time-types.md) | DATE, TIMESTAMP, etc. |
| [JSON Types](data-types/json-types.md) | JSON and JSONB |
| [Special Types](data-types/special-types.md) | UUID, BOOLEAN, arrays |

### Migration Guides

| Topic | Description |
|-------|-------------|
| [Python to PSQL](python-to-psql-migration.md) | Map Python idioms to PSQL equivalents |

---

## SQL Syntax Basics

### Identifiers

```sql
-- Unquoted (case-insensitive)
SELECT name FROM users;

-- Quoted (case-sensitive, special characters)
SELECT "Name" FROM "User Table";
```

### Literals

```sql
-- String
'Hello, World'
'It''s escaped'

-- Numbers
42
3.14
1.5e10

-- Boolean
TRUE, FALSE

-- NULL
NULL
```

### Comments

```sql
-- Single line comment

/* Multi-line
   comment */
```

---

## Operators

### Comparison

| Operator | Description |
|----------|-------------|
| `=` | Equal |
| `<>` or `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less or equal |
| `>=` | Greater or equal |
| `BETWEEN` | Range check |
| `IN` | List membership |
| `LIKE` | Pattern match |
| `IS NULL` | NULL check |

### Logical

| Operator | Description |
|----------|-------------|
| `AND` | Logical and |
| `OR` | Logical or |
| `NOT` | Logical not |

### Arithmetic

| Operator | Description |
|----------|-------------|
| `+` | Addition |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `%` | Modulo |

---

## Transactions

```sql
-- Start transaction
BEGIN;
-- or
START TRANSACTION;

-- Commit
COMMIT;

-- Rollback
ROLLBACK;

-- Savepoint
SAVEPOINT mypoint;
ROLLBACK TO mypoint;
RELEASE SAVEPOINT mypoint;
```

---

## Protocol-Specific Syntax

### PostgreSQL Style (Default)

```sql
-- Parameter markers
SELECT * FROM users WHERE id = $1;

-- LIMIT/OFFSET
SELECT * FROM users LIMIT 10 OFFSET 20;
```

### MySQL Style

```sql
-- Parameter markers
SELECT * FROM users WHERE id = ?;

-- LIMIT with offset
SELECT * FROM users LIMIT 20, 10;

-- Backtick identifiers
SELECT `name` FROM `users`;
```

### Firebird Style

```sql
-- Parameter markers
SELECT * FROM users WHERE id = ?;

-- FIRST/SKIP
SELECT FIRST 10 SKIP 20 * FROM users;
```

---

## Quick Reference

### Create Table

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

### Insert

```sql
INSERT INTO users (name, email)
VALUES ('Alice', 'alice@example.com');
```

### Select

```sql
SELECT name, email
FROM users
WHERE created_at > '2024-01-01'
ORDER BY name
LIMIT 10;
```

### Update

```sql
UPDATE users
SET email = 'new@example.com'
WHERE id = 1;
```

### Delete

```sql
DELETE FROM users WHERE id = 1;
```

### Join

```sql
SELECT u.name, o.total
FROM users u
JOIN orders o ON u.id = o.user_id
WHERE o.status = 'completed';
```

---

## See Also

- [Basic SQL Tutorial](../getting-started/basic-sql.md)
- [Glossary](../glossary.md)
