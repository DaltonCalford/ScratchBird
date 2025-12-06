# ScratchBird SQL Commands Reference

**Last Updated:** December 4, 2025

This document provides a complete reference of all SQL commands supported by ScratchBird, organized by category.

---

## DDL (Data Definition Language)

### Table Operations

#### CREATE TABLE
```sql
CREATE TABLE [IF NOT EXISTS] [schema.]table_name (
    column_name data_type [constraints]...
    [, ...]
    [, table_constraint]...
)
[TABLESPACE tablespace_name]
[DEFAULT CHARSET charset_name]
[COLLATE collation_name]
```

**Column Constraints:**
- `NOT NULL` / `NULL`
- `DEFAULT expression`
- `PRIMARY KEY`
- `UNIQUE`
- `REFERENCES table(column) [ON DELETE action] [ON UPDATE action]`
- `CHECK (expression)`
- `GENERATED ALWAYS AS (expression) STORED`
- `GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY [(sequence_options)]`

**Table Constraints:**
- `PRIMARY KEY (column_list)`
- `UNIQUE (column_list)`
- `FOREIGN KEY (columns) REFERENCES table(columns) [ON DELETE/UPDATE actions]`
- `CHECK (expression)`

#### ALTER TABLE
```sql
ALTER TABLE [schema.]table_name action [, action]...

Actions:
  ADD [COLUMN] column_name data_type [constraints]
  DROP [COLUMN] column_name [CASCADE | RESTRICT]
  ALTER [COLUMN] column_name SET DATA TYPE new_type
  ALTER [COLUMN] column_name SET DEFAULT expression
  ALTER [COLUMN] column_name DROP DEFAULT
  ALTER [COLUMN] column_name SET NOT NULL
  ALTER [COLUMN] column_name DROP NOT NULL
  RENAME [COLUMN] old_name TO new_name
  RENAME TO new_table_name
  ADD CONSTRAINT constraint_name constraint_definition
  DROP CONSTRAINT constraint_name [CASCADE | RESTRICT]
  SET TABLESPACE tablespace_name [ONLINE]
  ENABLE ROW LEVEL SECURITY
  DISABLE ROW LEVEL SECURITY
  FORCE ROW LEVEL SECURITY
  NO FORCE ROW LEVEL SECURITY
```

#### DROP TABLE
```sql
DROP TABLE [IF EXISTS] [schema.]table_name [CASCADE | RESTRICT]
```

#### TRUNCATE TABLE
```sql
TRUNCATE TABLE [schema.]table_name [ASYNC | SYNC]
```

---

### Index Operations

#### CREATE INDEX
```sql
CREATE [UNIQUE] INDEX [IF NOT EXISTS] index_name
ON [schema.]table_name
[USING index_type]
(column_or_expression [ASC | DESC] [NULLS {FIRST | LAST}], ...)
[WHERE predicate]
[TABLESPACE tablespace_name]

Index Types:
  BTREE (default)
  HASH
  HNSW        -- Vector similarity search
  FULLTEXT    -- Full-text search
  GIN         -- Generalized inverted index
  GIST        -- Generalized search tree
  BRIN        -- Block range index
  RTREE       -- Spatial index
  SPGIST      -- Space-partitioned GiST
  BITMAP      -- Bitmap index
  COLUMNSTORE -- Columnar storage
  LSM         -- Log-structured merge tree
```

#### DROP INDEX
```sql
DROP INDEX [IF EXISTS] index_name [CASCADE | RESTRICT]
```

---

### Sequence Operations

#### CREATE SEQUENCE
```sql
CREATE SEQUENCE [IF NOT EXISTS] [schema.]sequence_name
[INCREMENT [BY] value]
[MINVALUE value | NO MINVALUE]
[MAXVALUE value | NO MAXVALUE]
[START [WITH] value]
[CACHE value]
[CYCLE | NO CYCLE]
```

