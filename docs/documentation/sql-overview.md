### SQL Language Overview

**What it is**

ScratchBird implements a comprehensive SQL dialect that combines standard SQL features with advanced capabilities inspired by PostgreSQL, Firebird, and other enterprise databases. The engine parses SQL statements through a modular architecture where each statement type has a dedicated parser, providing clear separation of concerns and maintainability.

**Why it matters**

- **Complete SQL Support**: Understand the full range of SQL statements supported, from basic SELECT to advanced PSQL blocks
- **Parser Architecture**: Learn how statements are routed and processed through the engine pipeline
- **Semantics vs Parsing**: Distinguish between what the parser accepts and what the runtime actually executes
- **Code Navigation**: Quickly locate implementation details for any SQL feature in the codebase

**How to use it**

This overview provides a map of the SQL surface area. Each statement type links to detailed documentation with examples. Use this page to understand the overall architecture, then dive into specific features as needed.

## Statement Categories and Routing

The main SQL router (`src/engine/parser.cpp`) examines the beginning of each statement and dispatches to specialized parsers:

### 1. Data Query Language (DQL)
**Parser**: `src/engine/parser_select.cpp` (`parse_select_minimal`)  
**Header**: `include/scratchbird/engine/parser_select.h`

- **SELECT** statements with full SQL capabilities:
  - WITH/WITH RECURSIVE common table expressions (CTEs)
  - Complex FROM clauses with subqueries and LATERAL joins
  - JOIN operations: INNER, LEFT, RIGHT, FULL, CROSS, NATURAL
  - WHERE, GROUP BY, HAVING clauses
  - Window functions with OVER clauses
  - Set operations: UNION, INTERSECT, EXCEPT (with ALL variants)
  - ORDER BY with NULLS FIRST/LAST
  - LIMIT/OFFSET and FETCH FIRST/NEXT
  - FOR UPDATE locking clauses
  - PLAN hints for optimizer control

### 2. Data Manipulation Language (DML)
**Parser**: `src/engine/parser_dml.cpp`  
**Header**: `include/scratchbird/engine/parser_dml.h`

- **INSERT**: Single/multi-row, VALUES, SELECT source, RETURNING clause
- **UPDATE**: SET assignments, FROM joins, WHERE conditions, RETURNING
- **DELETE**: WHERE filtering, USING joins, RETURNING
- **MERGE**: MATCHED/NOT MATCHED actions, complex conditions
- **UPSERT**: ON CONFLICT DO UPDATE/NOTHING (PostgreSQL-style)

### 3. Data Definition Language (DDL)
**Parser**: `src/engine/parser_ddl.cpp`  
**Headers**: Various specialized headers for each object type

#### Core Objects
- **Tables** (`parse_ddl_table`): CREATE, ALTER (20+ operations), DROP, RECREATE
- **Indexes** (`parse_ddl_index`): B-tree, Hash, Bitmap, GIN, RTtree, partial, expression-based
- **Schemas** (`parse_ddl_schema`): Namespace management
- **Views** (`parse_ddl_view`): Standard and WITH CHECK OPTION
- **Sequences** (`parse_ddl_sequence`): Auto-increment generators
- **Domains** (`parse_ddl_domain`): Custom types with constraints

#### Advanced Objects
- **Tablespaces** (`parse_ddl_tablespace`): Storage management with LOCATION/FILE
- **Foreign Data** (`parse_ddl_foreign_*`): Servers, user mappings, foreign tables
- **Materialized Views** (`parse_ddl_materialized_view`): Cached query results
- **Publication/Subscription** (`parse_ddl_publication/subscription`): Logical replication
- **Policies** (`parse_ddl_rls_policy`): Row-level security
- **Clusters** (`parse_ddl_cluster*`): Multi-node configuration

#### Security and Administration
- **Roles/Users** (`parse_ddl_role/user`): Authentication entities
- **Grants/Revokes** (`parse_ddl_grant/revoke`): Privilege management
- **Comments** (`parse_ddl_comment`): Object documentation

### 4. Session and Transaction Control
**Parser**: `src/engine/parser_session.cpp`  
**Header**: `include/scratchbird/engine/parser_session.h`

