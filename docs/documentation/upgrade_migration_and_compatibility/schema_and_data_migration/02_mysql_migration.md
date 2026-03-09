# MySQL Migration Guide

[Schema and Data Migration README](../README.md)

## Synopsis

Migrate databases from MySQL to ScratchBird.

## Migration Methods

### Method 1: Dump and Import

```bash
# 1. Create MySQL dump
mysqldump -h mysql_host -u root --single-transaction --routines mydb > mydb.sql

# 2. Convert schema (may need manual edits)
# Replace AUTO_INCREMENT with SERIAL
# Replace backticks with double quotes
# Convert data types

# 3. Create SB database
sb_isql -c "CREATE DATABASE mydb;"

# 4. Import
sb_my_isql -d mydb -f mydb.sql
```

### Method 2: FDW Migration

```sql
-- Set up MySQL FDW
CREATE EXTENSION mysql_fdw;

CREATE SERVER mysql_source
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (host 'mysql_host', port '3306');

CREATE USER MAPPING FOR current_user
    SERVER mysql_source
    OPTIONS (username 'root', password 'secret');

-- Import tables
CREATE FOREIGN TABLE mysql_users (
    id INT,
    name VARCHAR(255),
    email VARCHAR(255)
)
SERVER mysql_source
OPTIONS (database 'mydb', table_name 'users');

-- Copy data
CREATE TABLE local_users AS SELECT * FROM mysql_users;
```

## Schema Conversion

### Data Type Mapping

| MySQL | ScratchBird | Conversion |
|-------|-------------|------------|
| `TINYINT` | `SMALLINT` | Expand to 2 bytes |
| `TINYINT(1)` | `BOOLEAN` | Use boolean |
| `INT` | `INTEGER` | Direct |
| `BIGINT` | `BIGINT` | Direct |
| `VARCHAR(n)` | `VARCHAR(n)` | Direct |
| `TEXT` | `TEXT` | Direct |
| `DATETIME` | `TIMESTAMP` | No timezone |
| `TIMESTAMP` | `TIMESTAMPTZ` | With timezone |
| `JSON` | `JSONB` | Binary JSON |
| `ENUM` | `VARCHAR` + CHECK | Add constraint |

### ENUM Conversion

```sql
-- MySQL
CREATE TABLE users (
    status ENUM('active', 'inactive', 'pending') DEFAULT 'pending'
);

-- ScratchBird
CREATE TABLE users (
    status VARCHAR(20) DEFAULT 'pending'
        CHECK (status IN ('active', 'inactive', 'pending'))
);
```

### AUTO_INCREMENT Conversion

```sql
-- MySQL
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(255)
);

-- ScratchBird
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255)
);

-- Or with IDENTITY
CREATE TABLE users (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name VARCHAR(255)
);
```

## SQL Differences

### LIMIT Syntax

| MySQL | ScratchBird |
|-------|-------------|
| `LIMIT 10` | `LIMIT 10` |
| `LIMIT 10, 20` | `LIMIT 20 OFFSET 10` |
| `LIMIT 10 OFFSET 20` | `LIMIT 10 OFFSET 20` |

### String Concatenation

| MySQL | ScratchBird |
|-------|-------------|
| `CONCAT(a, b, c)` | `a \|\| b \|\| c` or `CONCAT(a, b, c)` |

### Date Functions

| MySQL | ScratchBird |
|-------|-------------|
| `NOW()` | `NOW()` |
| `CURDATE()` | `CURRENT_DATE` |
| `DATE_ADD(d, INTERVAL 1 DAY)` | `d + INTERVAL '1 day'` |
| `DATEDIFF(d1, d2)` | `d1 - d2` |

## Index Migration

```sql
-- MySQL
CREATE INDEX idx_name ON users(name);
CREATE UNIQUE INDEX idx_email ON users(email);

-- ScratchBird (same syntax)
CREATE INDEX idx_name ON users(name);
CREATE UNIQUE INDEX idx_email ON users(email);
```

## Stored Procedures

MySQL stored procedures need conversion to PL/pgSQL:

```sql
-- MySQL
DELIMITER //
CREATE PROCEDURE GetUserCount()
BEGIN
    SELECT COUNT(*) FROM users;
END //
DELIMITER ;

-- ScratchBird
CREATE OR REPLACE FUNCTION get_user_count()
RETURNS INTEGER AS $$
DECLARE
    v_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO v_count FROM users;
    RETURN v_count;
END;
$$ LANGUAGE plpgsql;
```

## Verification

```sql
-- Check table counts
SELECT 'users' as table, count(*) FROM users
UNION ALL
SELECT 'orders', count(*) FROM orders;

-- Verify data types
SELECT column_name, data_type
FROM information_schema.columns
WHERE table_name = 'users';

-- Check indexes
SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'users';
```

## Post-Migration Tasks

1. **Update Application Configuration**
   ```
   # MySQL
   mysql://user:pass@mysql_host/mydb
   
   # ScratchBird
   mysql://user:pass@sb_host:3306/mydb
   ```

2. **Optimize Tables**
   ```sql
   ANALYZE users;
   ANALYZE orders;
   ```

3. **Review Execution Plans**
   ```sql
   EXPLAIN SELECT * FROM users WHERE email = 'test@example.com';
   ```

## Common Issues

| Issue | Solution |
|-------|----------|
| Backtick identifiers | Replace with double quotes |
| AUTO_INCREMENT | Replace with SERIAL |
| ENUM types | Replace with CHECK constraint |
| GROUP BY with non-aggregated columns | Add to GROUP BY or use aggregate |

## See Also

- [PostgreSQL Migration](01_postgresql_migration.md)
- [Firebird Migration](03_firebird_migration.md)