#### ALTER SEQUENCE
```sql
ALTER SEQUENCE [schema.]sequence_name
[RESTART [WITH value]]
[INCREMENT [BY] value]
[MINVALUE value | NO MINVALUE]
[MAXVALUE value | NO MAXVALUE]
[CACHE value]
[CYCLE | NO CYCLE]
```

#### DROP SEQUENCE
```sql
DROP SEQUENCE [IF EXISTS] [schema.]sequence_name [CASCADE | RESTRICT]
```

**Sequence Functions:**
```sql
NEXTVAL('sequence_name')
CURRVAL('sequence_name')
SETVAL('sequence_name', value [, is_called])
```

---

### View Operations

#### CREATE VIEW
```sql
CREATE [OR REPLACE] [MATERIALIZED] VIEW [schema.]view_name
[(column_name, ...)]
AS select_statement
```

#### DROP VIEW
```sql
DROP VIEW [IF EXISTS] [schema.]view_name [CASCADE | RESTRICT]
```

#### REFRESH MATERIALIZED VIEW
```sql
REFRESH MATERIALIZED VIEW [CONCURRENTLY] [schema.]view_name
```

---

### Tablespace Operations

#### CREATE TABLESPACE
```sql
CREATE TABLESPACE tablespace_name
LOCATION 'directory_path'
[SIZE initial_size]
[AUTOEXTEND {ON | OFF}]
[NEXT extent_size]
[MAXSIZE max_size | UNLIMITED]
```

#### ALTER TABLESPACE
```sql
ALTER TABLESPACE tablespace_name
{
  AUTOEXTEND {ON | OFF}
  | NEXT extent_size
  | MAXSIZE max_size | UNLIMITED
  | RENAME TO new_name
}
```

#### DROP TABLESPACE
```sql
DROP TABLESPACE tablespace_name [FORCE]
```

#### ATTACH/DETACH TABLESPACE
```sql
ATTACH TABLESPACE 'file_path' AS tablespace_name
DETACH TABLESPACE tablespace_name [FORCE]
```

---

### Schema Operations

#### CREATE SCHEMA
```sql
CREATE SCHEMA [IF NOT EXISTS] schema_name
[AUTHORIZATION owner_name]
```

#### DROP SCHEMA
```sql
DROP SCHEMA [IF EXISTS] schema_name [CASCADE | RESTRICT]
```

---

### Trigger Operations

#### CREATE TRIGGER
```sql
CREATE TRIGGER trigger_name
{BEFORE | AFTER | INSTEAD OF}
{INSERT | UPDATE | DELETE} [OR ...]
ON [schema.]table_name
[REFERENCING {OLD TABLE AS old_alias} {NEW TABLE AS new_alias}]
[FOR EACH {ROW | STATEMENT}]
[WHEN (condition)]
EXECUTE PROCEDURE procedure_name(arguments)
```

#### DROP TRIGGER
```sql
DROP TRIGGER [IF EXISTS] trigger_name ON table_name
```

---

## DML (Data Manipulation Language)

### SELECT
```sql
[WITH [RECURSIVE] cte_name [(columns)] AS (select_statement) [, ...]]
SELECT [DISTINCT | ALL] select_list
FROM table_reference [, ...]
[JOIN ...]
[WHERE condition]
[GROUP BY {expression | ROLLUP(...) | CUBE(...) | GROUPING SETS(...)}]
[HAVING condition]
[ORDER BY expression [ASC | DESC] [NULLS {FIRST | LAST}] [, ...]]
[LIMIT count [OFFSET offset]]
[FOR UPDATE | FOR SHARE]

Join Types:
  [INNER] JOIN
  LEFT [OUTER] JOIN
  RIGHT [OUTER] JOIN
  FULL [OUTER] JOIN
  CROSS JOIN
  NATURAL JOIN
```

### Set Operations
```sql
select_statement
{UNION | INTERSECT | EXCEPT} [ALL]
select_statement
```

