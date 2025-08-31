# ScratchBird Core SQL Dialect Specification

## Overview
ScratchBird's core SQL dialect is designed to be a superset of SQL:2016 with the best features from all major databases.

## Design Principles

1. **ANSI SQL Compliance**: Follow SQL:2016 standard as baseline
2. **Best of All Worlds**: Include the best features from each database
3. **Predictable Behavior**: Clear, consistent semantics
4. **Performance First**: Syntax should enable optimization
5. **Migration Friendly**: Easy to migrate from any database

## Lexical Structure

### Identifiers

```sql
-- Supported quote styles (configurable per connection)
SELECT "column_name" FROM "table";      -- ANSI standard (default)
SELECT `column_name` FROM `table`;      -- MySQL style
SELECT [column_name] FROM [table];      -- MSSQL style

-- Case sensitivity (configurable per database)
-- Default: Case-preserving, case-insensitive comparison
CREATE TABLE MyTable (MyColumn INT);
SELECT mycolumn FROM mytable;  -- Works
```

### Comments

```sql
-- Single line comment

/* Multi-line
   comment */

# MySQL-style comment (compatibility mode)
```

### String Literals

```sql
-- Standard string literals
'This is a string'

-- Escaped quotes
'It''s a string'  -- Standard
'It\'s a string'  -- MySQL compatibility

-- Unicode strings
N'Unicode string'  -- MSSQL style
U&'Unicode \0041'  -- PostgreSQL style

-- Dollar quoting (PostgreSQL style)
$$This is a string with 'quotes' and "more quotes"$$
```

### Constants

```sql
-- Boolean
TRUE, FALSE, NULL

-- Numbers
123                  -- Integer
123.456             -- Decimal
1.23e-4             -- Scientific notation
0x1AF               -- Hexadecimal
0b101010            -- Binary

-- Temporal
DATE '2024-01-01'
TIME '12:34:56'
TIMESTAMP '2024-01-01 12:34:56'
INTERVAL '1 DAY'
```

## Data Types

### Core Types

```sql
-- Integers
TINYINT, SMALLINT, INTEGER, BIGINT

-- Decimals
DECIMAL(p,s), NUMERIC(p,s)

-- Floating point
REAL, DOUBLE PRECISION

-- Strings
CHAR(n), VARCHAR(n), TEXT

-- Binary
BINARY(n), VARBINARY(n), BLOB

-- Temporal
DATE, TIME, TIMESTAMP, INTERVAL

-- Boolean
BOOLEAN

-- UUID
UUID

-- JSON
JSON, JSONB  -- JSONB is binary, indexed

-- Arrays (PostgreSQL style)
INTEGER[], TEXT[][], etc.
```

## DDL (Data Definition Language)

### CREATE TABLE

```sql
CREATE [TEMPORARY] TABLE [IF NOT EXISTS] table_name (
    -- Column definitions
    id INTEGER PRIMARY KEY GENERATED ALWAYS AS IDENTITY,
    uuid UUID DEFAULT gen_random_uuid(),
    name VARCHAR(100) NOT NULL,
    email VARCHAR(255) UNIQUE,
    age INTEGER CHECK (age >= 0),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    
    -- Table constraints
    PRIMARY KEY (id),
    UNIQUE (email),
    CHECK (age >= 18 OR parent_id IS NOT NULL),
    FOREIGN KEY (parent_id) REFERENCES users(id) ON DELETE CASCADE
) [WITH (options)];

-- Table options
WITH (
    fillfactor = 80,
    autovacuum_enabled = true,
    compression = lz4
);
```

### ALTER TABLE

```sql
-- Add column
ALTER TABLE users ADD COLUMN phone VARCHAR(20);

-- Drop column
ALTER TABLE users DROP COLUMN phone;

-- Rename column
ALTER TABLE users RENAME COLUMN email TO email_address;

-- Change type
ALTER TABLE users ALTER COLUMN age TYPE BIGINT;

-- Add constraint
ALTER TABLE users ADD CONSTRAINT check_age CHECK (age >= 0);

-- Drop constraint
ALTER TABLE users DROP CONSTRAINT check_age;
```

### Indexes

```sql
-- B-Tree index (default)
CREATE [UNIQUE] INDEX idx_name ON table(column1, column2);

-- Partial index
CREATE INDEX idx_active ON users(email) WHERE active = true;

-- Expression index
CREATE INDEX idx_lower_email ON users(LOWER(email));

-- Include columns (covering index)
CREATE INDEX idx_email ON users(email) INCLUDE (name, phone);

-- Specific index types
CREATE INDEX idx_gin ON documents USING GIN (content);
CREATE INDEX idx_hash ON users USING HASH (email);
CREATE INDEX idx_rtree ON locations USING RTREE (coordinates);
```

## DML (Data Manipulation Language)

### INSERT

