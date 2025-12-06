# ScratchBird Miscellaneous SQL Commands Reference

**Last Updated:** December 6, 2025

This document provides a comprehensive reference for miscellaneous SQL commands in ScratchBird that don't fit into the standard DDL, DML, DCL, or TCL categories. These include utility, diagnostic, and procedural commands.

---

## Table of Contents

1. [Schema Information Commands](#schema-information-commands)
   - [DESCRIBE](#describe)
2. [Procedural Commands](#procedural-commands)
   - [CALL](#call)
3. [Table Maintenance Commands](#table-maintenance-commands)
   - [TRUNCATE TABLE](#truncate-table)
4. [Statistics and Optimization Commands](#statistics-and-optimization-commands)
   - [ANALYZE](#analyze)
   - [EXPLAIN](#explain)
5. [Materialized View Commands](#materialized-view-commands)
   - [REFRESH MATERIALIZED VIEW](#refresh-materialized-view)
6. [Tablespace Attachment Commands](#tablespace-attachment-commands)
   - [ATTACH TABLESPACE](#attach-tablespace)
   - [DETACH TABLESPACE](#detach-tablespace)

---

## Schema Information Commands

### DESCRIBE

Display the structure/schema of a table, including columns, data types, and constraints.

**Syntax (BNF):**
```bnf
<describe-statement> ::=
    { DESCRIBE | DESC } <table-name>

<table-name> ::= <identifier>
```

**SQL Syntax:**
```sql
DESCRIBE table_name
DESC table_name
```

**Description:**
- `DESCRIBE` and `DESC` are aliases for the same operation
- Shows complete table structure including:
  - Column names
  - Data types
  - Constraints (NOT NULL, PRIMARY KEY, FOREIGN KEY, etc.)
  - Default values
  - Generated columns
- This is a read-only operation that queries system catalog tables

**Implementation Details:**
- **Parser Function:** `parseDescribeStatement()` (line 5468)
- **AST Node:** `DescribeStmt` (ast.h line 3261)
- **Dispatch:** Main parser switch at line 610
- **AST Fields:**
  - `table_name_`: StringPool::StringId - The table to describe

**Examples:**
```sql
-- Using full keyword
DESCRIBE employees;

-- Using shorthand
DESC customers;

-- Schema-qualified table (if schema support implemented)
DESCRIBE public.orders;
```

**Notes:**
- Currently only accepts simple table names (identifiers)
- Does not support schema qualification in current implementation
- Alternative to `SHOW COLUMNS FROM table_name`
- Commonly used for quick schema inspection during development

---

## Procedural Commands

### CALL

Invoke a stored procedure with optional arguments.

**Syntax (BNF):**
```bnf
<call-statement> ::=
    CALL <procedure-name> <argument-list>

<procedure-name> ::= <identifier>

<argument-list> ::=
    LEFT_PAREN [ <expression> { COMMA <expression> }* ] RIGHT_PAREN

<expression> ::=
    <literal>
    | <column-reference>
    | <function-call>
    | <arithmetic-expression>
    | <string-expression>
    | <subquery>
    | ...
```

**SQL Syntax:**
```sql
CALL procedure_name()
CALL procedure_name(arg1)
CALL procedure_name(arg1, arg2, ...)
```

**Description:**
- Executes a stored procedure by name
- Supports zero or more arguments
- Arguments are arbitrary SQL expressions
- Parentheses are required even for zero arguments
- Procedure must be previously created with `CREATE PROCEDURE`

**Implementation Details:**
- **Parser Function:** `parseCallStatement()` (line 5488)
- **AST Node:** `CallStmt` (ast.h line 3928)
- **Dispatch:** Main parser switch at line 615
- **AST Fields:**
  - `procedure_name_`: StringPool::StringId - Procedure to invoke
  - `arguments_`: std::vector<Expression*> - List of argument expressions

**Parsing Algorithm:**
1. Consume `CALL` keyword (already done in dispatch)
2. Parse procedure name (must be identifier)
3. Expect `(` left parenthesis
4. If not immediately `)`
   - Parse first expression
   - While `,` comma found, parse next expression
5. Expect `)` right parenthesis
6. Create `CallStmt` AST node

**Examples:**
```sql
-- No arguments
CALL update_statistics();

-- Single argument
CALL process_order(12345);

-- Multiple arguments
CALL transfer_funds(100, 'USD', 'checking', 'savings');

-- With expressions as arguments
CALL calculate_discount(subtotal * 0.10, current_date);

-- With subquery argument
CALL bulk_update((SELECT id FROM pending_orders WHERE region = 'WEST'));
```

**Notes:**
- PostgreSQL-compatible syntax
- Arguments can be any valid expression
- Named parameter syntax not currently supported
- OUT/INOUT parameter handling depends on executor implementation
- Return values must be handled by executor (not parser)

---

## Table Maintenance Commands

### TRUNCATE TABLE

Quickly remove all rows from a table without logging individual row deletions.

**Syntax (BNF):**
```bnf
<truncate-statement> ::=
    TRUNCATE [ TABLE ] <table-name> [ <truncate-mode> ]

<table-name> ::= <identifier>

<truncate-mode> ::=
    ASYNC | SYNC
```

**SQL Syntax:**
```sql
TRUNCATE [TABLE] table_name [ASYNC | SYNC]
```

**Description:**
- `TABLE` keyword is optional
- Removes all rows from the specified table
- Much faster than `DELETE FROM table` for large tables
- Default mode is `ASYNC` (background operation)
- `SYNC` mode blocks until operation completes
- Does not fire DELETE triggers
- Resets auto-increment/identity counters

**Truncate Modes:**
- `ASYNC`: Operation runs in background (non-blocking, default)
- `SYNC`: Operation blocks until complete (ensures immediate effect)

**Implementation Details:**
- **Parser Function:** `parseTruncateTable()` (line 5750)
- **AST Node:** `TruncateTableStmt` (ast.h line 1533)
- **Dispatch:** Main parser switch at line 521-523
- **AST Fields:**
  - `table_name_`: StringPool::StringId - Table to truncate
  - `mode_`: TruncateMode enum (ASYNC=0, SYNC=1)
- **Enum Definition:** `TruncateMode::ASYNC` | `TruncateMode::SYNC`

**Parsing Algorithm:**
1. Consume `TRUNCATE` keyword
2. Optionally consume `TABLE` keyword
3. Parse table name (must be identifier)
4. Check for mode keyword:
   - If `SYNC` found → mode = SYNC
   - If `ASYNC` found → mode = ASYNC
   - Otherwise → mode = ASYNC (default)
5. Create `TruncateTableStmt` with table name and mode

**Examples:**
```sql
-- Basic truncate (async by default)
TRUNCATE TABLE logs;

-- Without TABLE keyword
TRUNCATE sessions;

-- Explicit async mode
TRUNCATE TABLE temp_data ASYNC;

-- Synchronous mode (blocks until complete)
TRUNCATE TABLE staging_area SYNC;
```

**Notes:**
- Does not support CASCADE/RESTRICT (unlike PostgreSQL)
- Does not support multiple tables in single statement
- Foreign key constraints may prevent truncation
- More efficient than DELETE for removing all rows
- Cannot be rolled back if auto-commit is on
- ASYNC mode is unique to ScratchBird (Firebird-inspired)

**Comparison with DELETE:**
```sql
-- TRUNCATE: Fast, resets sequences, no triggers
TRUNCATE TABLE huge_table;

-- DELETE: Slow, keeps sequences, fires triggers
DELETE FROM huge_table;
```

---

## Statistics and Optimization Commands

### ANALYZE

Collect or update statistics on table columns for query optimization.

**Syntax (BNF):**
```bnf
<analyze-statement> ::=
    ANALYZE <table-name> [ <column-clause> ] [ <sample-clause> ]

<table-name> ::= <identifier>

<column-clause> ::=
    COLUMN <column-name>

<column-name> ::= <identifier>

<sample-clause> ::=
    SAMPLE <sample-rate>

<sample-rate> ::=
    <float-literal> | <integer-literal>
```

**SQL Syntax:**
```sql
ANALYZE table_name
ANALYZE table_name COLUMN column_name
ANALYZE table_name SAMPLE sample_rate
ANALYZE table_name COLUMN column_name SAMPLE sample_rate
```

**Description:**
- Collects statistical information about table data
- Used by query optimizer to choose efficient execution plans
- Can analyze all columns or a specific column
- Supports sampling for faster statistics on large tables
- Sample rate must be between 0.0 and 1.0 (0.0 = auto-determine)

**Options:**
- `COLUMN column_name`: Analyze only the specified column
- `SAMPLE rate`: Use sampling (0.0-1.0, where 1.0 = 100% of rows)

**Implementation Details:**
- **Parser Function:** `parseAnalyze()` (line 3996)
- **AST Node:** `AnalyzeStmt` (ast.h line 2562)
- **Dispatch:** Main parser switch at line 408-410
- **AST Fields:**
  - `table_name_`: StringPool::StringId - Table to analyze
  - `column_name_`: StringPool::StringId - Specific column (0 = all columns)
  - `sample_rate_`: float - Sampling rate (0.0 = auto, 0.0-1.0 = explicit)

**Parsing Algorithm:**
1. Consume `ANALYZE` keyword
2. Parse table name (must be identifier)
3. If `COLUMN` keyword found:
   - Parse column name (must be identifier)
4. If `SAMPLE` keyword found:
   - Parse numeric literal (float or integer)
   - Validate range (0.0 to 1.0)
   - If out of range, report error
5. Create `AnalyzeStmt` with parsed values

**Examples:**
```sql
-- Analyze all columns in table
ANALYZE customers;

-- Analyze specific column
ANALYZE orders COLUMN order_date;

-- Analyze with 10% sampling
ANALYZE large_transactions SAMPLE 0.1;

-- Analyze specific column with sampling
ANALYZE sales COLUMN amount SAMPLE 0.25;

-- Full scan (100% sample)
ANALYZE inventory SAMPLE 1.0;
```

**Statistics Collected:**
- Number of distinct values (NDV)
- Minimum and maximum values
- Null count and null fraction
- Average column width
- Most common values (MCV)
- Histogram buckets for value distribution

**Notes:**
- Phase 1 Task 1.1.2 implementation
- Essential for cost-based query optimization
- Should be run after bulk data loads
- Auto-vacuum systems may run ANALYZE automatically
- Sample rate of 0.0 lets optimizer choose sample size
- Lower sample rates are faster but less accurate
- Statistics stored in system catalog tables

**When to Use:**
- After loading significant amounts of data
- After major UPDATE operations that change data distribution
- Before running complex queries that need optimization
- Periodically for tables with changing data patterns

---

### EXPLAIN

Display the query execution plan for a SQL statement.

**Syntax (BNF):**
```bnf
<explain-statement> ::=
    EXPLAIN <statement>

<statement> ::=
    <select-statement>
    | <insert-statement>
    | <update-statement>
    | <delete-statement>
    | <merge-statement>
    | ...
```

**SQL Syntax:**
```sql
EXPLAIN SELECT ...
EXPLAIN INSERT ...
EXPLAIN UPDATE ...
EXPLAIN DELETE ...
```

**Description:**
- Shows the execution plan chosen by the query optimizer
- Helps identify performance bottlenecks
- Does NOT execute the statement (only plans it)
- Currently Phase 1.5 implementation supports SELECT only
- Future phases will support all DML statements

**Implementation Details:**
- **Parser Function:** `parseExplain()` (line 4063)
- **AST Node:** `ExplainStmt` (ast.h line 2612)
- **Dispatch:** Main parser switch at line 412-414
- **AST Fields:**
  - `query_`: Statement* - The statement to explain
- **Current Limitation:** Only SELECT statements validated (Phase 1.5)

**Parsing Algorithm:**
1. Consume `EXPLAIN` keyword
2. Recursively parse the statement to explain
3. Verify parse was successful
4. Verify it's a SELECT statement (Phase 1.5 limitation)
5. Create `ExplainStmt` wrapping the query

**Examples:**
```sql
-- Explain simple query
EXPLAIN SELECT * FROM users WHERE active = true;

-- Explain join query
EXPLAIN
SELECT o.order_id, c.customer_name
FROM orders o
JOIN customers c ON o.customer_id = c.id
WHERE o.order_date > '2025-01-01';

-- Explain subquery
EXPLAIN
SELECT product_name
FROM products
WHERE category_id IN (
    SELECT id FROM categories WHERE active = true
);

-- Explain with aggregation
EXPLAIN
SELECT department, COUNT(*), AVG(salary)
FROM employees
GROUP BY department
HAVING AVG(salary) > 50000;
```

**Expected Output Information:**
- Node type (Seq Scan, Index Scan, Join, etc.)
- Table/index being accessed
- Filter conditions
- Join conditions and type
- Sort operations
- Aggregate operations
- Cost estimates
- Row count estimates

**Notes:**
- Phase 1 Task 1.5 implementation
- Does not include EXPLAIN ANALYZE (execution timing)
- Does not include output format options (JSON, YAML, etc.)
- Future enhancements may include:
  - `EXPLAIN ANALYZE` - Execute and show actual timing
  - `EXPLAIN (FORMAT JSON)` - Structured output
  - `EXPLAIN (VERBOSE)` - Additional details
  - Support for all statement types (INSERT, UPDATE, DELETE, MERGE)

**Use Cases:**
- Debugging slow queries
- Verifying index usage
- Comparing different query formulations
- Understanding optimizer decisions
- Query tuning and optimization

---

## Materialized View Commands

### REFRESH MATERIALIZED VIEW

Rebuild the contents of a materialized view from its defining query.

**Syntax (BNF):**
```bnf
<refresh-statement> ::=
    REFRESH [ CONCURRENTLY ] MATERIALIZED VIEW <view-name>

<view-name> ::= <identifier>
```

**SQL Syntax:**
```sql
REFRESH MATERIALIZED VIEW view_name
REFRESH CONCURRENTLY MATERIALIZED VIEW view_name
```

**Description:**
- Recomputes the materialized view by re-executing its query
- Replaces the old contents with fresh data
- `CONCURRENTLY` option allows non-blocking refresh (queries can continue)
- Without `CONCURRENTLY`, the view is locked during refresh
- Required when underlying table data changes

**Options:**
- `CONCURRENTLY`: Non-blocking refresh (allows concurrent reads)

**Implementation Details:**
- **Parser Function:** `parseRefreshMaterializedView()` (line 6238)
- **AST Node:** `RefreshMaterializedViewStmt` (ast.h line 1902)
- **Dispatch:** Main parser switch at line 416-418
- **AST Fields:**
  - `name_`: StringPool::StringId - View name to refresh
  - `concurrently_`: bool - Whether to refresh concurrently

**Parsing Algorithm:**
1. Consume `REFRESH` keyword
2. Check for `CONCURRENTLY` keyword (optional)
3. Consume `MATERIALIZED` keyword (required)
4. Consume `VIEW` keyword (required)
5. Parse view name (must be identifier)
6. Create `RefreshMaterializedViewStmt` with name and concurrent flag

**Examples:**
```sql
-- Basic refresh (blocking)
REFRESH MATERIALIZED VIEW sales_summary;

-- Concurrent refresh (non-blocking)
REFRESH CONCURRENTLY MATERIALIZED VIEW monthly_reports;

-- Refresh after data change
INSERT INTO sales VALUES (...);
REFRESH MATERIALIZED VIEW sales_by_region;
```

**Refresh Modes:**

| Mode | Blocking | Use Case |
|------|----------|----------|
| Standard | Yes | Low-traffic periods, small views |
| CONCURRENTLY | No | Production systems, large views |

**Notes:**
- ALPHA Phase 1 - Materialized Views implementation
- Standard refresh locks the view (exclusive access)
- Concurrent refresh requires a unique index on the view
- More expensive than incremental refresh (recomputes everything)
- Alternative to automatic refresh triggers
- Consider scheduling during low-traffic periods

**Performance Considerations:**
- Standard refresh is faster but blocks queries
- Concurrent refresh is slower but allows queries
- For large views, consider:
  - Partitioning the underlying data
  - Using CONCURRENTLY option
  - Scheduling during off-peak hours
  - Incremental refresh (if supported in future)

**Related Commands:**
```sql
-- Create a materialized view
CREATE MATERIALIZED VIEW sales_summary AS
SELECT region, SUM(amount) as total_sales
FROM sales
GROUP BY region;

-- Query the view (uses cached data)
SELECT * FROM sales_summary;

-- Refresh when data changes
REFRESH MATERIALIZED VIEW sales_summary;
```

---

## Tablespace Attachment Commands

### ATTACH TABLESPACE

Attach an existing tablespace file to the database.

**Syntax (BNF):**
```bnf
<attach-statement> ::=
    ATTACH TABLESPACE <file-path> [ AS <tablespace-name> ]

<file-path> ::= <string-literal>

<tablespace-name> ::= <string-literal> | <identifier>
```

**SQL Syntax:**
```sql
ATTACH TABLESPACE 'file_path'
ATTACH TABLESPACE 'file_path' AS 'tablespace_name'
ATTACH TABLESPACE 'file_path' AS identifier_name
```

**Description:**
- Attaches an existing tablespace file (.sbts) to the database
- Makes the tablespace available for table/index placement
- Optional `AS` clause specifies a logical name
- If no name provided, uses filename as tablespace name
- File must exist and be a valid ScratchBird tablespace

**Options:**
- `AS name`: Assign a logical name to the tablespace

**Implementation Details:**
- **Parser Function:** `parseAttachTablespace()` (line 6317)
- **AST Node:** `AttachTablespaceStmt` (ast.h line 2939)
- **Dispatch:** Main parser switch at line 525-535
- **AST Fields:**
  - `file_path_`: StringPool::StringId - Path to .sbts file
  - `tablespace_name_`: StringPool::StringId - Optional logical name (0 = use filename)

**Parsing Algorithm:**
1. Consume `ATTACH` keyword (already done in dispatch)
2. Consume `TABLESPACE` keyword (required)
3. Parse file path (must be string literal)
4. If `AS` keyword found:
   - Parse name (string literal or identifier)
5. Create `AttachTablespaceStmt` with path and optional name

**Examples:**
```sql
-- Attach with filename as name
ATTACH TABLESPACE '/data/tablespaces/archive.sbts';

-- Attach with custom name (string literal)
ATTACH TABLESPACE '/data/tablespaces/ts_2024.sbts' AS 'archive_2024';

-- Attach with custom name (identifier)
ATTACH TABLESPACE '/data/tablespaces/fast_ssd.sbts' AS fast_storage;

-- Attach multiple tablespaces
ATTACH TABLESPACE '/data/ts1.sbts' AS ts1;
ATTACH TABLESPACE '/data/ts2.sbts' AS ts2;
```

**Use Cases:**
- Mounting archived data
- Adding external storage
- Database migration
- Multi-database sharing
- Separating hot/cold data

**Notes:**
- Phase 6 Task 6.1 implementation
- File path must be absolute or relative to database directory
- Tablespace must not already be attached
- Name must be unique within the database
- Does not create a new tablespace (use CREATE TABLESPACE for that)
- File permissions must allow database access

**Related Commands:**
```sql
-- Create a new tablespace
CREATE TABLESPACE archive LOCATION '/data/archive.sbts';

-- Attach existing tablespace
ATTACH TABLESPACE '/data/existing.sbts' AS external;

-- Use tablespace for table
CREATE TABLE orders (...)
TABLESPACE archive;

-- Detach when no longer needed
DETACH TABLESPACE archive;
```

**Error Conditions:**
- File does not exist
- File is not a valid tablespace
- Insufficient permissions
- Tablespace name already in use
- File already attached

---

### DETACH TABLESPACE

Detach a tablespace from the database.

**Syntax (BNF):**
```bnf
<detach-statement> ::=
    DETACH TABLESPACE <tablespace-name> [ FORCE ]

<tablespace-name> ::= <identifier>
```

**SQL Syntax:**
```sql
DETACH TABLESPACE tablespace_name
DETACH TABLESPACE tablespace_name FORCE
```

**Description:**
- Detaches a tablespace from the database
- Removes the logical association but does not delete the file
- Default behavior prevents detach if tables/indexes exist
- `FORCE` option allows detach even with existing objects
- File remains on disk and can be re-attached later

**Options:**
- `FORCE`: Detach even if objects exist in the tablespace

**Implementation Details:**
- **Parser Function:** `parseDetachTablespace()` (line 6356)
- **AST Node:** `DetachTablespaceStmt` (ast.h line 2966)
- **Dispatch:** Main parser switch at line 537-547
- **AST Fields:**
  - `tablespace_name_`: StringPool::StringId - Tablespace to detach
  - `force_`: bool - Whether to force detachment

**Parsing Algorithm:**
1. Consume `DETACH` keyword (already done in dispatch)
2. Consume `TABLESPACE` keyword (required)
3. Parse tablespace name (must be identifier)
4. Check for `FORCE` keyword (optional)
5. Create `DetachTablespaceStmt` with name and force flag

**Examples:**
```sql
-- Detach empty tablespace
DETACH TABLESPACE archive;

-- Force detach (even with objects)
DETACH TABLESPACE old_data FORCE;

-- Detach after moving objects
ALTER TABLE orders SET TABLESPACE pg_default;
DETACH TABLESPACE archive;
```

**Detach Modes:**

| Mode | Behavior | Safety |
|------|----------|--------|
| Standard | Fails if objects exist | Safe (prevents data loss) |
| FORCE | Succeeds regardless | Unsafe (may orphan objects) |

**Notes:**
- Phase 6 Task 6.2 implementation
- Does not delete the physical file
- File can be re-attached later with ATTACH TABLESPACE
- FORCE should be used with caution
- Objects in forced-detached tablespace become inaccessible
- Best practice: Move objects before detaching

**Recommended Workflow:**
```sql
-- 1. Find objects in tablespace
SELECT tablename FROM pg_tables WHERE tablespace = 'old_data';

-- 2. Move objects to different tablespace
ALTER TABLE table1 SET TABLESPACE pg_default;
ALTER TABLE table2 SET TABLESPACE pg_default;

-- 3. Detach safely
DETACH TABLESPACE old_data;

-- 4. File still exists for archival or re-attachment
```

**Force Detach Scenario:**
```sql
-- Detach read-only archive immediately
DETACH TABLESPACE archive_2023 FORCE;

-- Objects become inaccessible but can be restored by re-attaching
ATTACH TABLESPACE '/data/archive_2023.sbts' AS archive_2023;
```

**Error Conditions:**
- Tablespace does not exist
- Tablespace is not attached
- Objects exist (without FORCE)
- Tablespace is system tablespace (pg_default, pg_global)

**Comparison with DROP TABLESPACE:**
```sql
-- DETACH: Removes association, keeps file
DETACH TABLESPACE old_data;
-- File remains at /data/old_data.sbts

-- DROP: Removes association AND deletes file
DROP TABLESPACE old_data;
-- File deleted permanently
```

---

## Summary Table

| Command | Category | Purpose | Key Options |
|---------|----------|---------|-------------|
| DESCRIBE / DESC | Schema Info | Show table structure | - |
| CALL | Procedural | Invoke stored procedure | Arguments (expressions) |
| TRUNCATE TABLE | Maintenance | Remove all rows quickly | ASYNC / SYNC |
| ANALYZE | Statistics | Collect optimizer statistics | COLUMN, SAMPLE |
| EXPLAIN | Diagnostic | Show query execution plan | Statement type |
| REFRESH MATERIALIZED VIEW | Maintenance | Rebuild materialized view | CONCURRENTLY |
| ATTACH TABLESPACE | Storage | Attach existing tablespace | AS (name) |
| DETACH TABLESPACE | Storage | Detach tablespace | FORCE |

---

## Implementation Status

| Command | Parser Status | Executor Status | Phase |
|---------|---------------|-----------------|-------|
| DESCRIBE | Complete | Unknown | - |
| CALL | Complete | Unknown | - |
| TRUNCATE TABLE | Complete | Unknown | ALPHA Phase 1 |
| ANALYZE | Complete | Unknown | Phase 1 Task 1.1.2 |
| EXPLAIN | Complete (SELECT only) | Unknown | Phase 1 Task 1.5 |
| REFRESH MATERIALIZED VIEW | Complete | Unknown | ALPHA Phase 1 |
| ATTACH TABLESPACE | Complete | Unknown | Phase 6 Task 6.1 |
| DETACH TABLESPACE | Complete | Unknown | Phase 6 Task 6.2 |

---

## Parser Dispatch Summary

Commands are dispatched from the main `parseStatement()` function:

```cpp
// Lines 82-87: Statement type checking
case TokenType::KW_ANALYZE:  // Phase 1 Task 1.1.2
case TokenType::KW_EXPLAIN:  // Phase 1 Task 1.5
case TokenType::KW_REFRESH:  // ALPHA Phase 1 - Materialized Views

// Lines 408-418: Statistics and optimization
else if (match(TokenType::KW_ANALYZE))
    stmt = parseAnalyze();
else if (match(TokenType::KW_EXPLAIN))
    stmt = parseExplain();
else if (match(TokenType::KW_REFRESH))
    stmt = parseRefreshMaterializedView();

// Lines 521-523: Table maintenance
else if (match(TokenType::KW_TRUNCATE))
    stmt = parseTruncateTable();

// Lines 525-547: Tablespace attachment
else if (match(TokenType::KW_ATTACH))
    if (check(TokenType::KW_TABLESPACE))
        stmt = parseAttachTablespace();
else if (match(TokenType::KW_DETACH))
    if (check(TokenType::KW_TABLESPACE))
        stmt = parseDetachTablespace();

// Lines 610-617: Utility commands
else if (match(TokenType::KW_DESCRIBE) || match(TokenType::KW_DESC))
    stmt = parseDescribeStatement();
else if (match(TokenType::KW_CALL))
    stmt = parseCallStatement();
```

---

## Token Definitions

Relevant tokens defined in `/include/scratchbird/parser/token.h`:

```cpp
KW_ANALYZE,      // Line 97  - Phase 1 Task 1.1.2: Statistics collection
KW_EXPLAIN,      // Line 98  - Phase 1 Task 1.5: EXPLAIN command
KW_COLUMN,       // Line 99  - Phase 1 Task 1.1.2: ANALYZE ... COLUMN ...
KW_SAMPLE,       // Line 100 - Phase 1 Task 1.1.2: ANALYZE ... SAMPLE ...
KW_DESC,         // Line 119 - ORDER BY DESC and DESCRIBE shorthand
KW_TRUNCATE,     // Line 339 - ALPHA Phase 1 - TRUNCATE TABLE
KW_REFRESH,      // Line 356 - ALPHA Phase 1 - Materialized Views
KW_CONCURRENTLY, // Line 357 - ALPHA Phase 1 - REFRESH CONCURRENTLY
KW_ATTACH,       // Line 384 - Phase 6 Task 6.1 - ATTACH TABLESPACE
KW_DETACH,       // Line 385 - Phase 6 Task 6.2 - DETACH TABLESPACE
KW_DESCRIBE,     // Line 459 - DESCRIBE table
KW_CALL,         // Line 499 - CALL procedure_name(args...)
```

---

## AST Node Locations

AST node definitions in `/include/scratchbird/parser/ast.h`:

```cpp
TruncateTableStmt           // Line 1533 - TRUNCATE TABLE
RefreshMaterializedViewStmt // Line 1902 - REFRESH MATERIALIZED VIEW
AnalyzeStmt                 // Line 2562 - ANALYZE
ExplainStmt                 // Line 2612 - EXPLAIN
AttachTablespaceStmt        // Line 2939 - ATTACH TABLESPACE
DetachTablespaceStmt        // Line 2966 - DETACH TABLESPACE
DescribeStmt                // Line 3261 - DESCRIBE
CallStmt                    // Line 3928 - CALL
```

---

## Future Enhancements

Possible extensions for these commands:

### DESCRIBE
- Schema-qualified names: `DESCRIBE schema.table`
- Output format options: `DESCRIBE FORMAT JSON table`
- Show indexes: `DESCRIBE INDEXES table`

### CALL
- Named parameters: `CALL proc(param1 => value1, param2 => value2)`
- OUT parameter support in syntax
- Return value handling: `result = CALL function()`

### TRUNCATE TABLE
- Multiple tables: `TRUNCATE TABLE t1, t2, t3`
- CASCADE/RESTRICT: `TRUNCATE TABLE t1 CASCADE`
- RESTART IDENTITY: `TRUNCATE TABLE t1 RESTART IDENTITY`

### ANALYZE
- Verbose output: `ANALYZE VERBOSE table`
- Skip locked: `ANALYZE table SKIP LOCKED`
- Partitions: `ANALYZE table PARTITION (p1, p2)`

### EXPLAIN
- Analyze execution: `EXPLAIN ANALYZE SELECT ...`
- Output formats: `EXPLAIN (FORMAT JSON) SELECT ...`
- Options: `EXPLAIN (VERBOSE, BUFFERS) SELECT ...`
- All statement types (currently SELECT only)

### REFRESH MATERIALIZED VIEW
- WITH DATA / WITH NO DATA: `REFRESH ... WITH NO DATA`
- Incremental refresh for large views
- Automatic refresh policies

### ATTACH/DETACH TABLESPACE
- Read-only mode: `ATTACH TABLESPACE ... READ ONLY`
- Automatic path resolution
- Multiple attach/detach in single statement

---

## Comparative Notes

### PostgreSQL Compatibility

| Feature | ScratchBird | PostgreSQL | Notes |
|---------|-------------|------------|-------|
| DESCRIBE | Yes | Via psql (\d) | SQL command vs psql meta-command |
| CALL | Yes | Yes | Compatible syntax |
| TRUNCATE | Partial | Full | Missing CASCADE, RESTART IDENTITY |
| ANALYZE | Partial | Full | Missing VERBOSE, partition support |
| EXPLAIN | Partial | Full | Missing ANALYZE, formats, options |
| REFRESH MV | Yes | Yes | Compatible |
| ATTACH/DETACH | ScratchBird-specific | N/A | Unique feature |

### Firebird Compatibility

| Feature | ScratchBird | Firebird | Notes |
|---------|-------------|----------|-------|
| TRUNCATE ASYNC/SYNC | Yes | No | ScratchBird extension |
| Execute procedure | CALL | EXECUTE PROCEDURE | Different syntax |
| Analyze | ANALYZE | SET STATISTICS | Different mechanism |

---

**End of Document**