### INSERT
```sql
INSERT INTO [schema.]table_name [(column_list)]
{VALUES (value_list) [, ...] | select_statement}
[RETURNING column_list]
```

### UPDATE
```sql
UPDATE [schema.]table_name
SET column = expression [, ...]
[FROM table_reference]
[WHERE condition]
[RETURNING column_list]
```

### DELETE
```sql
DELETE FROM [schema.]table_name
[USING table_reference]
[WHERE condition]
[RETURNING column_list]
```

### MERGE
```sql
MERGE INTO target_table
USING source_table ON condition
WHEN MATCHED [AND condition] THEN
  UPDATE SET column = expression [, ...]
WHEN NOT MATCHED [AND condition] THEN
  INSERT [(columns)] VALUES (values)
```

---

## TCL (Transaction Control Language)

### START TRANSACTION
```sql
START TRANSACTION
[READ WRITE | READ ONLY]
[ISOLATION LEVEL {READ COMMITTED | SNAPSHOT | SNAPSHOT TABLE STABILITY}]
[LOCK TIMEOUT timeout_seconds]
[WAIT | NO WAIT]
[RESERVING table_name [FOR {SHARED | PROTECTED} {READ | WRITE}] [, ...]]
```

### SET TRANSACTION
```sql
SET TRANSACTION
[READ WRITE | READ ONLY]
[ISOLATION LEVEL {READ COMMITTED | SNAPSHOT | SNAPSHOT TABLE STABILITY}]
```

### COMMIT
```sql
COMMIT [WORK]
```

### ROLLBACK
```sql
ROLLBACK [WORK]
```

### SET CONSTRAINTS
```sql
SET CONSTRAINTS {ALL | constraint_name [, ...]} {DEFERRED | IMMEDIATE}
```

---

## DCL (Data Control Language)

### User Management

#### CREATE USER
```sql
CREATE USER username
[WITH PASSWORD 'password']
[SUPERUSER | NOSUPERUSER]
```

#### ALTER USER
```sql
ALTER USER username
{
  [WITH] PASSWORD 'new_password'
  | SUPERUSER | NOSUPERUSER
}
```

#### DROP USER
```sql
DROP USER [IF EXISTS] username [CASCADE | RESTRICT]
```

### Role Management

#### CREATE ROLE
```sql
CREATE ROLE role_name
```

#### DROP ROLE
```sql
DROP ROLE [IF EXISTS] role_name [CASCADE | RESTRICT]
```

### Group Management

#### CREATE GROUP
```sql
CREATE GROUP group_name
```

#### DROP GROUP
```sql
DROP GROUP [IF EXISTS] group_name [CASCADE | RESTRICT]
```

### Privilege Management

#### GRANT (Privileges)
```sql
GRANT privilege_list
ON object_type object_name
TO grantee [, ...]
[WITH GRANT OPTION]

Privileges:
  SELECT, INSERT, UPDATE, DELETE, TRUNCATE,
  REFERENCES, TRIGGER, CREATE, USAGE, EXECUTE,
  CONNECT, ALL [PRIVILEGES]

Object Types:
  TABLE, VIEW, SEQUENCE, FUNCTION, PROCEDURE,
  SCHEMA, DATABASE, DOMAIN
```

#### GRANT (Roles)
```sql
GRANT role_name [, ...]
TO grantee [, ...]
[WITH ADMIN OPTION]
```

#### REVOKE (Privileges)
```sql
REVOKE [GRANT OPTION FOR] privilege_list
ON object_type object_name
FROM grantee [, ...]
[CASCADE | RESTRICT]
```

#### REVOKE (Roles)
```sql
REVOKE [ADMIN OPTION FOR] role_name [, ...]
FROM grantee [, ...]
[CASCADE | RESTRICT]
```

### Session Control

#### SET ROLE
```sql
SET ROLE role_name
RESET ROLE
```

#### SET SESSION AUTHORIZATION
```sql
SET SESSION AUTHORIZATION username
RESET SESSION AUTHORIZATION
```