```sql
-- Basic insert
INSERT INTO users (name, email) VALUES ('John', 'john@example.com');

-- Multi-row insert
INSERT INTO users (name, email) VALUES 
    ('John', 'john@example.com'),
    ('Jane', 'jane@example.com');

-- Insert from select
INSERT INTO users_archive SELECT * FROM users WHERE created_at < '2020-01-01';

-- UPSERT (PostgreSQL style)
INSERT INTO users (id, name) VALUES (1, 'John')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name;

-- UPSERT (MySQL style)
INSERT INTO users (id, name) VALUES (1, 'John')
ON DUPLICATE KEY UPDATE name = VALUES(name);

-- RETURNING clause
INSERT INTO users (name) VALUES ('John') RETURNING id, created_at;
```

### UPDATE

```sql
-- Basic update
UPDATE users SET email = 'new@example.com' WHERE id = 1;

-- Update with JOIN
UPDATE users u
SET status = 'premium'
FROM orders o
WHERE u.id = o.user_id AND o.total > 1000;

-- Update with RETURNING
UPDATE users SET status = 'active' WHERE id = 1 RETURNING *;
```

### DELETE

```sql
-- Basic delete
DELETE FROM users WHERE id = 1;

-- Delete with JOIN
DELETE FROM users
USING orders
WHERE users.id = orders.user_id AND orders.status = 'cancelled';

-- Delete with RETURNING
DELETE FROM users WHERE status = 'inactive' RETURNING id;
```

### MERGE (UPSERT)

```sql
MERGE INTO target_table AS t
USING source_table AS s
ON t.id = s.id
WHEN MATCHED THEN
    UPDATE SET t.value = s.value
WHEN NOT MATCHED THEN
    INSERT (id, value) VALUES (s.id, s.value)
WHEN NOT MATCHED BY SOURCE THEN
    DELETE;
```

## SELECT Statement

### Basic SELECT

```sql
SELECT [DISTINCT] column1, column2
FROM table
WHERE condition
GROUP BY column1
HAVING aggregate_condition
ORDER BY column1 [ASC|DESC] [NULLS FIRST|LAST]
LIMIT count OFFSET start;
```

### JOINs

```sql
-- Inner join
SELECT * FROM users u
INNER JOIN orders o ON u.id = o.user_id;

-- Left/Right/Full outer join
SELECT * FROM users u
LEFT JOIN orders o ON u.id = o.user_id;

-- Cross join
SELECT * FROM users CROSS JOIN products;

-- Natural join
SELECT * FROM users NATURAL JOIN addresses;

-- LATERAL join (PostgreSQL) / CROSS APPLY (MSSQL)
SELECT * FROM users u
CROSS JOIN LATERAL (
    SELECT * FROM orders o 
    WHERE o.user_id = u.id 
    ORDER BY created_at DESC 
    LIMIT 5
) AS recent_orders;
```

### Common Table Expressions (CTEs)

```sql
WITH RECURSIVE cte_name (columns) AS (
    -- Anchor query
    SELECT ...
    UNION [ALL]
    -- Recursive query
    SELECT ... FROM cte_name ...
)
SELECT * FROM cte_name;

-- Multiple CTEs
WITH 
    cte1 AS (SELECT ...),
    cte2 AS (SELECT ... FROM cte1 ...)
SELECT * FROM cte2;
```

### Window Functions

```sql
SELECT 
    name,
    salary,
    ROW_NUMBER() OVER (ORDER BY salary DESC) as rank,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) as dept_rank,
    SUM(salary) OVER (PARTITION BY department) as dept_total,
    LAG(salary, 1) OVER (ORDER BY hire_date) as prev_salary,
    FIRST_VALUE(name) OVER (PARTITION BY department ORDER BY salary DESC) as top_earner
FROM employees;
```

## Transactions

```sql
-- Transaction control
BEGIN [TRANSACTION] [ISOLATION LEVEL isolation_level];
COMMIT [TRANSACTION];
ROLLBACK [TRANSACTION];

-- Savepoints
SAVEPOINT savepoint_name;
ROLLBACK TO SAVEPOINT savepoint_name;
RELEASE SAVEPOINT savepoint_name;

-- Isolation levels
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;    -- Default
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
```

## Procedural SQL

### Stored Procedures

```sql
CREATE [OR REPLACE] PROCEDURE procedure_name (
    IN param1 INTEGER,
    INOUT param2 VARCHAR(100),
    OUT param3 DECIMAL
)
LANGUAGE SQL
AS $$
BEGIN
    -- Procedure body
    SET param3 = param1 * 2;
    SET param2 = UPPER(param2);
END;
$$;

-- Call procedure
CALL procedure_name(10, 'test', @result);
```

### Functions

```sql
CREATE [OR REPLACE] FUNCTION function_name (param1 INTEGER, param2 TEXT)
RETURNS INTEGER
LANGUAGE SQL
DETERMINISTIC
AS $$
BEGIN
    RETURN param1 + LENGTH(param2);
END;
$$;

-- Use function
SELECT function_name(10, 'test');
```

### Triggers