- **Database Management**: CREATE/ALTER/DROP DATABASE
- **Connections**: CONNECT, DISCONNECT
- **Transactions**: BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RELEASE
- **Session Settings**: SET NAMES, SET ROLE, SET TRANSACTION, SET various options
- **Maintenance**: ANALYZE, VACUUM, EXPLAIN, SET CONSTRAINTS

### 5. Procedural SQL (PSQL)
**Parser**: `src/engine/parser_psql.cpp`  
**Executor**: `src/engine/psql_executor.cpp`  
**Headers**: `include/scratchbird/engine/parser_psql.h`, `include/scratchbird/engine/psql_executor.h`

- **EXECUTE BLOCK**: Anonymous code blocks
- **Stored Procedures/Functions**: CREATE/ALTER/DROP PROCEDURE/FUNCTION
- **Packages**: Grouping of routines
- **Triggers**: Event-driven code execution
- **Control Flow**: IF/THEN/ELSE, WHILE, FOR SELECT INTO, LEAVE, CONTINUE
- **Cursors**: DECLARE, OPEN, FETCH, CLOSE
- **Exception Handling**: EXCEPTION blocks with WHEN handlers
- **Dynamic SQL**: EXECUTE STATEMENT with parameter binding

## Expression System

**Parser**: `src/engine/parser_expr.cpp`  
**Evaluator**: `src/engine/expr.cpp`  
**Header**: `include/scratchbird/engine/expr.h`

The expression system handles:

### Parsing Capabilities (Full)
- Arithmetic operators: `+`, `-`, `*`, `/`, `%`
- Comparison operators: `=`, `!=`, `<>`, `<`, `<=`, `>`, `>=`
- Logical operators: `AND`, `OR`, `NOT`
- Pattern matching: `LIKE`, `SIMILAR TO`, `~` (regex)
- Range/membership: `IN`, `BETWEEN`, `EXISTS`
- NULL handling: `IS NULL`, `IS NOT NULL`, `IS DISTINCT FROM`
- Type operations: `::` cast, `COLLATE`
- Array/JSON operators: `[]`, `->`, `->>`

### Runtime Evaluation (Subset)
Currently, the runtime evaluator (`evaluate_predicate`) supports:
- Boolean operators: `AND`, `OR`, `NOT`
- Comparisons: `=`, `!=`, `<`, `<=`, `>`, `>=`
- NULL checks: `IS NULL`, `IS NOT NULL`
- Column references and literals (integer, string)
- Parenthesized expressions

**Note on Semantics vs Parsing**: The parser accepts a broader range of expressions than the current runtime evaluator implements. This allows for parser validation while runtime features are incrementally added.

## Lexical Analysis

**Lexer**: `src/engine/lexer.cpp`  
**Header**: `include/scratchbird/engine/lexer.h`

Token types recognized:
- **Identifiers**: Regular and quoted (`"identifier"`)
- **Keywords**: Reserved words (case-insensitive)
- **Literals**: 
  - Numeric: Integer, Decimal
  - String: Single-quoted, dollar-quoted (`$$text$$`)
  - Temporal: DATE, TIME, TIMESTAMP literals
  - UUID: Standard UUID format
- **Symbols**: Single and multi-character operators
- **Comments**: `--` line comments, `/* */` block comments
- **Character sets**: `N'string'` or `_UTF8'string'` prefixes

## Type System

**Parser**: `src/engine/types.cpp` (`parse_type_spec`)  
**Header**: `include/scratchbird/engine/types.h`

Supported type categories:
- **Numeric**: TINYINT, SMALLINT, INTEGER, BIGINT, NUMERIC(p,s), DECIMAL(p,s), FLOAT, DOUBLE
- **Text**: CHAR(n), VARCHAR(n), CITEXT with CHARACTER SET and COLLATE
- **Binary**: BLOB, BYTEA
- **Temporal**: DATE, TIME, TIMESTAMP, with/without TIME ZONE
- **Boolean**: BOOLEAN
- **UUID**: UUID type
- **JSON**: JSON, JSONB
- **Network**: INET, CIDR, MACADDR
- **Ranges**: INT4RANGE, INT8RANGE, DATERANGE, TSRANGE
- **Arrays**: Type arrays (e.g., `INTEGER[]`)
- **Composite**: User-defined composite types