### Row-Level Security

#### CREATE POLICY
```sql
CREATE POLICY policy_name
ON [schema.]table_name
[FOR {ALL | SELECT | INSERT | UPDATE | DELETE}]
[TO role_name [, ...]]
USING (expression)
[WITH CHECK (expression)]
```

#### DROP POLICY
```sql
DROP POLICY [IF EXISTS] policy_name ON table_name [CASCADE | RESTRICT]
```

---

## Stored Procedures & Functions

### CREATE FUNCTION
```sql
CREATE [OR REPLACE] FUNCTION [schema.]function_name
([parameter_mode] param_name param_type [DEFAULT value] [, ...])
RETURNS return_type
[SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
  -- function body
END
```

### CREATE PROCEDURE
```sql
CREATE [OR REPLACE] PROCEDURE [schema.]procedure_name
([parameter_mode] param_name param_type [DEFAULT value] [, ...])
[SQL SECURITY {DEFINER | INVOKER}]
AS
BEGIN
  -- procedure body
END

Parameter Modes: IN, OUT, INOUT
```

### PL/SQL Statements
```sql
DECLARE
  variable_name type [:= value];
  CONSTANT const_name type := value;
BEGIN
  -- statements
  variable := expression;

  IF condition THEN
    statements
  ELSIF condition THEN
    statements
  ELSE
    statements
  END IF;

  LOOP
    statements
    EXIT [label] [WHEN condition];
  END LOOP;

  WHILE condition LOOP
    statements
  END LOOP;

  RETURN [value];

  RAISE [level] 'message' [USING arguments];
EXCEPTION
  WHEN exception_name THEN
    statements
END;
```

---

## Utility Commands

### ANALYZE
```sql
ANALYZE [schema.]table_name
[COLUMN column_name]
[SAMPLE percentage]
```

### EXPLAIN
```sql
EXPLAIN [options] statement
```

### SHOW
```sql
SHOW TABLES [FROM schema]
SHOW DATABASES
SHOW COLUMNS FROM table_name
SHOW INDEXES FROM table_name
SHOW CREATE TABLE table_name
```

### DESCRIBE
```sql
DESCRIBE table_name
DESC table_name
```

### SWEEP
```sql
SWEEP DATABASE  -- Garbage collection (MGA)
```

---

## Expression Types

### Aggregate Functions
- `COUNT(*)`, `COUNT(expression)`, `COUNT(DISTINCT expression)`
- `SUM(expression)`, `SUM(DISTINCT expression)`
- `AVG(expression)`, `AVG(DISTINCT expression)`
- `MIN(expression)`, `MAX(expression)`
- `ARRAY_AGG(expression)`

### Window Functions
```sql
function_name([args]) OVER (
  [PARTITION BY expression [, ...]]
  [ORDER BY expression [ASC|DESC] [NULLS {FIRST|LAST}] [, ...]]
  [frame_clause]
)

Functions:
  ROW_NUMBER(), RANK(), DENSE_RANK(),
  LAG(expr, offset, default), LEAD(expr, offset, default),
  FIRST_VALUE(expr), LAST_VALUE(expr), NTH_VALUE(expr, n),
  CUME_DIST(), PERCENT_RANK()

Frame Clause:
  {ROWS | RANGE | GROUPS}
  {UNBOUNDED PRECEDING | n PRECEDING | CURRENT ROW}
  [AND {UNBOUNDED FOLLOWING | n FOLLOWING | CURRENT ROW}]
```

### Conditional Expressions
```sql
CASE expression
  WHEN value THEN result
  [...]
  [ELSE default]
END

CASE
  WHEN condition THEN result
  [...]
  [ELSE default]
END

COALESCE(expr1, expr2, ...)
NULLIF(expr1, expr2)
```

### Type Casting
```sql
CAST(expression AS type)
TRY_CAST(expression AS type)  -- Returns NULL on failure
expression::type              -- PostgreSQL-style
```

