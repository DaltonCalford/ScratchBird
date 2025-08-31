# ScratchBird SQL Dialect Complete Specification

## Overview

This document defines the complete SQL dialect supported by ScratchBird, serving as the authoritative reference for all SQL features, commands, and extensions.

## Table of Contents

1. [Core Principles](#core-principles)
2. [DDL - Data Definition Language](#ddl---data-definition-language)
3. [DML - Data Manipulation Language](#dml---data-manipulation-language)
4. [DCL - Data Control Language](#dcl---data-control-language)
5. [TCL - Transaction Control Language](#tcl---transaction-control-language)
6. [PSQL - Procedural SQL](#psql---procedural-sql)
7. [System Commands](#system-commands)
8. [Schema Navigation](#schema-navigation)
9. [Extensions](#extensions)
10. [Compatibility Features](#compatibility-features)

---

## Core Principles

### Context-Aware Parsing
- Minimal reserved words
- Keywords only reserved where ambiguous
- Automatic statement termination detection
- Intelligent identifier resolution
- **Precise error reporting with visual indicators** (see PARSER_ERROR_REPORTING_AND_COMMENTS.md)
- Shows exact location of syntax errors with context
- Multiple error detection in single pass
- Helpful hints and suggestions for fixes

### Case Sensitivity
- **Identifiers**: Case-preserving, case-insensitive (MSSQL style)
- **Quoted Identifiers**: Case-sensitive
- **Keywords**: Case-insensitive

### Comment Support
```sql
-- Single line comment
/* Multi-line 
   comment */
// C-style comment (supported)
```

**Intelligent Comment Management** (MSSQL-style):
- Comments before DDL automatically become object documentation
- Comments before columns become column comments
- JavaDoc-style comments parsed for metadata
- Comments preserved through schema changes
- See PARSER_ERROR_REPORTING_AND_COMMENTS.md for details

---

## DDL - Data Definition Language

### DATABASE Operations

```sql
-- Create database
CREATE DATABASE database_name
  [PAGE_SIZE = {8K|16K|32K|64K|128K}]
  [DEFAULT CHARACTER SET charset_name]
  [DEFAULT COLLATE collation_name]
  [ENCRYPTED [WITH PASSWORD 'password']]
  [OWNER username];

-- Alter database
ALTER DATABASE database_name
  SET DEFAULT CHARACTER SET charset_name
  | SET DEFAULT COLLATE collation_name
  | SET SWEEP INTERVAL integer
  | SET OWNER TO username;

-- Drop database
DROP DATABASE [IF EXISTS] database_name [CASCADE | RESTRICT];
```

### SCHEMA Operations

```sql
-- Create schema
CREATE SCHEMA [IF NOT EXISTS] schema_name
  [AUTHORIZATION owner_name]
  [DEFAULT CHARACTER SET charset_name]
  [PATH schema_path];

-- Alter schema
ALTER SCHEMA schema_name
  RENAME TO new_name
  | OWNER TO new_owner
  | SET PATH schema_path;

-- Drop schema
DROP SCHEMA [IF EXISTS] schema_name [CASCADE | RESTRICT];

-- Set current schema
SET SCHEMA schema_name;
SET SEARCH_PATH TO schema1, schema2, ...;
```

### TABLE Operations

```sql
-- Create table
CREATE [TEMPORARY | TEMP | GLOBAL TEMPORARY] TABLE [IF NOT EXISTS] 
  [schema_name.]table_name (
    column_name data_type [column_constraints] [, ...]
    [, table_constraints]
  )
  [TABLESPACE tablespace_name]
  [ON COMMIT {PRESERVE ROWS | DELETE ROWS | DROP}]
  [AS select_statement]
  [WITH [NO] DATA];

-- Column constraints
[CONSTRAINT constraint_name]
  NOT NULL
  | NULL
  | UNIQUE
  | PRIMARY KEY
  | REFERENCES table_name [(column_name)] [match_type] [referential_actions]
  | CHECK (expression)
  | DEFAULT default_value
  | GENERATED ALWAYS AS (expression) [STORED | VIRTUAL]
  | GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY [(sequence_options | UUID options)]
  | COLLATE collation_name
  | COMPRESS [USING method]
  | ENCRYPT [USING method]

-- Table constraints
[CONSTRAINT constraint_name]
  PRIMARY KEY (column_list) [index_parameters]
  | UNIQUE (column_list) [index_parameters]
  | FOREIGN KEY (column_list) REFERENCES table_name [(column_list)]
    [MATCH {FULL | PARTIAL | SIMPLE}]
    [ON DELETE {CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION}]
    [ON UPDATE {CASCADE | SET NULL | SET DEFAULT | RESTRICT | NO ACTION}]
  | CHECK (expression)
  | EXCLUDE USING index_method (exclude_element WITH operator [, ...])

-- Alter table
ALTER TABLE [IF EXISTS] table_name
  ADD [COLUMN] column_definition
  | DROP [COLUMN] [IF EXISTS] column_name [CASCADE | RESTRICT]
  | ALTER [COLUMN] column_name [SET DATA] TYPE data_type [USING expression]
  | ALTER [COLUMN] column_name SET DEFAULT expression
  | ALTER [COLUMN] column_name DROP DEFAULT
  | ALTER [COLUMN] column_name {SET | DROP} NOT NULL
  | ALTER [COLUMN] column_name SET STATISTICS integer
  | ALTER [COLUMN] column_name SET STORAGE {PLAIN | EXTERNAL | EXTENDED | MAIN}
  | ADD table_constraint
  | DROP CONSTRAINT [IF EXISTS] constraint_name [CASCADE | RESTRICT]
  | RENAME [COLUMN] column_name TO new_column_name
  | RENAME TO new_table_name
  | SET TABLESPACE tablespace_name
  | SET SCHEMA new_schema
  | ENABLE TRIGGER {trigger_name | ALL | USER}
  | DISABLE TRIGGER {trigger_name | ALL | USER}
  | CLUSTER ON index_name
  | SET WITHOUT CLUSTER
  | SET {LOGGED | UNLOGGED}
  | OWNER TO new_owner;

-- Drop table
DROP TABLE [IF EXISTS] table_name [, ...] [CASCADE | RESTRICT];

-- Truncate table
TRUNCATE [TABLE] table_name [, ...]
  [RESTART IDENTITY | CONTINUE IDENTITY]
  [CASCADE | RESTRICT];
```

### Data Types

```sql
-- Numeric types
SMALLINT | INT2                    -- 16-bit signed
INTEGER | INT | INT4                -- 32-bit signed
BIGINT | INT8                       -- 64-bit signed
INT128                              -- 128-bit signed
UINT8 | TINYINT UNSIGNED           -- 8-bit unsigned
UINT16 | SMALLINT UNSIGNED         -- 16-bit unsigned
UINT32 | INT UNSIGNED              -- 32-bit unsigned
UINT64 | BIGINT UNSIGNED           -- 64-bit unsigned
DECIMAL(p,s) | NUMERIC(p,s)        -- Exact decimal
REAL | FLOAT4                       -- 32-bit float
DOUBLE PRECISION | FLOAT8          -- 64-bit float
MONEY                               -- Currency type

-- Character types
CHAR(n) | CHARACTER(n)              -- Fixed-length
VARCHAR(n) | CHARACTER VARYING(n)   -- Variable-length
TEXT                                -- Unlimited text
NCHAR(n)                           -- Unicode fixed
NVARCHAR(n)                        -- Unicode variable

-- Binary types
BINARY(n)                          -- Fixed-length binary
VARBINARY(n)                       -- Variable binary
BLOB | BYTEA                       -- Binary large object

-- Date/Time types
DATE                               -- Date only
TIME [WITHOUT TIME ZONE]          -- Time only
TIME WITH TIME ZONE | TIMETZ      -- Time with timezone
TIMESTAMP [WITHOUT TIME ZONE]      -- Date and time
TIMESTAMP WITH TIME ZONE | TIMESTAMPTZ
INTERVAL                           -- Time interval

-- Boolean type
BOOLEAN | BOOL

-- UUID type
UUID

-- JSON types
JSON                               -- Text JSON
JSONB                              -- Binary JSON

-- XML type
XML

-- Array types
data_type[]                        -- Array of any type
data_type[n]                       -- Fixed-size array

-- Set types (ScratchBird Enhanced)
SET OF data_type                   -- Unordered collection
SET OF domain_name                 -- Domain-based set

-- Special types
SERIAL | SERIAL4                   -- Auto-incrementing int
BIGSERIAL | SERIAL8                -- Auto-incrementing bigint
```

### UUID IDENTITY Columns (ScratchBird Enhanced)

```sql
-- UUID IDENTITY with UUID v7 (time-ordered, recommended)
CREATE TABLE events (
    id UUID GENERATED ALWAYS AS IDENTITY (UUID VERSION 7),
    event_data JSONB
);

-- UUID v4 (random)
CREATE TABLE tokens (
    token_id UUID GENERATED BY DEFAULT AS IDENTITY (UUID VERSION 4),
    token_value TEXT
);

-- Default is UUID v7 for optimal index performance
CREATE TABLE orders (
    id UUID GENERATED ALWAYS AS IDENTITY,  -- Uses UUID v7 by default
    customer_id UUID
);

-- Namespace-based UUID v5
CREATE TABLE namespaced_items (
    id UUID GENERATED ALWAYS AS IDENTITY (
        UUID VERSION 5
        NAMESPACE 'dns'  -- or 'url', 'oid', 'x500', or custom UUID
        NAME COLUMN 'item_name'  -- Use column value as input
    ),
    item_name VARCHAR(100) UNIQUE
);

-- Distributed system with node ID
CREATE TABLE distributed_entities (
    id UUID GENERATED ALWAYS AS IDENTITY (
        UUID VERSION 7
        NODE ID 42  -- Ensures uniqueness across nodes (0-1023)
        SEQUENCE COMPONENT  -- Monotonic within millisecond
    ),
    data JSONB
);

-- Custom UUID generator
CREATE TABLE custom_table (
    id UUID GENERATED ALWAYS AS IDENTITY (UUID CUSTOM 'my_uuid_gen'),
    data TEXT
);

-- Supported UUID versions:
-- VERSION 1: MAC address based (deprecated)
-- VERSION 3: MD5 namespace based
-- VERSION 4: Random
-- VERSION 5: SHA-1 namespace based  
-- VERSION 6: Reordered v1 for databases
-- VERSION 7: Time-ordered (RECOMMENDED for primary keys)
-- VERSION 8: Custom/vendor specific

-- Generation strategies:
-- GENERATED ALWAYS: Cannot override with explicit value
-- GENERATED BY DEFAULT: Can override with explicit value
-- GENERATED BY DEFAULT ON NULL: Generate only if NULL

-- See UUID_IDENTITY_COLUMNS.md for complete specification
```

### DOMAIN Operations

```sql
-- Create domain (Firebird-style custom types)
CREATE DOMAIN domain_name [AS] data_type
  [DEFAULT default_value]
  [NOT NULL]
  [CHECK (expression)]
  [COLLATE collation_name];

-- ADVANCED DOMAIN TYPES (ScratchBird Enhanced)
-- See ADVANCED_DOMAIN_TYPES.md for complete specification

-- Record domains (complex types)
CREATE DOMAIN person_name AS RECORD (
    first_name VARCHAR(50) NOT NULL,
    middle_name VARCHAR(50),
    last_name VARCHAR(50) NOT NULL
);

-- Enum domains with positional arithmetic
CREATE DOMAIN hex_digit AS ENUM (
    '0','1','2','3','4','5','6','7',
    '8','9','A','B','C','D','E','F'
) WITH OPTIONS (WRAP = TRUE);

-- Using domains in tables
CREATE TABLE customers (
    id UUID GENERATED ALWAYS AS IDENTITY,
    name person_name NOT NULL,
    status order_state DEFAULT 'Draft'
);

-- Extract from record domains
SELECT 
    EXTRACT(first_name FROM name) AS first,
    EXTRACT(last_name FROM name) AS last
FROM customers;

-- Enum arithmetic
DECLARE @hex hex_digit = 'F';
SELECT @hex + 3;  -- Returns '2' (with WRAP=TRUE)
SELECT POSITION(@hex);  -- Returns 15
SELECT CAST(15 AS hex_digit);  -- Returns 'F'

-- Alter domain
ALTER DOMAIN domain_name
  SET DEFAULT default_value
  | DROP DEFAULT
  | ADD CONSTRAINT constraint_name CHECK (expression)
  | DROP CONSTRAINT constraint_name
  | RENAME TO new_name
  | OWNER TO new_owner;

-- Drop domain
DROP DOMAIN [IF EXISTS] domain_name [CASCADE | RESTRICT];
```

### INDEX Operations

```sql
-- Create index
CREATE [UNIQUE] INDEX [CONCURRENTLY] [IF NOT EXISTS] index_name
  ON table_name 
  [USING method] -- BTREE | HASH | GIN | GIST | RTREE | BITMAP | LSM | COLUMNSTORE
  (
    {column_name | (expression)} [COLLATE collation] [opclass] 
    [ASC | DESC] [NULLS {FIRST | LAST}] [, ...]
  )
  [INCLUDE (column_name [, ...])]
  [WHERE predicate]
  [TABLESPACE tablespace_name];

-- Alter index
ALTER INDEX [IF EXISTS] index_name
  RENAME TO new_name
  | SET TABLESPACE tablespace_name
  | SET (storage_parameter = value [, ...])
  | RESET (storage_parameter [, ...]);

-- Drop index
DROP INDEX [CONCURRENTLY] [IF EXISTS] index_name [, ...] [CASCADE | RESTRICT];

-- Reindex
REINDEX [VERBOSE] {INDEX | TABLE | SCHEMA | DATABASE} name;
```

### VIEW Operations

```sql
-- Create view
CREATE [OR REPLACE] [TEMPORARY | TEMP] VIEW view_name 
  [(column_name [, ...])]
  AS select_statement
  [WITH [CASCADED | LOCAL] CHECK OPTION];

-- Create materialized view
CREATE MATERIALIZED VIEW [IF NOT EXISTS] view_name
  [(column_name [, ...])]
  [USING method]
  [WITH (storage_parameter [= value] [, ...])]
  [TABLESPACE tablespace_name]
  AS select_statement
  [WITH [NO] DATA];

-- Refresh materialized view
REFRESH MATERIALIZED VIEW [CONCURRENTLY] view_name [WITH [NO] DATA];

-- Alter view
ALTER VIEW [IF EXISTS] view_name
  ALTER [COLUMN] column_name SET DEFAULT expression
  | ALTER [COLUMN] column_name DROP DEFAULT
  | RENAME [COLUMN] column_name TO new_column_name
  | RENAME TO new_name
  | SET SCHEMA new_schema
  | OWNER TO new_owner;

-- Drop view
DROP VIEW [IF EXISTS] view_name [, ...] [CASCADE | RESTRICT];
DROP MATERIALIZED VIEW [IF EXISTS] view_name [, ...] [CASCADE | RESTRICT];
```

### SEQUENCE Operations

```sql
-- Create sequence
CREATE SEQUENCE [IF NOT EXISTS] sequence_name
  [AS data_type]
  [INCREMENT [BY] increment]
  [MINVALUE minvalue | NO MINVALUE]
  [MAXVALUE maxvalue | NO MAXVALUE]
  [START [WITH] start]
  [CACHE cache]
  [[NO] CYCLE]
  [OWNED BY {table_name.column_name | NONE}];

-- Alter sequence
ALTER SEQUENCE [IF EXISTS] sequence_name
  [AS data_type]
  [INCREMENT [BY] increment]
  [MINVALUE minvalue | NO MINVALUE]
  [MAXVALUE maxvalue | NO MAXVALUE]
  [START [WITH] start]
  [RESTART [[WITH] restart]]
  [CACHE cache]
  [[NO] CYCLE]
  [OWNED BY {table_name.column_name | NONE}];

-- Sequence functions
NEXT VALUE FOR sequence_name
CURRVAL('sequence_name')
LASTVAL()
SETVAL('sequence_name', value [, is_called])

-- Drop sequence
DROP SEQUENCE [IF EXISTS] sequence_name [, ...] [CASCADE | RESTRICT];
```

---

## DML - Data Manipulation Language

### SELECT Statement

```sql
-- Complete SELECT syntax
[WITH [RECURSIVE] cte_name [(column_list)] AS (select_statement) [, ...]]
SELECT [ALL | DISTINCT [ON (expression [, ...])]]
  [* | expression [[AS] alias] [, ...]]
  [FROM from_item [, ...]]
  [WHERE condition]
  [GROUP BY grouping_element [, ...]]
  [HAVING condition]
  [WINDOW window_name AS (window_definition) [, ...]]
  [{UNION | INTERSECT | EXCEPT} [ALL | DISTINCT] select_statement]
  [ORDER BY expression [ASC | DESC] [NULLS {FIRST | LAST}] [, ...]]
  [LIMIT {count | ALL}]
  [OFFSET start [ROW | ROWS]]
  [FETCH {FIRST | NEXT} count {ROW | ROWS} {ONLY | WITH TIES}]
  [FOR {UPDATE | NO KEY UPDATE | SHARE | KEY SHARE} 
    [OF table_name [, ...]] [NOWAIT | SKIP LOCKED]];

-- FROM clause items
table_name [[AS] alias [(column_alias [, ...])]]
| (select_statement) [AS] alias [(column_alias [, ...])]
| function_name([arguments]) [[AS] alias [(column_alias [, ...])]]
| LATERAL from_item
| from_item [NATURAL] join_type from_item [ON join_condition | USING (column_list)]

-- Join types
[INNER] JOIN
| LEFT [OUTER] JOIN
| RIGHT [OUTER] JOIN
| FULL [OUTER] JOIN
| CROSS JOIN

-- Grouping elements
expression
| ROLLUP (expression [, ...])
| CUBE (expression [, ...])
| GROUPING SETS ((expression [, ...]) [, ...])
| ()

-- Window functions
function_name([arguments]) OVER window_name
function_name([arguments]) OVER (window_definition)

-- Window definition
[PARTITION BY expression [, ...]]
[ORDER BY expression [ASC | DESC] [NULLS {FIRST | LAST}] [, ...]]
[{RANGE | ROWS | GROUPS} {frame_start | BETWEEN frame_start AND frame_end}]

-- Frame boundaries
UNBOUNDED PRECEDING
| expression PRECEDING
| CURRENT ROW
| expression FOLLOWING
| UNBOUNDED FOLLOWING
```

### INSERT Statement

```sql
-- Insert syntax
[WITH [RECURSIVE] cte_name [(column_list)] AS (select_statement) [, ...]]
INSERT INTO table_name [(column_name [, ...])]
  {VALUES (expression [, ...]) [, ...]
  | select_statement
  | DEFAULT VALUES}
  [ON CONFLICT [conflict_target] conflict_action]
  [RETURNING * | expression [[AS] alias] [, ...]];

-- Conflict target
(column_name [, ...]) [WHERE predicate]
| ON CONSTRAINT constraint_name

-- Conflict action
DO NOTHING
| DO UPDATE SET {column_name = expression | (column_list) = (expression_list)} [, ...]
  [WHERE condition]

-- Bulk insert optimization
INSERT INTO table_name [(column_list)]
VALUES 
  (row1_values),
  (row2_values),
  ...
  (rowN_values);
```

### UPDATE Statement

```sql
-- Update syntax
[WITH [RECURSIVE] cte_name [(column_list)] AS (select_statement) [, ...]]
UPDATE [ONLY] table_name [[AS] alias]
  SET {column_name = expression 
      | (column_name [, ...]) = {(expression [, ...]) | (select_statement)}
      } [, ...]
  [FROM from_list]
  [WHERE condition | WHERE CURRENT OF cursor_name]
  [RETURNING * | expression [[AS] alias] [, ...]];
```

### DELETE Statement

```sql
-- Delete syntax
[WITH [RECURSIVE] cte_name [(column_list)] AS (select_statement) [, ...]]
DELETE FROM [ONLY] table_name [[AS] alias]
  [USING from_list]
  [WHERE condition | WHERE CURRENT OF cursor_name]
  [RETURNING * | expression [[AS] alias] [, ...]];
```

### MERGE Statement

```sql
-- Merge syntax (UPSERT)
[WITH [RECURSIVE] cte_name [(column_list)] AS (select_statement) [, ...]]
MERGE INTO target_table [[AS] target_alias]
USING source_table [[AS] source_alias]
ON merge_condition
[WHEN MATCHED [AND condition] THEN 
  {UPDATE SET {column = expression} [, ...] | DELETE}]
[WHEN NOT MATCHED [AND condition] THEN
  INSERT [(column_list)] VALUES (value_list)]
[WHEN NOT MATCHED BY SOURCE [AND condition] THEN
  {UPDATE SET {column = expression} [, ...] | DELETE}];
```

### COPY Statement

```sql
-- Copy data in/out
COPY table_name [(column_list)]
  FROM {filename | STDIN}
  [WITH] ([FORMAT {CSV | BINARY | TEXT}]
         [, DELIMITER 'delimiter_character']
         [, NULL 'null_string']
         [, HEADER [boolean]]
         [, QUOTE 'quote_character']
         [, ESCAPE 'escape_character']
         [, ENCODING 'encoding_name']);

COPY {table_name [(column_list)] | (select_statement)}
  TO {filename | STDOUT}
  [WITH] (options);
```

---

## DCL - Data Control Language

### User Management

```sql
-- Create user
CREATE USER user_name
  [WITH] [ENCRYPTED] PASSWORD 'password'
  | VALID UNTIL 'timestamp'
  | IN ROLE role_name [, ...]
  | ADMIN role_name [, ...]
  | CREATEDB | NOCREATEDB
  | CREATEROLE | NOCREATEROLE
  | INHERIT | NOINHERIT
  | LOGIN | NOLOGIN
  | REPLICATION | NOREPLICATION
  | CONNECTION LIMIT connlimit;

-- Alter user
ALTER USER user_name
  [[WITH] option [...]];

-- Drop user
DROP USER [IF EXISTS] user_name [, ...];
```

### Role Management

```sql
-- Create role
CREATE ROLE role_name
  [WITH] [ADMIN role_name]
  | [option ...];

-- Alter role
ALTER ROLE role_name
  [[WITH] option [...]];

-- Drop role
DROP ROLE [IF EXISTS] role_name [, ...];

-- Grant/revoke role membership
GRANT role_name [, ...] TO role_spec [, ...] [WITH ADMIN OPTION];
REVOKE [ADMIN OPTION FOR] role_name [, ...] FROM role_spec [, ...];
```

### Privileges

```sql
-- Grant privileges
GRANT {privilege [, ...] | ALL [PRIVILEGES]}
  ON [object_type] object_name [, ...]
  TO {role_spec | PUBLIC} [, ...]
  [WITH GRANT OPTION];

-- Object types and privileges
DATABASE: CREATE, CONNECT, TEMPORARY, TEMP
SCHEMA: CREATE, USAGE, plus granular permissions (see below)
TABLE: SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER
COLUMN: SELECT, INSERT, UPDATE, REFERENCES
SEQUENCE: USAGE, SELECT, UPDATE
FUNCTION: EXECUTE
PROCEDURE: EXECUTE
TYPE: USAGE

-- GRANULAR SCHEMA PERMISSIONS (ScratchBird Enhanced)
-- See SCHEMA_PERMISSIONS_AND_INHERITANCE.md for full details

-- DDL Rights on schemas
GRANT CREATE_TABLE, ALTER_TABLE, DROP_TABLE,
      CREATE_INDEX, DROP_INDEX, CREATE_VIEW, DROP_VIEW
  ON SCHEMA schema_name TO user
  [WITH INHERITANCE {CASCADE | LOCAL | NONE}];

-- DML Rights on schemas  
GRANT SELECT, INSERT, UPDATE, DELETE, TRUNCATE, EXECUTE
  ON SCHEMA schema_name TO user
  [WITH INHERITANCE {CASCADE | LOCAL | NONE}];

-- Schema Management Rights
GRANT CREATE_SCHEMA, ALTER_SCHEMA, DROP_SCHEMA,
      GRANT_RIGHTS, REVOKE_RIGHTS
  ON SCHEMA schema_name TO user
  [WITH INHERITANCE {CASCADE | LOCAL | NONE}];

-- Example: Different permissions for different users
GRANT SELECT, INSERT, UPDATE, DELETE  -- John can CRUD but not DDL
  ON SCHEMA sales TO john
  WITH INHERITANCE CASCADE;

GRANT CREATE_TABLE, ALTER_TABLE, DROP_TABLE  -- Jake can DDL but not CRUD
  ON SCHEMA development TO jake  
  WITH INHERITANCE LOCAL;

-- Revoke privileges
REVOKE [GRANT OPTION FOR]
  {privilege [, ...] | ALL [PRIVILEGES]}
  ON [object_type] object_name [, ...]
  FROM {role_spec | PUBLIC} [, ...]
  [CASCADE | RESTRICT];

-- Default privileges
ALTER DEFAULT PRIVILEGES
  [FOR {ROLE | USER} target_role [, ...]]
  [IN SCHEMA schema_name [, ...]]
  grant_or_revoke_statement;
```

---

## TCL - Transaction Control Language

### Transaction Management

```sql
-- Begin transaction
BEGIN [WORK | TRANSACTION] [transaction_mode [, ...]];
START TRANSACTION [transaction_mode [, ...]];

-- Transaction modes
ISOLATION LEVEL {READ UNCOMMITTED | READ COMMITTED | REPEATABLE READ | SERIALIZABLE}
READ WRITE | READ ONLY
[NOT] DEFERRABLE

-- Commit transaction
COMMIT [WORK | TRANSACTION] [AND [NO] CHAIN];
END [WORK | TRANSACTION];

-- Rollback transaction
ROLLBACK [WORK | TRANSACTION] [AND [NO] CHAIN];
ABORT [WORK | TRANSACTION];

-- Savepoints
SAVEPOINT savepoint_name;
RELEASE [SAVEPOINT] savepoint_name;
ROLLBACK [WORK | TRANSACTION] TO [SAVEPOINT] savepoint_name;

-- Two-phase commit
PREPARE TRANSACTION transaction_id;
COMMIT PREPARED transaction_id;
ROLLBACK PREPARED transaction_id;
```

### Lock Management

```sql
-- Lock table
LOCK [TABLE] [ONLY] table_name [, ...]
  [IN lock_mode MODE] [NOWAIT];

-- Lock modes
ACCESS SHARE
ROW SHARE
ROW EXCLUSIVE
SHARE UPDATE EXCLUSIVE
SHARE
SHARE ROW EXCLUSIVE
EXCLUSIVE
ACCESS EXCLUSIVE

-- Advisory locks
SELECT pg_advisory_lock(key);
SELECT pg_advisory_lock_shared(key);
SELECT pg_try_advisory_lock(key);
SELECT pg_advisory_unlock(key);
```

---

## PSQL - Procedural SQL

### Stored Procedures

```sql
-- Create procedure
CREATE [OR REPLACE] PROCEDURE procedure_name 
  ([parameter_mode] parameter_name data_type [DEFAULT default_value] [, ...])
  [LANGUAGE language_name]
  [SECURITY {DEFINER | INVOKER}]
  [SET configuration_parameter {TO value | = value | FROM CURRENT}]
AS $$
  -- Procedure body
  DECLARE
    -- Variable declarations
  BEGIN
    -- Statements
    [EXCEPTION
      WHEN condition THEN
        -- Exception handlers]
  END;
$$ LANGUAGE plpgsql;

-- Call procedure
CALL procedure_name([arguments]);
EXECUTE PROCEDURE procedure_name([arguments]);

-- Drop procedure
DROP PROCEDURE [IF EXISTS] procedure_name ([parameter_types]) [CASCADE | RESTRICT];
```

### Functions

```sql
-- Create function
CREATE [OR REPLACE] FUNCTION function_name 
  ([parameter_mode] parameter_name data_type [DEFAULT default_value] [, ...])
  RETURNS return_type
  | RETURNS TABLE (column_name data_type [, ...])
  | RETURNS SETOF return_type
  [LANGUAGE language_name]
  [IMMUTABLE | STABLE | VOLATILE]
  [STRICT | CALLED ON NULL INPUT]
  [SECURITY {DEFINER | INVOKER}]
  [PARALLEL {UNSAFE | RESTRICTED | SAFE}]
  [COST execution_cost]
  [ROWS result_rows]
  [SET configuration_parameter {TO value | = value | FROM CURRENT}]
AS $$
  -- Function body
$$ LANGUAGE plpgsql;

-- Drop function
DROP FUNCTION [IF EXISTS] function_name ([parameter_types]) [CASCADE | RESTRICT];
```

### Triggers

```sql
-- Create trigger
CREATE [OR REPLACE] TRIGGER trigger_name
  {BEFORE | AFTER | INSTEAD OF} 
  {INSERT | UPDATE [OF column_name [, ...]] | DELETE | TRUNCATE | SELECT}
  [OR {INSERT | UPDATE | DELETE | TRUNCATE | SELECT}] [...]
  ON table_name
  [REFERENCING {OLD | NEW} TABLE [AS] transition_table_name [, ...]]
  [FOR [EACH] {ROW | STATEMENT}]
  [WHEN (condition)]
  [POSITION integer]
  EXECUTE {FUNCTION | PROCEDURE} function_name(arguments);

-- Database-level triggers
CREATE [OR REPLACE] TRIGGER trigger_name
  {ON CONNECT | ON DISCONNECT | 
   ON TRANSACTION START | ON TRANSACTION COMMIT | 
   ON TRANSACTION ROLLBACK}
  [POSITION integer]
  EXECUTE {FUNCTION | PROCEDURE} function_name();

-- Enable/disable triggers
ALTER TABLE table_name {ENABLE | DISABLE} TRIGGER {trigger_name | ALL | USER};

-- Drop trigger
DROP TRIGGER [IF EXISTS] trigger_name ON table_name [CASCADE | RESTRICT];
```

### Control Structures

```sql
-- Variable declaration
DECLARE
  variable_name [CONSTANT] data_type [NOT NULL] [{DEFAULT | :=} expression];
  cursor_name CURSOR [(parameters)] FOR select_statement;

-- Assignment
variable_name := expression;
SELECT expression INTO variable_name;

-- Conditional statements
IF condition THEN
  statements;
[ELSIF condition THEN
  statements;]
[ELSE
  statements;]
END IF;

-- Case statement
CASE [expression]
  WHEN value_or_condition THEN
    statements;
  [WHEN value_or_condition THEN
    statements;]
  [ELSE
    statements;]
END CASE;

-- Loops
-- Simple loop
LOOP
  statements;
  EXIT [label] [WHEN condition];
  CONTINUE [label] [WHEN condition];
END LOOP [label];

-- While loop
WHILE condition LOOP
  statements;
END LOOP;

-- For loop (integer)
FOR variable IN [REVERSE] lower_bound..upper_bound [BY step] LOOP
  statements;
END LOOP;

-- For loop (cursor/query)
FOR record IN {cursor_name | select_statement} LOOP
  statements;
END LOOP;

-- Foreach loop (array)
FOREACH variable [SLICE dimension] IN ARRAY array_expression LOOP
  statements;
END LOOP;
```

### Exception Handling

```sql
-- Exception block
BEGIN
  -- Statements that might raise exceptions
EXCEPTION
  WHEN division_by_zero THEN
    -- Handle division by zero
  WHEN unique_violation THEN
    -- Handle unique constraint violation
  WHEN OTHERS THEN
    -- Handle all other exceptions
    GET STACKED DIAGNOSTICS 
      error_text = MESSAGE_TEXT,
      error_detail = PG_EXCEPTION_DETAIL;
    RAISE NOTICE 'Error: %', error_text;
END;

-- Raise exceptions
RAISE [level] 'format_string' [, expression [, ...]];
RAISE [level] condition_name;
RAISE [level] SQLSTATE 'sqlstate_code';
RAISE [level] USING option = expression [, ...];

-- Levels
DEBUG | LOG | INFO | NOTICE | WARNING | EXCEPTION

-- Assertion
ASSERT condition [, 'message'];
```

### Cursors

```sql
-- Declare cursor
DECLARE cursor_name [BINARY] [INSENSITIVE] [NO SCROLL | SCROLL] 
  CURSOR [(parameters)] 
  FOR select_statement
  [FOR {READ ONLY | UPDATE [OF column_name [, ...]]}];

-- Open cursor
OPEN cursor_name [(parameters)];

-- Fetch from cursor
FETCH [direction] FROM cursor_name [INTO target];
-- Directions: NEXT | PRIOR | FIRST | LAST | 
--            ABSOLUTE count | RELATIVE count | 
--            count | ALL | FORWARD [count | ALL] | 
--            BACKWARD [count | ALL]

-- Move cursor
MOVE [direction] IN cursor_name;

-- Update/delete current row
UPDATE table_name SET ... WHERE CURRENT OF cursor_name;
DELETE FROM table_name WHERE CURRENT OF cursor_name;

-- Close cursor
CLOSE cursor_name;
```

### Dynamic SQL

```sql
-- Execute dynamic SQL
EXECUTE immediate_sql_string [INTO target] [USING expression [, ...]];

-- Execute with result
EXECUTE sql_string INTO [STRICT] target [USING expression [, ...]];

-- Execute returning multiple rows
RETURN QUERY EXECUTE sql_string [USING expression [, ...]];

-- Format dynamic SQL
sql_string := format('SELECT * FROM %I WHERE %I = $1', table_name, column_name);
```

---

## System Commands

### SHOW Commands

```sql
-- Show configuration
SHOW parameter_name;
SHOW ALL;

-- Show database objects
SHOW DATABASES;
SHOW SCHEMAS [FROM database_name];
SHOW TABLES [FROM schema_name] [LIKE 'pattern'];
SHOW COLUMNS FROM table_name;
SHOW INDEX FROM table_name;
SHOW CREATE TABLE table_name;
SHOW CREATE VIEW view_name;
SHOW CREATE PROCEDURE procedure_name;
SHOW CREATE FUNCTION function_name;
SHOW CREATE TRIGGER trigger_name;

-- Show system information
SHOW PROCESSLIST;
SHOW VARIABLES [LIKE 'pattern'];
SHOW STATUS [LIKE 'pattern'];
SHOW WARNINGS [LIMIT count];
SHOW ERRORS [LIMIT count];
SHOW GRANTS [FOR user_name];
SHOW PRIVILEGES;
```

### DESCRIBE Commands

```sql
-- Describe table structure
DESCRIBE table_name;
DESC table_name;
\d table_name           -- PostgreSQL style
\d+ table_name          -- Verbose

-- Describe other objects
DESCRIBE VIEW view_name;
DESCRIBE PROCEDURE procedure_name;
DESCRIBE FUNCTION function_name;
```

### SET Commands

```sql
-- Set session variables
SET parameter_name = value;
SET parameter_name TO value;
SET LOCAL parameter_name = value;    -- Transaction only
SET SESSION parameter_name = value;  -- Session only

-- Common settings
SET ROLE role_name;
SET SESSION AUTHORIZATION user_name;
SET TIME ZONE timezone;
SET SCHEMA schema_name;
SET SEARCH_PATH TO schema1, schema2, ...;
SET CLIENT_ENCODING TO encoding;
SET TRANSACTION ISOLATION LEVEL level;
SET CONSTRAINTS {ALL | name [, ...]} {DEFERRED | IMMEDIATE};
```

### EXPLAIN Commands

```sql
-- Explain query plan
EXPLAIN [ANALYZE] [VERBOSE] [COSTS] [BUFFERS] [TIMING] [SUMMARY] [FORMAT {TEXT | XML | JSON | YAML}]
  statement;

-- Explain options
ANALYZE     -- Actually execute (with timing)
VERBOSE     -- Show detailed output
COSTS       -- Show cost estimates
BUFFERS     -- Show buffer usage
TIMING      -- Show actual timing
SUMMARY     -- Show summary at end
```

### Administrative Commands

```sql
-- Analyze/vacuum
ANALYZE [VERBOSE] [table_name [(column_name [, ...])]];
VACUUM [FULL] [FREEZE] [VERBOSE] [ANALYZE] [table_name [(column_name [, ...])]];

-- Cluster
CLUSTER [VERBOSE] table_name [USING index_name];
CLUSTER [VERBOSE];

-- Reindex
REINDEX [VERBOSE] {INDEX | TABLE | SCHEMA | DATABASE} name;

-- Checkpoint
CHECKPOINT;

-- Reset
RESET parameter_name;
RESET ALL;

-- Discard
DISCARD {ALL | PLANS | SEQUENCES | TEMPORARY | TEMP};
```

---

## Schema Navigation

### Search Path Resolution

```sql
-- Set search path
SET SEARCH_PATH TO schema1, schema2, "$user", public;

-- Show current search path
SHOW SEARCH_PATH;
SELECT current_schemas(true);  -- Include implicit schemas

-- Fully qualified names
database.schema.table.column

-- Relative navigation
../table_name              -- Parent schema
../../schema/table_name    -- Navigate up and down
./table_name               -- Current schema (explicit)

-- Special schemas
[root]                     -- Root of hierarchy
[root].[sys]              -- System catalog
[root].[sec]              -- Security
[root].[app]              -- Applications
[root].[remote]           -- Remote databases
[root].[users]            -- User schemas
[root].[roles]            -- Role schemas
```

### Schema Introspection

```sql
-- Current schema
SELECT current_schema();

-- List schemas
SELECT schema_name FROM information_schema.schemata;

-- Schema exists
SELECT EXISTS (
  SELECT 1 FROM information_schema.schemata 
  WHERE schema_name = 'schema_name'
);

-- Objects in schema
SELECT * FROM information_schema.tables 
WHERE table_schema = 'schema_name';
```

---

## Extensions

### Event System

```sql
-- Define event
CREATE EVENT event_name;

-- Post event
POST_EVENT 'event_name' [WITH 'data'];

-- Wait for event (in PSQL)
WAIT_FOR_EVENT('event_name', timeout_seconds);

-- Register event handler (API level)
-- Handled through C API or client library
```

### Debugging Extensions

```sql
-- Debug output (in PSQL)
RAISE DEBUG 'Variable value: %', variable_name;
RAISE LOG 'Function entry: %', function_name;
RAISE INFO 'Processing row %', row_count;
RAISE NOTICE 'Checkpoint reached';
RAISE WARNING 'Unexpected value: %', value;

-- Trace execution
SET TRACE ON;
SET TRACE OFF;

-- Profiling
SET PROFILING = 1;
SHOW PROFILES;
SHOW PROFILE [FOR QUERY n];
```

### Temporary Tables

```sql
-- Session temporary table
CREATE TEMPORARY TABLE temp_table (...) 
  ON COMMIT {PRESERVE ROWS | DELETE ROWS | DROP};

-- Global temporary table
CREATE GLOBAL TEMPORARY TABLE gtt_table (...)
  ON COMMIT {PRESERVE ROWS | DELETE ROWS};

-- Transaction-scoped table
CREATE TEMP TABLE trans_temp (...) ON COMMIT DROP;
```

### Result Sets as First-Class Types

```sql
-- Return result set from function
CREATE FUNCTION get_data() 
RETURNS SETOF record AS $$
BEGIN
  RETURN QUERY SELECT * FROM table_name;
END;
$$ LANGUAGE plpgsql;

-- Pass result set to procedure
CREATE PROCEDURE process_data(data_set SETOF record) AS $$
BEGIN
  -- Process the result set
END;
$$ LANGUAGE plpgsql;
```

---

## Compatibility Features

### PostgreSQL Compatibility

```sql
-- Dollar quoting
$$text$$
$tag$text$tag$

-- :: casting
expression::type

-- RETURNING clause
INSERT ... RETURNING *;
UPDATE ... RETURNING *;
DELETE ... RETURNING *;

-- LATERAL joins
FROM table1, LATERAL (SELECT * FROM table2 WHERE table2.id = table1.id) AS t2

-- ON CONFLICT
INSERT ... ON CONFLICT (column) DO UPDATE SET ...;
```

### MySQL Compatibility

```sql
-- Backtick identifiers
`table_name`
`column name with spaces`

-- LIMIT without OFFSET
SELECT * FROM table LIMIT 10;

-- SHOW commands
SHOW TABLES;
SHOW CREATE TABLE table_name;

-- REPLACE INTO
REPLACE INTO table_name VALUES (...);

-- AUTO_INCREMENT
CREATE TABLE t (id INT AUTO_INCREMENT PRIMARY KEY);
```

### Firebird Compatibility

```sql
-- EXECUTE BLOCK
EXECUTE BLOCK [(parameter datatype = value [, ...])]
[RETURNS (parameter datatype [, ...])]
AS
BEGIN
  -- Block body
END

-- LIST() aggregate
SELECT LIST(column_name, ', ') FROM table_name;

-- POSITION function
POSITION(substring IN string)

-- Generators (sequences)
CREATE GENERATOR gen_name;
SET GENERATOR gen_name TO value;
GEN_ID(gen_name, increment)
```

### MSSQL Compatibility

```sql
-- Square bracket identifiers
[table name]
[column name]

-- TOP clause
SELECT TOP 10 * FROM table_name;
SELECT TOP 10 PERCENT * FROM table_name;

-- IDENTITY columns
CREATE TABLE t (id INT IDENTITY(1,1) PRIMARY KEY);

-- GO batch separator
-- Handled at client level

-- Pascal case preservation
CREATE TABLE MyTable (MyColumn INT);  -- Preserves case for display
```

---

## Operators

### Arithmetic Operators
```sql
+ - * / %                  -- Basic arithmetic
^                          -- Exponentiation
|/ ||/                     -- Square root, cube root
@ - abs()                  -- Absolute value
```

### Comparison Operators
```sql
= <> != < > <= >=          -- Standard comparison
IS NULL / IS NOT NULL      -- Null checks
IS DISTINCT FROM           -- Null-safe inequality
IS NOT DISTINCT FROM       -- Null-safe equality
BETWEEN ... AND ...        -- Range check
NOT BETWEEN ... AND ...    -- Inverse range
IN (...)                   -- Set membership
NOT IN (...)               -- Set non-membership
EXISTS (subquery)          -- Existence check
```

### String Operators
```sql
||                         -- Concatenation
LIKE / NOT LIKE           -- Pattern matching
ILIKE / NOT ILIKE         -- Case-insensitive pattern
SIMILAR TO                -- Regex pattern
~ ~* !~ !~*               -- Regex operators
```

### Logical Operators
```sql
AND OR NOT                -- Boolean logic
ALL ANY SOME              -- Quantified comparison
```

### Bitwise Operators
```sql
& | # ~ << >>             -- Bitwise operations
```

---

## Functions

### Aggregate Functions
```sql
COUNT(*) COUNT(expr) COUNT(DISTINCT expr)
SUM(expr) AVG(expr) 
MIN(expr) MAX(expr)
STDDEV(expr) VARIANCE(expr)
STRING_AGG(expr, delimiter)
ARRAY_AGG(expr)
JSON_AGG(expr)
XMLAGG(expr)
LIST(expr [, delimiter])  -- Firebird style
```

### Window Functions
```sql
ROW_NUMBER() OVER (...)
RANK() OVER (...)
DENSE_RANK() OVER (...)
PERCENT_RANK() OVER (...)
CUME_DIST() OVER (...)
NTILE(n) OVER (...)
LAG(expr [, offset [, default]]) OVER (...)
LEAD(expr [, offset [, default]]) OVER (...)
FIRST_VALUE(expr) OVER (...)
LAST_VALUE(expr) OVER (...)
NTH_VALUE(expr, n) OVER (...)
```

### String Functions
```sql
LENGTH(str) CHAR_LENGTH(str) BIT_LENGTH(str)
LOWER(str) UPPER(str) INITCAP(str)
TRIM([LEADING | TRAILING | BOTH] [chars] FROM str)
LTRIM(str [, chars]) RTRIM(str [, chars])
SUBSTRING(str FROM start [FOR length])
SUBSTR(str, start [, length])
POSITION(substring IN string)
REPLACE(str, from_str, to_str)
SPLIT_PART(str, delimiter, field)
CONCAT(str1, str2, ...)
CONCAT_WS(separator, str1, str2, ...)
REPEAT(str, count)
REVERSE(str)
```

### Date/Time Functions
```sql
CURRENT_DATE CURRENT_TIME CURRENT_TIMESTAMP
NOW() CLOCK_TIMESTAMP() STATEMENT_TIMESTAMP()
DATE_PART(field, timestamp)
EXTRACT(field FROM timestamp)
DATE_TRUNC(field, timestamp)
AGE(timestamp1, timestamp2)
INTERVAL 'value' unit
```

### Mathematical Functions
```sql
ABS(x) CEIL(x) FLOOR(x) ROUND(x [, d])
TRUNC(x [, d]) MOD(x, y)
POWER(x, y) SQRT(x) CBRT(x)
EXP(x) LN(x) LOG(b, x) LOG10(x)
SIN(x) COS(x) TAN(x) ASIN(x) ACOS(x) ATAN(x)
DEGREES(x) RADIANS(x)
PI() RANDOM() SETSEED(x)
```

### Type Conversion Functions
```sql
CAST(expr AS type)
expr::type                 -- PostgreSQL style
CONVERT(expr, type)        -- MySQL style
TO_CHAR(expr, format)
TO_DATE(str, format)
TO_NUMBER(str, format)
TO_TIMESTAMP(str, format)
```

### JSON Functions
```sql
JSON_BUILD_OBJECT(key, value, ...)
JSON_BUILD_ARRAY(value, ...)
JSON_OBJECT_AGG(key, value)
JSON_ARRAY_AGG(value)
JSON_EXTRACT(json, path)
JSON_EXTRACT_PATH(json, VARIADIC path)
JSONB_SET(json, path, new_value)
JSONB_INSERT(json, path, new_value)
```

### UUID Functions
```sql
UUID_GENERATE_V1()
UUID_GENERATE_V4()
UUID_GENERATE_V7()         -- ScratchBird preferred
UUID_TO_STRING(uuid)
STRING_TO_UUID(str)
```

---

## Comments and Documentation

### Object Comments

```sql
-- Add comments to objects
COMMENT ON DATABASE database_name IS 'comment';
COMMENT ON SCHEMA schema_name IS 'comment';
COMMENT ON TABLE table_name IS 'comment';
COMMENT ON COLUMN table_name.column_name IS 'comment';
COMMENT ON INDEX index_name IS 'comment';
COMMENT ON FUNCTION function_name(arg_types) IS 'comment';
COMMENT ON TRIGGER trigger_name ON table_name IS 'comment';

-- Remove comments
COMMENT ON object IS NULL;

-- View comments
SELECT obj_description('table_name'::regclass);
SELECT col_description('table_name'::regclass, column_number);
```

### Inline Documentation

```sql
-- Function documentation
CREATE FUNCTION function_name()
RETURNS type AS $$
/**
 * Function: function_name
 * Purpose: Brief description
 * Parameters: None
 * Returns: Description of return value
 * Author: Name
 * Date: YYYY-MM-DD
 */
BEGIN
  -- Implementation
END;
$$ LANGUAGE plpgsql;
```

---

## Special Features

### Context-Aware Parsing

```sql
-- Minimal reserved words
CREATE TABLE timestamp (timestamp timestamp);  -- 'timestamp' as identifier and type

-- Automatic termination detection
SELECT * FROM table1
SELECT * FROM table2  -- New statement detected

-- Intelligent completion
-- Parser understands context and suggests appropriate completions
```

### UUID-Based Object References

```sql
-- All objects have UUIDs (internal)
SELECT object_uuid FROM sys.objects WHERE object_name = 'table_name';

-- UUID references in BLR (transparent to user)
-- Enables rename without breaking compiled code
```

### Multi-Protocol Support

```sql
-- Connection can specify protocol
-- Y-Valve routes to appropriate parser
-- Same database accessible via PostgreSQL, MySQL, Firebird, TDS protocols
```

### 128-bit Integer Support

```sql
-- Native 128-bit integers
CREATE TABLE large_numbers (
  id INT128 PRIMARY KEY,
  unsigned_id UINT128
);
```

---

## Implementation Notes

### Parser Behavior
- Context-aware keyword recognition
- Automatic statement termination
- Multi-line statement support
- Intelligent error recovery

### Execution Model
- SQL → Parser → BLR → Engine
- Prepared statements cached as BLR
- Direct BLR execution available via API

### Compatibility Modes
- Protocol detected by Y-Valve
- Parser adapts to client protocol
- Emulation of protocol-specific features

### Performance Optimizations
- Bulk insert optimization
- Prepared statement caching
- Query result caching
- Parallel execution support

---

## Version History

- **Version 1.0**: Initial ScratchBird SQL dialect specification
- Combines best features from PostgreSQL, MySQL, Firebird, MSSQL
- Adds ScratchBird-specific enhancements

---

**Note**: This specification is comprehensive but may be extended based on implementation requirements. Features marked as compatibility features are provided for migration ease but native ScratchBird syntax is preferred.