## Examples

### Complex SELECT with CTEs and Window Functions
```sql
WITH RECURSIVE hierarchy AS (
    SELECT id, name, parent_id, 1 as level
    FROM departments 
    WHERE parent_id IS NULL
    UNION ALL
    SELECT d.id, d.name, d.parent_id, h.level + 1
    FROM departments d
    JOIN hierarchy h ON d.parent_id = h.id
),
ranked_employees AS (
    SELECT 
        e.name,
        e.salary,
        d.name as dept_name,
        ROW_NUMBER() OVER (PARTITION BY e.dept_id ORDER BY e.salary DESC) as rank
    FROM employees e
    JOIN departments d ON e.dept_id = d.id
)
SELECT * FROM ranked_employees WHERE rank <= 3
ORDER BY dept_name, rank;
```

### DML with RETURNING
```sql
-- Insert with RETURNING
INSERT INTO orders (customer_id, order_date, total)
VALUES (123, CURRENT_DATE, 99.99)
RETURNING id, order_date;

-- Update with JOIN and RETURNING
UPDATE inventory i
SET quantity = quantity - ol.quantity
FROM order_lines ol
WHERE i.product_id = ol.product_id 
  AND ol.order_id = 456
RETURNING i.product_id, i.quantity as new_quantity;

-- MERGE statement
MERGE INTO target_table t
USING source_table s ON t.id = s.id
WHEN MATCHED AND s.updated_at > t.updated_at THEN
    UPDATE SET t.value = s.value, t.updated_at = s.updated_at
WHEN NOT MATCHED THEN
    INSERT (id, value, updated_at) VALUES (s.id, s.value, s.updated_at);
```

### PSQL Procedural Block
```sql
EXECUTE BLOCK
RETURNS (dept_name VARCHAR(100), emp_count INTEGER)
AS
DECLARE VARIABLE dept_id INTEGER;
BEGIN
    FOR SELECT id, name FROM departments INTO :dept_id, :dept_name DO
    BEGIN
        SELECT COUNT(*) FROM employees WHERE dept_id = :dept_id INTO :emp_count;
        SUSPEND;
    END
    WHEN ANY DO
    BEGIN
        dept_name = 'ERROR';
        emp_count = -1;
        SUSPEND;
    END
END
```

## Configuration and Tools

- **Configuration**: `include/scratchbird/engine/config.h`, `src/engine/config.cpp`
- **CLI Tools**: `src/dbcheck.cpp` (validation), `src/dbspace.cpp` (storage management)
- **Dev Tools**: `src/engine/psql_dev_tools.cpp` (formatter, validator, profiler)
- **Packaging**: `packaging/config/scratchbird.conf`, `packaging/systemd/scratchbird.service`

## Testing Infrastructure

The `tests/` directory contains comprehensive test suites that serve as executable documentation:
- `psql_*_tests.cpp`: Procedural SQL features
- `stored_procedure_tests.cpp`: Routine definitions and execution
- `explain_analyze_tests.cpp`: Query planning and analysis
- `alter_table_tests.cpp`: Table modification operations
- `fdw_*_tests.cpp`: Foreign data wrapper functionality
- `window_functions_tests.cpp`: Window function implementations
- `create_index_tests.cpp`: Index creation variations

## See also

- [Lexical Structure](./sql-lexical.md) - Token types, comments, string literals
- [Operators](./sql-operators.md) - Operator precedence and associativity
- [Reserved Words](./sql-reserved-words.md) - Keyword list and quoting rules
- [Data Types](./sql-data-types.md) - Complete type reference
- [SELECT Statements](./sql-select.md) - Query construction details
- [DML Operations](./sql-dml.md) - Data manipulation specifics
- [Session & Transaction](./session-and-transaction.md) - Connection and transaction management
- [PSQL Runtime](./psql-runtime.md) - Procedural SQL execution model