### JSON Operations
```sql
json_column -> 'key'          -- Get JSON object field
json_column ->> 'key'         -- Get JSON object field as text
json_column #> '{path}'       -- Get JSON object at path
json_column #>> '{path}'      -- Get JSON object at path as text
JSON_EXTRACT(json, path)
JSON_OBJECT('key', value, ...)
JSON_ARRAY(value, ...)
JSON_SET(json, path, value)
```

### Array Operations
```sql
ARRAY[element, ...]
array1 && array2              -- Overlap
array1 @> array2              -- Contains
array1 <@ array2              -- Contained by
```

### Range Operations
```sql
range1 << range2              -- Strictly left of
range1 >> range2              -- Strictly right of
range1 -|- range2             -- Adjacent to
```

### Pattern Matching
```sql
string LIKE pattern
string ILIKE pattern          -- Case-insensitive
string ~ regex                -- Regex match
string ~* regex               -- Case-insensitive regex match
string !~ regex               -- Regex not match
string !~* regex              -- Case-insensitive regex not match
```

---

## Data Types

### Numeric Types
| Type | Description |
|------|-------------|
| `TINYINT` / `INT8` | 1-byte signed integer |
| `SMALLINT` / `INT16` | 2-byte signed integer |
| `INTEGER` / `INT` / `INT32` | 4-byte signed integer |
| `BIGINT` / `INT64` | 8-byte signed integer |
| `REAL` / `FLOAT` | 4-byte floating point |
| `DOUBLE PRECISION` | 8-byte floating point |
| `DECIMAL(p,s)` / `NUMERIC(p,s)` | Exact numeric |
| `MONEY` | Currency type |

### String Types
| Type | Description |
|------|-------------|
| `CHAR(n)` | Fixed-length string |
| `VARCHAR(n)` | Variable-length string |
| `TEXT` | Unlimited text |

### Binary Types
| Type | Description |
|------|-------------|
| `BYTEA` | Variable-length binary |
| `BLOB` | Binary large object |

### Date/Time Types
| Type | Description |
|------|-------------|
| `DATE` | Date (year, month, day) |
| `TIME` | Time of day |
| `TIMESTAMP` | Date and time |
| `TIMESTAMP WITH TIME ZONE` | Timestamp with timezone |
| `INTERVAL` | Time interval |

### Special Types
| Type | Description |
|------|-------------|
| `BOOLEAN` | True/false |
| `UUID` | 128-bit UUID |
| `JSON` | JSON document |
| `JSONB` | Binary JSON |
| `XML` | XML document |
| `VECTOR(n)` | Vector embeddings |

### Spatial Types
| Type | Description |
|------|-------------|
| `POINT` | 2D point |
| `LINESTRING` | Line |
| `POLYGON` | Polygon |
| `MULTIPOINT` | Point collection |
| `MULTILINESTRING` | Line collection |
| `MULTIPOLYGON` | Polygon collection |
| `GEOMETRYCOLLECTION` | Mixed geometry |

### Network Types
| Type | Description |
|------|-------------|
| `INET` | IPv4/IPv6 address |
| `CIDR` | Network address |
| `MACADDR` | MAC address (6 bytes) |
| `MACADDR8` | MAC address (8 bytes) |

### Text Search Types
| Type | Description |
|------|-------------|
| `TSVECTOR` | Text search vector |
| `TSQUERY` | Text search query |

### Range Types
| Type | Description |
|------|-------------|
| `INT4RANGE` | Integer range |
| `INT8RANGE` | Bigint range |
| `NUMRANGE` | Numeric range |
| `TSRANGE` | Timestamp range |
| `TSTZRANGE` | Timestamptz range |
| `DATERANGE` | Date range |

### Collection Types
| Type | Description |
|------|-------------|
| `type[]` / `ARRAY` | Array of elements |
| `COMPOSITE` | Record/struct type |