```sql
CREATE TRIGGER trigger_name
BEFORE INSERT OR UPDATE ON table_name
FOR EACH ROW
WHEN (NEW.salary > OLD.salary * 1.5)
EXECUTE FUNCTION audit_salary_change();
```

## Special Features

### EXECUTE BLOCK (Firebird style)

```sql
EXECUTE BLOCK (param1 INTEGER = ?)
RETURNS (result INTEGER)
AS
BEGIN
    result = param1 * 2;
    SUSPEND;
END;
```

### COPY (PostgreSQL style)

```sql
-- Copy from file
COPY users FROM '/path/to/file.csv' WITH (FORMAT CSV, HEADER);

-- Copy to file
COPY users TO '/path/to/file.csv' WITH (FORMAT CSV, HEADER);

-- Copy from STDIN
COPY users FROM STDIN;
```

### EXPLAIN

```sql
-- Explain query plan
EXPLAIN SELECT * FROM users WHERE email = 'test@example.com';

-- Explain with execution statistics
EXPLAIN ANALYZE SELECT * FROM users WHERE email = 'test@example.com';

-- Explain with all details
EXPLAIN (ANALYZE, BUFFERS, VERBOSE) SELECT * FROM users;
```

## System Information

### Information Schema

```sql
-- ANSI standard information schema
SELECT * FROM information_schema.tables;
SELECT * FROM information_schema.columns;
SELECT * FROM information_schema.constraints;
```

### System Functions

```sql
-- Session information
CURRENT_USER, SESSION_USER, CURRENT_DATABASE()

-- Temporal
CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP

-- UUID generation
gen_random_uuid()

-- Sequences
nextval('sequence_name'), currval('sequence_name')

-- System
version(), pg_backend_pid()
```

## Compatibility Modes

```sql
-- Set compatibility mode for session
SET SESSION sql_mode = 'POSTGRESQL';  -- PostgreSQL compatibility
SET SESSION sql_mode = 'MYSQL';       -- MySQL compatibility
SET SESSION sql_mode = 'MSSQL';       -- MSSQL compatibility
SET SESSION sql_mode = 'FIREBIRD';    -- Firebird compatibility
SET SESSION sql_mode = 'STANDARD';    -- Standard ScratchBird (default)
```

## Extensions to Standard SQL

### Array Operations

```sql
-- Array literals
SELECT ARRAY[1, 2, 3];
SELECT '{1,2,3}'::INTEGER[];

-- Array operations
SELECT * FROM users WHERE tags @> ARRAY['premium'];  -- Contains
SELECT * FROM users WHERE tags && ARRAY['new', 'premium'];  -- Overlaps
SELECT array_length(tags, 1) FROM users;  -- Length
SELECT unnest(tags) FROM users;  -- Expand array
```

### JSON Operations

```sql
-- JSON operators
SELECT data->>'name' FROM users;  -- Extract text
SELECT data->'address'->'city' FROM users;  -- Navigate path
SELECT data @> '{"active": true}' FROM users;  -- Contains

-- JSON functions
SELECT json_build_object('name', name, 'age', age) FROM users;
SELECT json_agg(users) FROM users;
SELECT jsonb_set(data, '{address,city}', '"New York"') FROM users;
```

### Full Text Search

```sql
-- Create full text index
CREATE INDEX idx_fts ON documents USING GIN (to_tsvector('english', content));

-- Full text search
SELECT * FROM documents 
WHERE to_tsvector('english', content) @@ to_tsquery('english', 'database & index');
```

## Error Handling

```sql
BEGIN
    -- Try block
    INSERT INTO users (email) VALUES ('test@example.com');
EXCEPTION
    WHEN unique_violation THEN
        -- Handle unique constraint violation
        RAISE NOTICE 'Email already exists';
    WHEN OTHERS THEN
        -- Handle any other error
        RAISE;
END;
```

## Performance Hints

```sql
-- Optimizer hints (optional)
SELECT /*+ INDEX(users idx_email) */ * FROM users WHERE email = 'test@example.com';
SELECT /*+ NO_INDEX(users) */ * FROM users;
SELECT /*+ PARALLEL(4) */ * FROM large_table;
```

## Security

### Row Level Security

```sql
-- Enable RLS
ALTER TABLE users ENABLE ROW LEVEL SECURITY;

-- Create policy
CREATE POLICY user_policy ON users
    FOR ALL
    TO PUBLIC
    USING (user_id = current_user_id());
```

### Column Encryption

```sql
-- Transparent column encryption
CREATE TABLE sensitive_data (
    id INTEGER,
    ssn VARCHAR(20) ENCRYPTED WITH (algorithm = 'AES256', key = 'key_name')
);
```

## Conclusion

ScratchBird's SQL dialect provides:
- Full SQL:2016 compliance
- Best features from all major databases
- Seamless migration paths
- Performance optimizations
- Modern extensions (JSON, Arrays, Full Text)
- Comprehensive procedural capabilities

The dialect is designed to be familiar to users of any major database while providing a consistent, powerful feature set.