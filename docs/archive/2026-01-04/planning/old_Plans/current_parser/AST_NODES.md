# ScratchBird Parser - AST Node Reference

**Complete Audit of AST Node Definitions**
**Source:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h`
**Date:** 2025-12-06

This document provides a comprehensive catalog of all Abstract Syntax Tree (AST) node classes defined in the ScratchBird parser. This audit is used for comparative analysis and completeness verification.

---

## Table of Contents

1. [Base Classes](#base-classes)
2. [Enumerations](#enumerations)
3. [Expression Nodes](#expression-nodes)
4. [Statement Nodes](#statement-nodes)
5. [Supporting Structures](#supporting-structures)
6. [Visitor Pattern](#visitor-pattern)

---

## Base Classes

### ASTNode
**Purpose:** Base class for all AST nodes
**Fields:**
- `ASTKind kind_` - Type discriminator for the node
- `SourceSpan span_` - Source location information (start, end)

**Methods:**
- `kind()` - Returns the kind of node
- `span()` - Returns source location
- `accept(ASTVisitor*)` - Visitor pattern entry point (pure virtual)

### Statement
**Purpose:** Base class for all statement nodes
**Inherits:** ASTNode

### Expression
**Purpose:** Base class for all expression nodes
**Inherits:** ASTNode

---

## Enumerations

### ASTKind (enum class : uint8_t)
**Purpose:** Discriminator for AST node types

**Statement Types:**
- `CREATE_TABLE` - CREATE TABLE statement
- `CREATE_INDEX` - CREATE INDEX statement (Phase 2 Task 2.3)
- `DROP_TABLE` - DROP TABLE statement (ALPHA Phase 1)
- `DROP_INDEX` - DROP INDEX statement (ALPHA Phase 1)
- `TRUNCATE_TABLE` - TRUNCATE TABLE statement (ALPHA Phase 1)
- `ALTER_TABLE` - ALTER TABLE statement (ALPHA Phase 1)
- `CREATE_SEQUENCE` - CREATE SEQUENCE statement (ALPHA Phase 1)
- `ALTER_SEQUENCE` - ALTER SEQUENCE statement (ALPHA Phase 1)
- `DROP_SEQUENCE` - DROP SEQUENCE statement (ALPHA Phase 1)
- `CREATE_VIEW` - CREATE VIEW statement (ALPHA Phase 1)
- `DROP_VIEW` - DROP VIEW statement (ALPHA Phase 1)
- `REFRESH_MATERIALIZED_VIEW` - REFRESH MATERIALIZED VIEW (ALPHA Phase 1)
- `CREATE_TRIGGER` - CREATE TRIGGER statement (Phase 2 Wave 2)
- `DROP_TRIGGER` - DROP TRIGGER statement (Phase 2 Wave 2)
- `CREATE_DATABASE_TRIGGER` - Database-level triggers (ON CONNECT/DISCONNECT/TRANSACTION)
- `CREATE_FUNCTION` - CREATE FUNCTION statement (Phase 2 Task 10.2)
- `CREATE_PROCEDURE` - CREATE PROCEDURE statement (Phase 2 Task 10.2)
- `CREATE_TABLESPACE` - CREATE TABLESPACE statement (Phase 2 Task 2.1)
- `ALTER_TABLESPACE` - ALTER TABLESPACE statement (Phase 2 Task 2.2)
- `ALTER_TABLE_SET_TABLESPACE` - ALTER TABLE ... SET TABLESPACE (Phase 4)
- `DROP_TABLESPACE` - DROP TABLESPACE statement (Phase 2 Task 2.1)
- `ATTACH_TABLESPACE` - ATTACH TABLESPACE statement (Phase 6)
- `DETACH_TABLESPACE` - DETACH TABLESPACE statement (Phase 6)
- `INSERT` - INSERT statement
- `SELECT` - SELECT statement
- `SET_OPERATION` - UNION/INTERSECT/EXCEPT set operations
- `MERGE` - MERGE statement (Alpha 1)
- `UPDATE` - UPDATE statement (Phase 1 Task 2.1)
- `DELETE_STMT` - DELETE statement (Phase 1 Task 2.2)
- `ANALYZE` - ANALYZE statement (Phase 1 Task 1.1.2)
- `EXPLAIN` - EXPLAIN command (Phase 1 Task 1.5)
- `START_TRANSACTION` - START TRANSACTION statement (Phase 2 Task 2.6)
- `SET_TRANSACTION` - SET TRANSACTION statement (Phase 3 Task 3.6)
- `COMMIT` - COMMIT statement (Phase 2 Task 2.6)
- `ROLLBACK` - ROLLBACK statement (Phase 2 Task 2.6)
- `SAVEPOINT` - SAVEPOINT statement
- `RELEASE_SAVEPOINT` - RELEASE SAVEPOINT statement
- `ROLLBACK_TO_SAVEPOINT` - ROLLBACK TO SAVEPOINT statement
- `SWEEP` - SWEEP DATABASE statement (Phase 3 Task 3.3)
- `CREATE_TYPE` - CREATE TYPE for user-defined types
- `CREATE_DOMAIN` - CREATE DOMAIN for domain types
- `SHOW` - SHOW statement (ALPHA Phase 1)
- `DESCRIBE` - DESCRIBE statement (ALPHA Phase 1)

**Security Statements (ALPHA Phase 1 - Security System Phase 2):**
- `CREATE_USER` - CREATE USER statement
- `ALTER_USER` - ALTER USER statement
- `DROP_USER` - DROP USER statement
- `CREATE_ROLE` - CREATE ROLE statement
- `DROP_ROLE` - DROP ROLE statement
- `CREATE_GROUP` - CREATE GROUP statement
- `DROP_GROUP` - DROP GROUP statement
- `GRANT_PRIVILEGE` - GRANT privilege statement
- `REVOKE_PRIVILEGE` - REVOKE privilege statement
- `GRANT_ROLE` - GRANT role statement
- `REVOKE_ROLE` - REVOKE role statement
- `SET_ROLE` - SET ROLE statement
- `SET_SESSION_AUTH` - SET SESSION AUTHORIZATION statement
- `SET_CONSTRAINTS` - SET CONSTRAINTS statement (P2-7)
- `SET_SQL_DIALECT` - SET SQL DIALECT (Firebird ISQL compatibility)
- `SET_NAMES` - SET NAMES (connection character set)
- `SET_LOCAL_TIMEOUT` - SET LOCAL_TIMEOUT (statement timeout)
- `CREATE_POLICY` - CREATE POLICY statement (Security Phase 3.4)
- `DROP_POLICY` - DROP POLICY statement (Security Phase 3.4)
- `ALTER_TABLE_RLS` - ALTER TABLE ... ROW LEVEL SECURITY (Security Phase 3.4)

**Procedural Language Statements (Phase 2 Task 10.2):**
- `BLOCK` - BEGIN...END block
- `VAR_DECLARATION` - Variable declaration
- `ASSIGNMENT` - Variable assignment
- `IF_STMT` - IF statement
- `LOOP_STMT` - LOOP statement
- `WHILE_STMT` - WHILE loop
- `EXIT_STMT` - EXIT statement
- `RETURN_STMT` - RETURN statement
- `RAISE_STMT` - RAISE exception
- `TRY_EXCEPT` - TRY/EXCEPT block
- `CALL` - CALL procedure statement

**Expression Types:**
- `LITERAL` - Literal values
- `IDENTIFIER` - Identifier/column reference
- `BINARY_OP` - Binary operation
- `CAST` - CAST expression
- `FUNCTION_CALL` - Function call
- `AGGREGATE_FUNC` - Aggregate function (Phase 1 Task 4.1)
- `WINDOW_FUNC` - Window function (Phase 1 Task 6)
- `WINDOW_SPEC` - Window specification (Phase 1 Task 6)
- `JSON_FUNC` - JSON function (Phase 1 Task 7)
- `COALESCE` - COALESCE expression (Phase 1 Task 8)
- `NULLIF` - NULLIF expression (Phase 1 Task 8)
- `CASE` - CASE expression (Phase 1 Task 8)
- `GROUPING` - GROUPING() function (Phase 3)
- `SUBQUERY` - Subquery expression (Phase 2 Wave 2)
- `SEQUENCE_FUNCTION` - NEXTVAL/CURRVAL/SETVAL (ALPHA Phase 1)
- `EXTRACT` - EXTRACT(field FROM value) expression

**Other Types:**
- `TYPE_NAME` - Type name
- `COLUMN_DEF` - Column definition
- `TABLE_CONSTRAINT` - Table constraint
- `SELECT_LIST` - Select list
- `WHERE_CLAUSE` - Where clause

### BinaryOp (enum class : uint8_t)
**Purpose:** Binary operator types

**Arithmetic:**
- `ADD` - Addition (+)
- `SUBTRACT` - Subtraction (-)
- `MULTIPLY` - Multiplication (*)
- `DIVIDE` - Division (/)
- `MODULO` - Modulo (%)

**Comparison:**
- `EQ` - Equals (=)
- `NE` - Not equals (!=)
- `LT` - Less than (<)
- `GT` - Greater than (>)
- `LE` - Less than or equal (<=)
- `GE` - Greater than or equal (>=)

**Logical:**
- `AND` - Logical AND
- `OR` - Logical OR

**Pattern Matching:**
- `LIKE` - LIKE operator
- `ILIKE` - Case-insensitive LIKE
- `IN` - IN operator (Phase 2 Wave 2)
- `NOT_IN` - NOT IN operator (Phase 2 Wave 2)

**Array Operators (Phase 2 Task 12):**
- `ARRAY_OVERLAP` - Array overlap (&&)
- `ARRAY_CONTAINS` - Array contains (@>)
- `ARRAY_CONTAINED_BY` - Array contained by (<@)

**Regex Operators (Phase 2 Task 13):**
- `REGEX_MATCH` - Regex match (~)
- `REGEX_MATCH_CI` - Regex match case-insensitive (~*)
- `REGEX_NOT_MATCH` - Regex not match (!~)
- `REGEX_NOT_MATCH_CI` - Regex not match case-insensitive (!~*)

**Range Operators (Task 15 Phase 4):**
- `RANGE_STRICTLY_LEFT` - Strictly left of (<<)
- `RANGE_STRICTLY_RIGHT` - Strictly right of (>>)
- `RANGE_ADJACENT` - Adjacent (-|-)

### GeneratedColumnStorage (enum class : uint8_t)
**Purpose:** Generated column storage type (ALPHA Phase 1)

**Values:**
- `NOT_GENERATED` (0) - Regular column
- `STORED` (1) - GENERATED ALWAYS AS ... STORED
- `VIRTUAL` (2) - GENERATED ALWAYS AS ... VIRTUAL

### ShowObjectType (enum class : uint8_t)
**Purpose:** Types of objects for SHOW commands

**Basic SHOW Commands (Alpha 1):**
- `TABLES` - SHOW TABLES
- `DATABASES` - SHOW DATABASES / SHOW SCHEMAS
- `COLUMNS` - SHOW COLUMNS FROM table
- `INDEXES` - SHOW INDEXES FROM table
- `CREATE_TABLE` - SHOW CREATE TABLE table

**Extended SHOW Commands (Firebird ISQL compatibility):**
- `TABLE` - SHOW TABLE [name] - detailed table structure
- `INDEX` - SHOW INDEX [name] - detailed index info
- `TRIGGER` - SHOW TRIGGER [name] - trigger definitions
- `PROCEDURE` - SHOW PROCEDURE [name] - stored procedure definitions
- `FUNCTION` - SHOW FUNCTION [name] - user-defined functions
- `VIEW` - SHOW VIEW [name] - view definitions
- `DOMAIN` - SHOW DOMAIN [name] - domain definitions
- `GENERATOR` - SHOW GENERATOR [name] / SHOW SEQUENCE [name]
- `SCHEMA` - SHOW SCHEMA [name] - schema definitions
- `ROLE` - SHOW ROLE [name] - role definitions
- `GRANTS` - SHOW GRANTS [object] - object privileges
- `CHECKS` - SHOW CHECKS [table] - check constraints
- `COLLATIONS` - SHOW COLLATIONS - collation sequences
- `COMMENTS` - SHOW COMMENTS - object comments
- `DEPENDENCIES` - SHOW DEPENDENCIES obj - dependency graph
- `PACKAGE` - SHOW PACKAGE [name] - package definitions
- `SYSTEM` - SHOW SYSTEM - system tables/views
- `SQL_DIALECT` - SHOW SQL DIALECT - current SQL dialect
- `VERSION` - SHOW VERSION - server version info
- `DATABASE` - SHOW DATABASE - database metadata

**Schema Navigation Commands:**
- `SCHEMA_PATH` - SHOW SCHEMA PATH - full path to current schema
- `SCHEMA_TREE` - SHOW SCHEMA TREE [DEPTH n] - schema hierarchy
- `SEARCH_PATH` - SHOW SEARCH PATH - current search path
- `LOCATION` - SHOW LOCATION OF [type] name - find object in search path
- `RESOLVED` - SHOW RESOLVED name - which object search path resolves to
- `OBJECTS` - SHOW OBJECTS - all objects in current schema

### ShowSchemaScope (enum class : uint8_t)
**Purpose:** Schema scope for SHOW commands

**Values:**
- `CURRENT` - Show in current schema only (default)
- `IN_PATH` - Show in all schemas in search path
- `IN_SCHEMA` - Show in specific schema

### JoinType (enum class : uint8_t)
**Purpose:** JOIN types (Phase 1 Task 3.1)

**Values:**
- `INNER` - INNER JOIN
- `LEFT` - LEFT OUTER JOIN
- `RIGHT` - RIGHT OUTER JOIN
- `FULL` - FULL OUTER JOIN
- `CROSS` - CROSS JOIN

### JoinConditionType (enum class : uint8_t)
**Purpose:** JOIN condition type

**Values:**
- `ON` - JOIN ... ON condition
- `USING` - JOIN ... USING (columns)
- `NATURAL` - NATURAL JOIN
- `CROSS` - CROSS JOIN (no condition)

### SortOrder (enum class : uint8_t)
**Purpose:** Sort direction for ORDER BY (Phase 1 Task 5.1)

**Values:**
- `ASC` - Ascending
- `DESC` - Descending

### NullsOrder (enum class : uint8_t)
**Purpose:** NULL ordering for ORDER BY (Phase 1 Task 5.1)

**Values:**
- `DEFAULT` - Database default
- `NULLS_FIRST` - NULLs first
- `NULLS_LAST` - NULLs last

### GroupingType (enum class : uint8_t)
**Purpose:** Advanced GROUP BY features (Phase 3)

**Values:**
- `STANDARD` - Regular GROUP BY
- `ROLLUP` - GROUP BY ROLLUP(...)
- `CUBE` - GROUP BY CUBE(...)
- `GROUPING_SETS` - GROUP BY GROUPING SETS(...)

### SetOperationType (enum class : uint8_t)
**Purpose:** Set operation types

**Values:**
- `UNION` - UNION (removes duplicates)
- `UNION_ALL` - UNION ALL (keeps duplicates)
- `INTERSECT` - INTERSECT (removes duplicates)
- `INTERSECT_ALL` - INTERSECT ALL (keeps duplicates)
- `EXCEPT` - EXCEPT (removes duplicates)
- `EXCEPT_ALL` - EXCEPT ALL (keeps duplicates)

### OnConflictAction (enum class : uint8_t)
**Purpose:** ON CONFLICT action type for UPSERT

**Values:**
- `NONE` - No ON CONFLICT clause
- `DO_NOTHING` - ON CONFLICT DO NOTHING
- `DO_UPDATE` - ON CONFLICT DO UPDATE SET ...

### TransactionMode (enum class : uint8_t)
**Purpose:** Transaction mode flags (Phase 2 Task 2.6)

**Values:**
- `READ_WRITE` (0) - Read-write transaction
- `READ_ONLY` (1) - Read-only transaction

### IsolationLevel (enum class : uint8_t)
**Purpose:** Transaction isolation level

**Values:**
- `READ_COMMITTED` (0) - Read committed isolation
- `SNAPSHOT` (1) - Snapshot isolation
- `SNAPSHOT_TABLE_STABILITY` (2) - Snapshot with table stability

### TableLockMode (enum class : uint8_t)
**Purpose:** Table lock mode for RESERVING clause (Firebird-style)

**Values:**
- `SHARED` (0) - SHARED READ - allows concurrent reads
- `PROTECTED` (1) - PROTECTED READ/WRITE - exclusive table access

### UserTypeKind (enum class : uint8_t)
**Purpose:** User-defined type kind

**Values:**
- `COMPOSITE` - CREATE TYPE name AS (field1 type1, ...)
- `ENUM` - CREATE TYPE name AS ENUM ('value1', ...)
- `RANGE` - CREATE TYPE name AS RANGE (subtype = ...) - future

### AggregateFunc (enum class : uint8_t)
**Purpose:** Aggregate function types (Phase 1 Task 4.1, Phase 2 Task 12)

**Values:**
- `COUNT` - COUNT aggregate
- `SUM` - SUM aggregate
- `AVG` - AVG aggregate
- `MIN` - MIN aggregate
- `MAX` - MAX aggregate
- `ARRAY_AGG` - ARRAY_AGG (Phase 2 Task 12)
- `STRING_AGG` - STRING_AGG with delimiter (Phase 2)

### WindowFunc (enum class : uint8_t)
**Purpose:** Window function types (Phase 1 Task 6)

**Values:**
- `ROW_NUMBER` - ROW_NUMBER()
- `RANK` - RANK()
- `DENSE_RANK` - DENSE_RANK()
- `LAG` - LAG()
- `LEAD` - LEAD()
- `FIRST_VALUE` - FIRST_VALUE()
- `LAST_VALUE` - LAST_VALUE()
- `NTH_VALUE` - NTH_VALUE()
- `CUME_DIST` - CUME_DIST() (Alpha 1)
- `PERCENT_RANK` - PERCENT_RANK() (Alpha 1)
- `NTILE` - NTILE(n) - divide rows into n buckets

### FrameBoundaryType (enum class : uint8_t)
**Purpose:** Window frame boundary type (Phase 1 Task 6)

**Values:**
- `UNBOUNDED_PRECEDING` - UNBOUNDED PRECEDING
- `PRECEDING` - N PRECEDING
- `CURRENT_ROW` - CURRENT ROW
- `FOLLOWING` - N FOLLOWING
- `UNBOUNDED_FOLLOWING` - UNBOUNDED FOLLOWING

### FrameMode (enum class : uint8_t)
**Purpose:** Window frame mode (Phase 1 Task 6, P2-9)

**Values:**
- `ROWS` - Physical row-based frames
- `RANGE` - Value range-based frames
- `GROUPS` - Peer group-based frames (P2-9)

### JSONFunc (enum class : uint8_t)
**Purpose:** JSON function types (Phase 1 Task 7)

**Extraction Functions (Task 7.1):**
- `JSON_EXTRACT` - JSON_EXTRACT(json, path)
- `JSONB_EXTRACT_PATH` - jsonb_extract_path(jsonb, path_elem...)
- `ARROW` - json_col -> 'field' (returns JSON)
- `DOUBLE_ARROW` - json_col ->> 'field' (returns text)
- `HASH_ARROW` - json_col #> ARRAY['path'] (returns JSON)
- `HASH_DOUBLE_ARROW` - json_col #>> ARRAY['path'] (returns text)

**Construction Functions (Task 7.2):**
- `JSON_OBJECT` - JSON_OBJECT('key1', val1, ...)
- `JSON_ARRAY` - JSON_ARRAY(val1, val2, ...)
- `JSONB_BUILD_OBJECT` - jsonb_build_object('key1', val1, ...)
- `JSONB_BUILD_ARRAY` - jsonb_build_array(val1, val2, ...)

**Modification Functions (Task 7.3):**
- `JSON_SET` - JSON_SET(json, path, value)
- `JSON_INSERT` - JSON_INSERT(json, path, value)
- `JSON_REMOVE` - JSON_REMOVE(json, path)
- `JSONB_SET` - jsonb_set(jsonb, path_array, value)

### SubqueryType (enum class : uint8_t)
**Purpose:** Subquery expression types (Phase 2 Wave 2)

**Values:**
- `SCALAR` - Returns single value: (SELECT col FROM ...)
- `EXISTS` - Returns boolean: EXISTS (SELECT ...)
- `IN` - Membership test: col IN (SELECT ...)
- `NOT_IN` - Membership test: col NOT IN (SELECT ...)
- `ARRAY` - Returns array: ARRAY(SELECT ...)

### SequenceFunctionType (enum class : uint8_t)
**Purpose:** Sequence function types (ALPHA Phase 1)

**Values:**
- `NEXTVAL` - NEXTVAL(sequence_name)
- `CURRVAL` - CURRVAL(sequence_name)
- `SETVAL` - SETVAL(sequence_name, value, is_called)

### TriggerTiming (enum class : uint8_t)
**Purpose:** Trigger timing (Phase 2 Wave 2)

**Values:**
- `BEFORE` - BEFORE trigger
- `AFTER` - AFTER trigger

### TriggerEvent (enum class : uint8_t)
**Purpose:** Trigger event (Phase 2 Wave 2)

**Values:**
- `INSERT` - INSERT event
- `UPDATE` - UPDATE event
- `DELETE` - DELETE event

### TriggerGranularity (enum class : uint8_t)
**Purpose:** Trigger granularity (Phase 2 Wave 2)

**Values:**
- `FOR_EACH_ROW` - Row-level trigger
- `FOR_EACH_STATEMENT` - Statement-level trigger (P2-8)

### DatabaseTriggerEvent (enum class : uint8_t)
**Purpose:** Database trigger events (Firebird-style)

**Values:**
- `ON_CONNECT` - Fire when client connects
- `ON_DISCONNECT` - Fire when client disconnects
- `ON_TRANSACTION_START` - Fire when transaction starts
- `ON_TRANSACTION_COMMIT` - Fire when transaction commits
- `ON_TRANSACTION_ROLLBACK` - Fire when transaction rolls back

### ParameterMode (enum class : uint8_t)
**Purpose:** Parameter mode for functions/procedures

**Values:**
- `IN` - Input parameter
- `OUT` - Output parameter
- `INOUT` - Input/output parameter

### TablespaceAlterationType (enum class : uint8_t)
**Purpose:** Tablespace alteration types (Phase 2 Task 2.2)

**Values:**
- `SET_AUTOEXTEND` - AUTOEXTEND ON|OFF
- `SET_AUTOEXTEND_SIZE` - AUTOEXTEND_SIZE N
- `SET_MAXSIZE` - MAXSIZE N | UNLIMITED
- `RENAME_TO` - RENAME TO new_name

### GrantPrivilegeStmt::PrivilegeType (enum class : uint32_t)
**Purpose:** Privilege types (bitmask)

**Values:**
- `SELECT` (0x00000001)
- `INSERT` (0x00000002)
- `UPDATE` (0x00000004)
- `DELETE` (0x00000008)
- `TRUNCATE` (0x00000010)
- `REFERENCES` (0x00000020)
- `TRIGGER` (0x00000040)
- `CREATE` (0x00000080)
- `USAGE` (0x00000100)
- `EXECUTE` (0x00000800)
- `CONNECT` (0x00001000)
- `ALL` (0xFFFFFFFF)

### GrantPrivilegeStmt::ObjectType (enum class : uint8_t)
**Purpose:** Object types for GRANT/REVOKE

**Values:**
- `TABLE`
- `VIEW`
- `SEQUENCE`
- `FUNCTION`
- `PROCEDURE`
- `SCHEMA`
- `DATABASE`
- `DOMAIN`

### GrantPrivilegeStmt::GranteeType (enum class : uint8_t)
**Purpose:** Grantee types

**Values:**
- `USER`
- `ROLE`
- `GROUP`
- `PUBLIC`

### CreatePolicyStmt::PolicyCommand (enum class : uint8_t)
**Purpose:** Policy command types (Security Phase 3.4)

**Values:**
- `ALL` (0)
- `SELECT` (1)
- `INSERT` (2)
- `UPDATE` (3)
- `DELETE_CMD` (4)

### AlterTableRLSStmt::RLSAction (enum class : uint8_t)
**Purpose:** Row Level Security actions (Security Phase 3.4)

**Values:**
- `ENABLE` - ENABLE ROW LEVEL SECURITY
- `DISABLE` - DISABLE ROW LEVEL SECURITY
- `FORCE` - FORCE ROW LEVEL SECURITY
- `NO_FORCE` - NO FORCE ROW LEVEL SECURITY

### DropTableStmt::DropBehavior (enum class : uint8_t)
**Purpose:** Drop behavior for various DROP statements

**Values:**
- `RESTRICT` - Fail if dependencies exist (default)
- `CASCADE` - Drop dependent objects recursively

### RaiseStmt::Level (enum class : uint8_t)
**Purpose:** RAISE statement severity levels

**Values:**
- `EXCEPTION`
- `NOTICE`
- `WARNING`
- `INFO`
- `DEBUG`

### CreateFunctionStmt::SqlSecurity (enum class : uint8_t)
**Purpose:** SQL Security mode (Phase 3.1)

**Values:**
- `DEFINER` (0) - Execute with owner's privileges
- `INVOKER` (1) - Execute with caller's privileges (default)

### AlterTableStmt::AlterAction (enum class : uint8_t)
**Purpose:** ALTER TABLE actions (ALPHA Phase 1)

**Values:**
- `ADD_COLUMN`
- `DROP_COLUMN`
- `ALTER_COLUMN_TYPE`
- `ALTER_COLUMN_SET_DEFAULT`
- `ALTER_COLUMN_DROP_DEFAULT`
- `RENAME_COLUMN`
- `ADD_CONSTRAINT`
- `DROP_CONSTRAINT`

### TruncateTableStmt::TruncateMode (enum class : uint8_t)
**Purpose:** Truncate mode (ALPHA Phase 1)

**Values:**
- `ASYNC` (0) - Background job (default, non-blocking)
- `SYNC` (1) - Block until complete

---

## Expression Nodes

### LiteralExpr
**Purpose:** Literal value expression
**Inherits:** Expression
**ASTKind:** LITERAL

**Literal Types:**
- `INTEGER` - Integer literal
- `FLOAT` - Floating-point literal
- `STRING` - String literal
- `NULL_LITERAL` - NULL literal
- `RANGE` - Range literal like '[1,10)' (Task 15 Phase 4)

**Fields:**
- `LiteralType literal_type_` - Type of literal
- `union` - Value storage:
  - `int64_t int_value_` - For INTEGER
  - `double float_value_` - For FLOAT
  - `StringPool::StringId string_value_` - For STRING
  - `StringPool::StringId range_value_` - For RANGE

**Methods:**
- `literalType()` - Returns literal type
- `intValue()`, `floatValue()`, `stringValue()`, `rangeValue()` - Value accessors
- `setIntValue()`, `setFloatValue()`, `setStringValue()`, `setRangeValue()` - Value setters

### IdentifierExpr
**Purpose:** Identifier/column reference
**Inherits:** Expression
**ASTKind:** IDENTIFIER

**Fields:**
- `StringPool::StringId name_` - Column name
- `StringPool::StringId qualifier_` - Optional table name or alias (0 if not qualified)

**Methods:**
- `name()` - Returns column name
- `qualifier()` - Returns qualifier (table/alias)
- `isQualified()` - Returns true if qualified (table.column)

**Constructors:**
- Simple identifier: `IdentifierExpr(span, name)`
- Qualified identifier: `IdentifierExpr(span, qualifier, name)` (Phase 1 Task 3.1)

### BinaryOpExpr
**Purpose:** Binary operation
**Inherits:** Expression
**ASTKind:** BINARY_OP

**Fields:**
- `BinaryOp op_` - Operator type
- `Expression* left_` - Left operand
- `Expression* right_` - Right operand

**Methods:**
- `op()` - Returns operator
- `left()` - Returns left operand
- `right()` - Returns right operand

### CastExpr
**Purpose:** CAST or TRY_CAST expression
**Inherits:** Expression
**ASTKind:** CAST

**Fields:**
- `Expression* expr_` - Expression to cast
- `TypeName target_type_` - Target type
- `bool is_try_cast_` - True if TRY_CAST (returns NULL on error)

**Methods:**
- `expr()` - Returns expression to cast
- `targetType()` - Returns target type
- `isTryCast()` - Returns true if TRY_CAST

### FunctionCallExpr
**Purpose:** Function call expression
**Inherits:** Expression
**ASTKind:** FUNCTION_CALL

**Fields:**
- `StringPool::StringId name_` - Function name
- `std::vector<Expression*> args_` - Function arguments

**Methods:**
- `name()` - Returns function name
- `args()` - Returns arguments

### SequenceFunctionExpr
**Purpose:** Sequence function expression (NEXTVAL/CURRVAL/SETVAL)
**Inherits:** Expression
**ASTKind:** SEQUENCE_FUNCTION
**Phase:** ALPHA Phase 1 - Sequences

**Fields:**
- `SequenceFunctionType func_type_` - Function type
- `Expression* sequence_name_` - Sequence name expression
- `Expression* value_` - For SETVAL (new value)
- `Expression* is_called_` - For SETVAL (is_called flag)

**Methods:**
- `functionType()` - Returns function type
- `sequenceName()` - Returns sequence name
- `value()` - Returns value (for SETVAL)
- `isCalled()` - Returns is_called (for SETVAL)

### ExtractExpr
**Purpose:** EXTRACT(field FROM value) expression
**Inherits:** Expression
**ASTKind:** EXTRACT

**Fields:**
- `uint8_t field_id_` - ExtractField enum value
- `std::string field_name_` - Field name as string (for error messages)
- `Expression* source_` - Source expression to extract from

**Methods:**
- `fieldId()` - Returns field ID
- `fieldName()` - Returns field name
- `source()` - Returns source expression

**Purpose:** Extracts sub-information from complex data types (DATE, TIME, TIMESTAMP, INTERVAL, UUID, INET, POINT, ARRAY, RANGE, etc.)

**Examples:**
- `EXTRACT(year FROM date_col)`
- `EXTRACT(x FROM point_col)`
- `EXTRACT(version FROM uuid_col)`

### AggregateExpr
**Purpose:** Aggregate function expression
**Inherits:** Expression
**ASTKind:** AGGREGATE_FUNC
**Phase:** Phase 1 Task 4.1

**Fields:**
- `AggregateFunc func_` - Aggregate function type
- `Expression* arg_` - Argument (nullptr for COUNT(*))
- `bool distinct_` - True for COUNT(DISTINCT col)
- `Expression* separator_` - For STRING_AGG(expr, separator)
- `std::vector<Expression*> order_by_` - For STRING_AGG ... ORDER BY
- `std::vector<bool> order_ascending_` - Order directions
- `Expression* filter_` - FILTER (WHERE condition)

**Methods:**
- `func()` - Returns function type
- `arg()` - Returns argument
- `distinct()` - Returns distinct flag
- `separator()`, `setSeparator()` - Separator for STRING_AGG
- `orderBy()`, `addOrderBy()` - ORDER BY within aggregate
- `orderAscending()` - Order directions
- `hasOrderBy()` - Returns true if has ORDER BY
- `filter()`, `setFilter()` - FILTER clause
- `hasFilter()` - Returns true if has FILTER

### WindowFuncExpr
**Purpose:** Window function expression
**Inherits:** Expression
**ASTKind:** WINDOW_FUNC
**Phase:** Phase 1 Task 6

**Fields:**
- `WindowFunc func_` - Window function type
- `std::vector<Expression*> args_` - Function arguments
- `WindowSpec* window_spec_` - OVER clause specification

**Methods:**
- `func()` - Returns function type
- `args()` - Returns arguments
- `windowSpec()` - Returns window specification

### WindowSpec
**Purpose:** Window specification (OVER clause)
**Inherits:** ASTNode
**ASTKind:** WINDOW_SPEC
**Phase:** Phase 1 Task 6

**Fields:**
- `std::vector<Expression*> partition_by_` - PARTITION BY expressions
- `std::vector<Expression*> order_by_` - ORDER BY expressions
- `std::vector<bool> order_ascending_` - Order directions
- `std::vector<bool> order_nulls_first_` - NULLS FIRST/LAST
- `bool has_frame_` - True if frame clause specified
- `FrameMode frame_mode_` - Frame mode (ROWS/RANGE/GROUPS)
- `FrameBoundary frame_start_` - Frame start boundary
- `FrameBoundary frame_end_` - Frame end boundary

**Methods:**
- `addPartitionBy()`, `partitionBy()` - PARTITION BY clause
- `addOrderBy()`, `orderBy()`, `orderAscending()`, `orderNullsFirst()` - ORDER BY clause
- `setFrame()`, `hasFrame()`, `frameMode()`, `frameStart()`, `frameEnd()` - Frame clause

### JSONFuncExpr
**Purpose:** JSON function expression
**Inherits:** Expression
**ASTKind:** JSON_FUNC
**Phase:** Phase 1 Task 7

**Fields:**
- `JSONFunc func_` - JSON function type
- `std::vector<Expression*> args_` - Function arguments

**Methods:**
- `func()` - Returns function type
- `args()` - Returns arguments

### CoalesceExpr
**Purpose:** COALESCE expression
**Inherits:** Expression
**ASTKind:** COALESCE
**Phase:** Phase 1 Task 8

**Fields:**
- `std::vector<Expression*> args_` - Arguments

**Methods:**
- `args()` - Returns arguments

### NullIfExpr
**Purpose:** NULLIF expression
**Inherits:** Expression
**ASTKind:** NULLIF
**Phase:** Phase 1 Task 8

**Fields:**
- `Expression* expr1_` - First expression
- `Expression* expr2_` - Second expression

**Methods:**
- `expr1()` - Returns first expression
- `expr2()` - Returns second expression

### CaseExpr
**Purpose:** CASE expression (searched or simple)
**Inherits:** Expression
**ASTKind:** CASE
**Phase:** Phase 1 Task 8

**Sub-structure:**
- `WhenClause` - WHEN condition THEN result pair
  - `Expression* condition` - WHEN condition
  - `Expression* result` - THEN result

**Fields:**
- `Expression* case_operand_` - NULL for searched CASE, value for simple CASE
- `std::vector<WhenClause> when_clauses_` - WHEN clauses
- `Expression* else_result_` - ELSE result (can be NULL)

**Methods:**
- `isSimpleCase()` - Returns true if simple CASE
- `caseOperand()` - Returns case operand (simple CASE)
- `whenClauses()` - Returns WHEN clauses
- `elseResult()` - Returns ELSE result

**Constructors:**
- Searched CASE: `CaseExpr(span, when_clauses, else_result)`
- Simple CASE: `CaseExpr(span, case_operand, when_clauses, else_result)`

### GroupingExpr
**Purpose:** GROUPING() function for ROLLUP/CUBE/GROUPING SETS
**Inherits:** Expression
**ASTKind:** GROUPING
**Phase:** Phase 3 - Advanced Grouping

**Fields:**
- `Expression* arg_` - The grouping column expression to check

**Methods:**
- `arg()` - Returns argument

### ArrayLiteral
**Purpose:** Array literal expression: ARRAY[elem1, elem2, ...]
**Inherits:** Expression
**ASTKind:** LITERAL
**Phase:** Phase 2 Task 12

**Fields:**
- `std::vector<Expression*> elements_` - Array elements

**Methods:**
- `elements()` - Returns array elements

### SubqueryExpr
**Purpose:** Subquery expression
**Inherits:** Expression
**ASTKind:** SUBQUERY
**Phase:** Phase 2 Wave 2 - Agent B

**Fields:**
- `SelectStmt* query_` - SELECT statement
- `SubqueryType type_` - Subquery type (SCALAR/EXISTS/IN/NOT_IN/ARRAY)

**Methods:**
- `query()` - Returns SELECT statement
- `type()` - Returns subquery type

---

## Statement Nodes

### DDL Statements

#### CreateTableStmt
**Purpose:** CREATE TABLE statement
**Inherits:** Statement
**ASTKind:** CREATE_TABLE

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `std::vector<ColumnDef*> columns_` - Column definitions
- `StringPool::StringId charset_` - DEFAULT CHARACTER SET clause
- `StringPool::StringId collation_` - DEFAULT COLLATE clause
- `StringPool::StringId tablespace_` - TABLESPACE clause (Phase 2 Task 2.3)
- `std::vector<TableConstraint*> table_constraints_` - Table-level constraints (Phase C)

**Methods:**
- `tableName()` - Returns table name
- `columns()` - Returns column definitions
- `charset()` - Returns character set
- `collation()` - Returns collation
- `tablespace()` - Returns tablespace (Phase 2 Task 2.3)
- `tableConstraints()` - Returns table constraints (Phase C)

#### CreateIndexStmt
**Purpose:** CREATE INDEX statement
**Inherits:** Statement
**ASTKind:** CREATE_INDEX
**Phase:** Phase 2 Task 2.3, Task 17

**Sub-structure:**
- `IndexColumn` - Index column (simple or expression)
  - `StringPool::StringId column_name` - For simple column
  - `Expression* expression` - For expression index
  - `bool is_expression` - True if expression

**Fields:**
- `StringPool::StringId index_name_` - Index name
- `StringPool::StringId table_name_` - Table name
- `std::vector<IndexColumn> index_columns_` - Columns or expressions (Task 17)
- `Expression* where_clause_` - WHERE clause for partial indexes (Task 17)
- `bool is_unique_` - UNIQUE flag
- `StringPool::StringId tablespace_` - Tablespace
- `StringPool::StringId index_type_` - Index type (e.g., "LSM", "BTREE")

**Methods:**
- `indexName()` - Returns index name
- `tableName()` - Returns table name
- `columns()` - Returns column names (legacy, backward compatible)
- `indexColumns()` - Returns index columns (Task 17)
- `whereClause()` - Returns WHERE clause (Task 17)
- `hasWhereClause()` - Returns true if has WHERE
- `hasExpressions()` - Returns true if has expression indexes
- `isUnique()` - Returns UNIQUE flag
- `tablespace()` - Returns tablespace
- `indexType()` - Returns index type
- `hasIndexType()` - Returns true if has index type

**Constructors:**
- Legacy: `CreateIndexStmt(span, index_name, table_name, columns, is_unique, tablespace, index_type)`
- Task 17: `CreateIndexStmt(span, index_name, table_name, index_columns, where_clause, is_unique, tablespace, index_type)`

#### DropTableStmt
**Purpose:** DROP TABLE statement
**Inherits:** Statement
**ASTKind:** DROP_TABLE
**Phase:** ALPHA Phase 1 - DDL Modifications

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `bool if_exists_` - IF EXISTS clause
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

**Methods:**
- `tableName()` - Returns table name
- `ifExists()` - Returns IF EXISTS flag
- `dropBehavior()` - Returns drop behavior

#### DropIndexStmt
**Purpose:** DROP INDEX statement
**Inherits:** Statement
**ASTKind:** DROP_INDEX
**Phase:** ALPHA Phase 1 - DDL Modifications

**Fields:**
- `StringPool::StringId index_name_` - Index name
- `bool if_exists_` - IF EXISTS clause
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

**Methods:**
- `indexName()` - Returns index name
- `ifExists()` - Returns IF EXISTS flag
- `dropBehavior()` - Returns drop behavior

#### TruncateTableStmt
**Purpose:** TRUNCATE TABLE statement
**Inherits:** Statement
**ASTKind:** TRUNCATE_TABLE
**Phase:** ALPHA Phase 1 - DDL Modifications

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `TruncateMode mode_` - ASYNC or SYNC mode

**Methods:**
- `tableName()` - Returns table name
- `mode()` - Returns truncate mode

#### AlterTableStmt
**Purpose:** ALTER TABLE statement
**Inherits:** Statement
**ASTKind:** ALTER_TABLE
**Phase:** ALPHA Phase 1 - DDL Modifications

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `AlterAction action_` - Alteration action type
- `ColumnDef* column_def_` - ADD COLUMN data
- `TypeName* new_type_` - ALTER COLUMN TYPE data
- `Expression* default_expr_` - ALTER COLUMN DEFAULT data
- `StringPool::StringId old_column_name_` - Column names
- `StringPool::StringId new_column_name_` - Column names
- `StringPool::StringId constraint_name_` - Constraint name
- `bool if_exists_` - DROP modifiers
- `DropBehavior drop_behavior_` - DROP modifiers

**Methods:**
- `tableName()` - Returns table name
- `action()` - Returns action type
- `setColumnDef()`, `columnDef()` - ADD COLUMN accessors
- `setDropColumnName()`, `dropColumnName()` - DROP COLUMN accessors
- `setAlterColumnType()`, `newType()` - ALTER COLUMN TYPE accessors
- `setAlterColumnDefault()`, `defaultExpr()` - ALTER COLUMN DEFAULT accessors
- `setRenameColumn()`, `oldColumnName()`, `newColumnName()` - RENAME COLUMN accessors
- `setConstraintName()`, `constraintName()` - ADD/DROP CONSTRAINT accessors
- `ifExists()`, `dropBehavior()` - DROP modifiers

#### CreateSequenceStmt
**Purpose:** CREATE SEQUENCE statement
**Inherits:** Statement
**ASTKind:** CREATE_SEQUENCE
**Phase:** ALPHA Phase 1 - Sequences

**Fields:**
- `StringPool::StringId name_` - Sequence name
- `Expression* increment_by_` - INCREMENT BY value
- `Expression* min_value_` - MINVALUE
- `Expression* max_value_` - MAXVALUE
- `Expression* start_with_` - START WITH value
- `Expression* cache_` - CACHE value
- `bool cycle_` - CYCLE flag
- `bool no_min_value_` - NO MINVALUE flag
- `bool no_max_value_` - NO MAXVALUE flag

**Methods:**
- `name()` - Returns sequence name
- `setIncrementBy()`, `incrementBy()` - INCREMENT BY
- `setMinValue()`, `minValue()` - MINVALUE
- `setMaxValue()`, `maxValue()` - MAXVALUE
- `setStartWith()`, `startWith()` - START WITH
- `setCache()`, `cache()` - CACHE
- `setCycle()`, `cycle()` - CYCLE
- `setNoMinValue()`, `noMinValue()` - NO MINVALUE
- `setNoMaxValue()`, `noMaxValue()` - NO MAXVALUE

#### AlterSequenceStmt
**Purpose:** ALTER SEQUENCE statement
**Inherits:** Statement
**ASTKind:** ALTER_SEQUENCE
**Phase:** ALPHA Phase 1 - Sequences

**Fields:**
- `StringPool::StringId name_` - Sequence name
- `Expression* increment_by_` - INCREMENT BY value
- `Expression* min_value_` - MINVALUE
- `Expression* max_value_` - MAXVALUE
- `Expression* restart_` - RESTART value
- `Expression* cache_` - CACHE value
- `bool has_cycle_` - True if CYCLE specified
- `bool cycle_` - CYCLE flag
- `bool no_min_value_` - NO MINVALUE flag
- `bool no_max_value_` - NO MAXVALUE flag

**Methods:**
- Same as CreateSequenceStmt, plus:
- `setRestart()`, `restart()` - RESTART
- `hasCycle()` - Returns true if CYCLE specified

#### DropSequenceStmt
**Purpose:** DROP SEQUENCE statement
**Inherits:** Statement
**ASTKind:** DROP_SEQUENCE
**Phase:** ALPHA Phase 1 - Sequences

**Fields:**
- `StringPool::StringId name_` - Sequence name
- `bool if_exists_` - IF EXISTS flag
- `bool cascade_` - CASCADE flag

**Methods:**
- `name()` - Returns sequence name
- `ifExists()` - Returns IF EXISTS flag
- `cascade()` - Returns CASCADE flag

#### CreateViewStmt
**Purpose:** CREATE VIEW statement
**Inherits:** Statement
**ASTKind:** CREATE_VIEW
**Phase:** ALPHA Phase 1 - Views

**Fields:**
- `StringPool::StringId name_` - View name
- `SelectStmt* query_` - SELECT query
- `bool or_replace_` - OR REPLACE flag
- `bool check_option_` - WITH CHECK OPTION flag
- `bool materialized_` - Materialized view flag (ALPHA Phase 1)
- `std::vector<StringPool::StringId> column_names_` - Optional column names
- `std::string query_definition_text_` - Actual SELECT text (ALPHA Phase 1)

**Methods:**
- `name()` - Returns view name
- `query()` - Returns SELECT query
- `orReplace()` - Returns OR REPLACE flag
- `checkOption()` - Returns WITH CHECK OPTION flag
- `materialized()` - Returns materialized flag
- `columnNames()` - Returns column names
- `setCheckOption()` - Sets check option
- `setColumnNames()` - Sets column names
- `queryDefinitionText()`, `setQueryDefinitionText()` - Query text storage

#### DropViewStmt
**Purpose:** DROP VIEW statement
**Inherits:** Statement
**ASTKind:** DROP_VIEW
**Phase:** ALPHA Phase 1 - Views

**Fields:**
- `StringPool::StringId name_` - View name
- `bool if_exists_` - IF EXISTS flag
- `bool cascade_` - CASCADE flag

**Methods:**
- `name()` - Returns view name
- `ifExists()` - Returns IF EXISTS flag
- `cascade()` - Returns CASCADE flag

#### RefreshMaterializedViewStmt
**Purpose:** REFRESH MATERIALIZED VIEW statement
**Inherits:** Statement
**ASTKind:** REFRESH_MATERIALIZED_VIEW
**Phase:** ALPHA Phase 1 - Materialized Views

**Fields:**
- `StringPool::StringId name_` - View name
- `bool concurrently_` - CONCURRENTLY option for non-blocking refresh

**Methods:**
- `name()` - Returns view name
- `concurrently()` - Returns CONCURRENTLY flag

#### CreateTablespaceStmt
**Purpose:** CREATE TABLESPACE statement
**Inherits:** Statement
**ASTKind:** CREATE_TABLESPACE
**Phase:** Phase 2 Task 2.1

**Fields:**
- `StringPool::StringId tablespace_name_` - Tablespace name
- `StringPool::StringId location_` - File location
- `bool autoextend_enabled_` - AUTOEXTEND flag
- `uint32_t autoextend_size_mb_` - AUTOEXTEND_SIZE parameter
- `uint32_t max_size_mb_` - MAXSIZE parameter (0 = UNLIMITED)
- `uint32_t prealloc_pages_` - PREALLOC parameter

**Methods:**
- `tablespaceName()` - Returns tablespace name
- `location()` - Returns file location
- `autoextendEnabled()` - Returns AUTOEXTEND flag
- `autoextendSizeMB()` - Returns AUTOEXTEND_SIZE
- `maxSizeMB()` - Returns MAXSIZE
- `preallocPages()` - Returns PREALLOC

#### AlterTablespaceStmt
**Purpose:** ALTER TABLESPACE statement
**Inherits:** Statement
**ASTKind:** ALTER_TABLESPACE
**Phase:** Phase 2 Task 2.2

**Fields:**
- `StringPool::StringId tablespace_name_` - Tablespace name
- `std::vector<TablespaceAlteration> alterations_` - List of alterations

**Methods:**
- `tablespaceName()` - Returns tablespace name
- `addAlteration()` - Adds alteration
- `alterations()` - Returns alterations

#### AlterTableSetTablespaceStmt
**Purpose:** ALTER TABLE ... SET TABLESPACE statement
**Inherits:** Statement
**ASTKind:** ALTER_TABLE_SET_TABLESPACE
**Phase:** Phase 4 Task 4.1.1

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `StringPool::StringId tablespace_name_` - Tablespace name
- `bool online_` - ONLINE clause

**Methods:**
- `tableName()` - Returns table name
- `tablespaceName()` - Returns tablespace name
- `online()` - Returns ONLINE flag

#### DropTablespaceStmt
**Purpose:** DROP TABLESPACE statement
**Inherits:** Statement
**ASTKind:** DROP_TABLESPACE
**Phase:** Phase 2 Task 2.1

**Fields:**
- `StringPool::StringId tablespace_name_` - Tablespace name
- `bool force_` - FORCE clause

**Methods:**
- `tablespaceName()` - Returns tablespace name
- `force()` - Returns FORCE flag

#### AttachTablespaceStmt
**Purpose:** ATTACH TABLESPACE statement
**Inherits:** Statement
**ASTKind:** ATTACH_TABLESPACE
**Phase:** Phase 6 Task 6.1

**Fields:**
- `StringPool::StringId file_path_` - Path to .sbts file
- `StringPool::StringId tablespace_name_` - Optional AS name

**Methods:**
- `filePath()` - Returns file path
- `tablespaceName()` - Returns tablespace name

#### DetachTablespaceStmt
**Purpose:** DETACH TABLESPACE statement
**Inherits:** Statement
**ASTKind:** DETACH_TABLESPACE
**Phase:** Phase 6 Task 6.2

**Fields:**
- `StringPool::StringId tablespace_name_` - Tablespace name
- `bool force_` - FORCE clause

**Methods:**
- `tablespaceName()` - Returns tablespace name
- `force()` - Returns FORCE flag

#### CreateTypeStmt
**Purpose:** CREATE TYPE for user-defined types
**Inherits:** Statement
**ASTKind:** CREATE_TYPE

**Fields:**
- `StringPool::StringId name_` - Type name
- `UserTypeKind type_kind_` - Type kind (COMPOSITE/ENUM/RANGE)
- `std::vector<StringPool::StringId> field_names_` - COMPOSITE field names
- `std::vector<TypeInfo> field_types_` - COMPOSITE field types
- `std::vector<StringPool::StringId> enum_values_` - ENUM values

**Methods:**
- `name()` - Returns type name
- `typeKind()` - Returns type kind
- `addField()`, `fieldNames()`, `fieldTypes()` - COMPOSITE type methods
- `addEnumValue()`, `enumValues()` - ENUM type methods

#### CreateDomainStmt
**Purpose:** CREATE DOMAIN for domain types
**Inherits:** Statement
**ASTKind:** CREATE_DOMAIN

**Fields:**
- `StringPool::StringId name_` - Domain name
- `TypeInfo base_type_` - Base type
- `Expression* default_value_` - DEFAULT clause
- `Expression* check_expr_` - CHECK constraint
- `bool not_null_` - NOT NULL constraint

**Methods:**
- `name()` - Returns domain name
- `baseType()` - Returns base type
- `setDefault()`, `defaultValue()` - DEFAULT clause
- `setCheck()`, `checkExpr()` - CHECK constraint
- `setNotNull()`, `isNotNull()` - NOT NULL constraint

### DML Statements

#### InsertStmt
**Purpose:** INSERT statement with UPSERT and multi-row support
**Inherits:** Statement
**ASTKind:** INSERT

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `std::vector<StringPool::StringId> columns_` - Column names
- `std::vector<std::vector<Expression*>> value_rows_` - Multi-row values
- `bool has_returning_` - RETURNING clause flag
- `std::vector<StringPool::StringId> returning_columns_` - RETURNING columns
- `OnConflictClause on_conflict_` - ON CONFLICT clause (UPSERT)

**Methods:**
- `tableName()` - Returns table name
- `columns()` - Returns column names
- `valueRows()` - Returns all value rows
- `rowCount()` - Returns number of rows
- `values()` - Returns first row (backward compatible)
- `hasReturning()` - Returns RETURNING flag
- `returningColumns()` - Returns RETURNING columns
- `setOnConflict()`, `onConflict()`, `hasOnConflict()` - UPSERT support

**Constructors:**
- Single-row: `InsertStmt(span, table_name, columns, values, has_returning, returning_columns)`
- Multi-row: `InsertStmt(span, table_name, columns, value_rows, has_returning, returning_columns)`

#### SelectStmt
**Purpose:** SELECT statement
**Inherits:** Statement
**ASTKind:** SELECT

**Fields:**
- `std::vector<SelectItem> select_list_` - SELECT list
- `FromClause from_clause_` - FROM clause with JOINs
- `Expression* where_clause_` - WHERE clause
- `bool has_joins_` - True if has JOINs
- `WithClause* with_clause_` - WITH clause (Phase 2 Wave 2)
- `bool distinct_` - SELECT DISTINCT flag (Alpha 1)
- `GroupByClause group_by_clause_` - GROUP BY clause (Phase 1 Task 4.1)
- `std::vector<OrderByItem> order_by_clause_` - ORDER BY clause (Phase 1 Task 5.1)
- `int64_t limit_count_` - LIMIT count (-1 = no limit) (Phase 1 Task 5.2)
- `int64_t offset_count_` - OFFSET count (-1 = no offset) (Phase 1 Task 5.2)

**Methods:**
- `selectList()` - Returns SELECT list
- `tableName()` - Returns table name (legacy, backward compatible)
- `fromClause()` - Returns FROM clause
- `hasJoins()` - Returns true if has JOINs
- `whereClause()`, `setWhereClause()` - WHERE clause
- `withClause()`, `setWithClause()` - WITH clause (Phase 2 Wave 2)
- `isDistinct()`, `setDistinct()` - DISTINCT flag (Alpha 1)
- `groupByClause()`, `setGroupByClause()` - GROUP BY clause (Phase 1 Task 4.1)
- `orderByClause()`, `setOrderByClause()` - ORDER BY clause (Phase 1 Task 5.1)
- `hasLimit()`, `limitCount()`, `setLimitCount()` - LIMIT clause (Phase 1 Task 5.2)
- `hasOffset()`, `offsetCount()`, `setOffsetCount()` - OFFSET clause (Phase 1 Task 5.2)

**Constructors:**
- Legacy: `SelectStmt(span, select_list, table_name, where_clause)`
- With JOINs: `SelectStmt(span, select_list, from_clause, where_clause)` (Phase 1 Task 3.1)
- With CTEs: `SelectStmt(span, with_clause, select_list, from_clause, where_clause)` (Phase 2 Wave 2)

#### SetOperationStmt
**Purpose:** Set operation statement (UNION/INTERSECT/EXCEPT)
**Inherits:** Statement
**ASTKind:** SET_OPERATION

**Fields:**
- `SetOperationType op_type_` - Operation type
- `Statement* left_` - Left SELECT or SetOperationStmt
- `Statement* right_` - Right SELECT or SetOperationStmt
- `std::vector<OrderByItem> order_by_clause_` - ORDER BY clause
- `int64_t limit_count_` - LIMIT count
- `int64_t offset_count_` - OFFSET count

**Methods:**
- `opType()` - Returns operation type
- `left()` - Returns left statement
- `right()` - Returns right statement
- `orderByClause()`, `setOrderByClause()` - ORDER BY clause
- `hasLimit()`, `limitCount()`, `setLimitCount()` - LIMIT clause
- `hasOffset()`, `offsetCount()`, `setOffsetCount()` - OFFSET clause

#### UpdateStmt
**Purpose:** UPDATE statement
**Inherits:** Statement
**ASTKind:** UPDATE
**Phase:** Phase 1 Task 2.1

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `std::vector<Assignment> assignments_` - SET clause assignments
- `Expression* where_clause_` - WHERE clause
- `bool has_returning_` - RETURNING clause flag
- `std::vector<StringPool::StringId> returning_columns_` - RETURNING columns

**Methods:**
- `tableName()` - Returns table name
- `assignments()` - Returns assignments
- `whereClause()` - Returns WHERE clause
- `hasReturning()` - Returns RETURNING flag
- `returningColumns()` - Returns RETURNING columns

#### DeleteStmt
**Purpose:** DELETE statement
**Inherits:** Statement
**ASTKind:** DELETE_STMT
**Phase:** Phase 1 Task 2.2

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `Expression* where_clause_` - WHERE clause
- `bool has_returning_` - RETURNING clause flag
- `std::vector<StringPool::StringId> returning_columns_` - RETURNING columns

**Methods:**
- `tableName()` - Returns table name
- `whereClause()` - Returns WHERE clause
- `hasReturning()` - Returns RETURNING flag
- `returningColumns()` - Returns RETURNING columns

#### MergeStmt
**Purpose:** MERGE statement
**Inherits:** Statement
**ASTKind:** MERGE
**Phase:** Alpha 1 - Advanced SQL

**Sub-structure:**
- `WhenClause` - WHEN clause
  - `Type type` - MATCHED/NOT_MATCHED/NOT_MATCHED_BY_SOURCE
  - `Expression* condition` - Optional additional condition
  - `std::vector<Assignment> assignments` - For UPDATE
  - `std::vector<StringPool::StringId> insert_columns` - For INSERT
  - `std::vector<Expression*> insert_values` - For INSERT

**Fields:**
- `StringPool::StringId target_table_` - Target table
- `Expression* source_` - Source (table or subquery)
- `Expression* on_condition_` - ON condition
- `std::vector<WhenClause> when_clauses_` - WHEN clauses

**Methods:**
- `targetTable()` - Returns target table
- `source()` - Returns source
- `onCondition()` - Returns ON condition
- `whenClauses()` - Returns WHEN clauses

### Transaction Statements

#### StartTransactionStmt
**Purpose:** START TRANSACTION statement
**Inherits:** Statement
**ASTKind:** START_TRANSACTION
**Phase:** Phase 2 Task 2.6, Phase 3 Task 3.6

**Fields:**
- `TransactionMode mode_` - READ_WRITE or READ_ONLY
- `IsolationLevel isolation_` - Isolation level
- `bool wait_` - Wait flag
- `bool commit_outstanding_` - Commit outstanding flag
- `uint32_t lock_timeout_` - Lock timeout in seconds
- `std::vector<TableReservation> table_reservations_` - RESERVING clause tables

**Methods:**
- `mode()` - Returns transaction mode
- `isolation()` - Returns isolation level
- `wait()` - Returns wait flag
- `commitOutstanding()` - Returns commit outstanding flag
- `lockTimeout()` - Returns lock timeout
- `tableReservations()` - Returns table reservations

#### SetTransactionStmt
**Purpose:** SET TRANSACTION statement
**Inherits:** Statement
**ASTKind:** SET_TRANSACTION
**Phase:** Phase 3 Task 3.6

**Fields:**
- `TransactionMode mode_` - Transaction mode
- `IsolationLevel isolation_` - Isolation level
- `bool wait_` - Wait flag
- `uint32_t lock_timeout_` - Lock timeout in seconds
- `std::vector<TableReservation> table_reservations_` - RESERVING clause tables

**Methods:**
- `mode()` - Returns transaction mode
- `isolation()` - Returns isolation level
- `wait()` - Returns wait flag
- `lockTimeout()` - Returns lock timeout
- `tableReservations()` - Returns table reservations

#### CommitStmt
**Purpose:** COMMIT statement
**Inherits:** Statement
**ASTKind:** COMMIT
**Phase:** Phase 2 Task 2.6

**Fields:** None

**Methods:** None (simple statement)

#### RollbackStmt
**Purpose:** ROLLBACK statement
**Inherits:** Statement
**ASTKind:** ROLLBACK
**Phase:** Phase 2 Task 2.6

**Fields:** None

**Methods:** None (simple statement)

#### SavepointStmt
**Purpose:** SAVEPOINT statement
**Inherits:** Statement
**ASTKind:** SAVEPOINT

**Fields:**
- `StringPool::StringId name_` - Savepoint name

**Methods:**
- `name()` - Returns savepoint name

#### ReleaseSavepointStmt
**Purpose:** RELEASE SAVEPOINT statement
**Inherits:** Statement
**ASTKind:** RELEASE_SAVEPOINT

**Fields:**
- `StringPool::StringId name_` - Savepoint name

**Methods:**
- `name()` - Returns savepoint name

#### RollbackToSavepointStmt
**Purpose:** ROLLBACK TO SAVEPOINT statement
**Inherits:** Statement
**ASTKind:** ROLLBACK_TO_SAVEPOINT

**Fields:**
- `StringPool::StringId name_` - Savepoint name

**Methods:**
- `name()` - Returns savepoint name

#### SweepStmt
**Purpose:** SWEEP DATABASE statement
**Inherits:** Statement
**ASTKind:** SWEEP
**Phase:** Phase 3 Task 3.3

**Fields:** None

**Methods:** None (simple statement)

### Utility Statements

#### AnalyzeStmt
**Purpose:** ANALYZE statement for statistics collection
**Inherits:** Statement
**ASTKind:** ANALYZE
**Phase:** Phase 1 Task 1.1.2

**Fields:**
- `StringPool::StringId table_name_` - Table to analyze
- `StringPool::StringId column_name_` - Specific column (or 0 for all columns)
- `float sample_rate_` - Sample rate (0.0 = auto, 0.0-1.0 = explicit)

**Methods:**
- `tableName()` - Returns table name
- `columnName()` - Returns column name
- `sampleRate()` - Returns sample rate
- `analyzeAllColumns()` - Returns true if analyzing all columns

**Syntax:** `ANALYZE table_name [COLUMN column_name] [SAMPLE sample_rate]`

#### ExplainStmt
**Purpose:** EXPLAIN command
**Inherits:** Statement
**ASTKind:** EXPLAIN
**Phase:** Phase 1 Task 1.5

**Fields:**
- `Statement* query_` - The statement to explain

**Methods:**
- `query()` - Returns query to explain

**Purpose:** Shows the execution plan for a query without executing it

**Example:**
```sql
EXPLAIN SELECT * FROM users WHERE id > 10;
```

#### ShowStmt
**Purpose:** SHOW statement for database introspection
**Inherits:** Statement
**ASTKind:** SHOW
**Phase:** ALPHA Phase 1 - Developer Experience + Firebird ISQL compatibility

**Fields:**
- `ShowObjectType object_type_` - Type of object to show
- `StringPool::StringId object_name_` - Object name
- `StringPool::StringId database_name_` - Database name
- `StringPool::StringId like_pattern_` - LIKE pattern for filtering
- `ShowSchemaScope schema_scope_` - CURRENT, IN_PATH, or IN_SCHEMA
- `StringPool::StringId schema_path_` - Schema path for IN_SCHEMA scope
- `bool in_detail_` - Show extended details
- `uint32_t tree_depth_` - Depth limit for SCHEMA_TREE

**Methods:**
- `objectType()` - Returns object type
- `objectName()` - Returns object name
- `tableName()` - Returns table name (backward compatible)
- `databaseName()` - Returns database name
- `likePattern()` - Returns LIKE pattern
- `schemaScope()` - Returns schema scope
- `schemaPath()` - Returns schema path
- `inDetail()` - Returns detail flag
- `treeDepth()` - Returns tree depth

**Constructors:**
- Basic: `ShowStmt(span, object_type, object_name, database_name, like_pattern)`
- Full: `ShowStmt(span, object_type, object_name, database_name, like_pattern, schema_scope, schema_path, in_detail, tree_depth)`

#### DescribeStmt
**Purpose:** DESCRIBE statement (alias for SHOW COLUMNS FROM table)
**Inherits:** Statement
**ASTKind:** DESCRIBE
**Phase:** ALPHA Phase 1 - Developer Experience

**Fields:**
- `StringPool::StringId table_name_` - Table name to describe

**Methods:**
- `tableName()` - Returns table name

### Security Statements

#### CreateUserStmt
**Purpose:** CREATE USER statement
**Inherits:** Statement
**ASTKind:** CREATE_USER
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId username_` - Username
- `StringPool::StringId password_` - Password (0 if no password)
- `bool has_password_` - True if password specified
- `bool is_superuser_` - SUPERUSER flag

**Methods:**
- `username()` - Returns username
- `password()` - Returns password
- `hasPassword()` - Returns true if has password
- `isSuperuser()` - Returns SUPERUSER flag

**Syntax:** `CREATE USER username [WITH PASSWORD 'xxx'] [SUPERUSER]`

#### AlterUserStmt
**Purpose:** ALTER USER statement
**Inherits:** Statement
**ASTKind:** ALTER_USER
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId username_` - Username
- `StringPool::StringId password_` - Password (0 if not changing)
- `bool change_password_` - True if changing password
- `bool is_superuser_` - SUPERUSER flag
- `bool change_superuser_` - True if changing SUPERUSER

**Methods:**
- `username()` - Returns username
- `password()` - Returns password
- `changePassword()` - Returns true if changing password
- `isSuperuser()` - Returns SUPERUSER flag
- `changeSuperuser()` - Returns true if changing SUPERUSER

**Syntax:** `ALTER USER username [WITH PASSWORD 'xxx'] [SUPERUSER | NOSUPERUSER]`

#### DropUserStmt
**Purpose:** DROP USER statement
**Inherits:** Statement
**ASTKind:** DROP_USER
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId username_` - Username
- `bool if_exists_` - IF EXISTS flag
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

**Methods:**
- `username()` - Returns username
- `ifExists()` - Returns IF EXISTS flag
- `dropBehavior()` - Returns drop behavior

**Syntax:** `DROP USER username [IF EXISTS] [CASCADE | RESTRICT]`

#### CreateRoleStmt
**Purpose:** CREATE ROLE statement
**Inherits:** Statement
**ASTKind:** CREATE_ROLE
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId rolename_` - Role name

**Methods:**
- `rolename()` - Returns role name

**Syntax:** `CREATE ROLE rolename`

#### DropRoleStmt
**Purpose:** DROP ROLE statement
**Inherits:** Statement
**ASTKind:** DROP_ROLE
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId rolename_` - Role name
- `bool if_exists_` - IF EXISTS flag
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

**Methods:**
- `rolename()` - Returns role name
- `ifExists()` - Returns IF EXISTS flag
- `dropBehavior()` - Returns drop behavior

**Syntax:** `DROP ROLE rolename [IF EXISTS] [CASCADE | RESTRICT]`

#### CreateGroupStmt
**Purpose:** CREATE GROUP statement
**Inherits:** Statement
**ASTKind:** CREATE_GROUP
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId groupname_` - Group name

**Methods:**
- `groupname()` - Returns group name

**Syntax:** `CREATE GROUP groupname`

#### DropGroupStmt
**Purpose:** DROP GROUP statement
**Inherits:** Statement
**ASTKind:** DROP_GROUP
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId groupname_` - Group name
- `bool if_exists_` - IF EXISTS flag
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

**Methods:**
- `groupname()` - Returns group name
- `ifExists()` - Returns IF EXISTS flag
- `dropBehavior()` - Returns drop behavior

**Syntax:** `DROP GROUP groupname [IF EXISTS] [CASCADE | RESTRICT]`

#### GrantPrivilegeStmt
**Purpose:** GRANT privilege statement
**Inherits:** Statement
**ASTKind:** GRANT_PRIVILEGE
**Phase:** ALPHA Phase 1 - Security System Phase 2, Phase 3.3.3

**Fields:**
- `uint32_t privileges_` - Privilege bitmask
- `ObjectType object_type_` - Object type
- `StringPool::StringId object_name_` - Object name
- `GranteeType grantee_type_` - Grantee type
- `StringPool::StringId grantee_name_` - Grantee name (0 for PUBLIC)
- `bool with_grant_option_` - WITH GRANT OPTION flag
- `std::vector<StringPool::StringId> column_names_` - Column-level permissions (Security Phase 3.3.3)

**Methods:**
- `privileges()` - Returns privilege bitmask
- `objectType()` - Returns object type
- `objectName()` - Returns object name
- `granteeType()` - Returns grantee type
- `granteeName()` - Returns grantee name
- `withGrantOption()` - Returns WITH GRANT OPTION flag
- `columnNames()` - Returns column names (Security Phase 3.3.3)
- `hasColumnList()` - Returns true if has column list

**Syntax:** `GRANT privilege ON object TO grantee [WITH GRANT OPTION]`

#### RevokePrivilegeStmt
**Purpose:** REVOKE privilege statement
**Inherits:** Statement
**ASTKind:** REVOKE_PRIVILEGE
**Phase:** ALPHA Phase 1 - Security System Phase 2, Phase 3.3.3

**Fields:**
- `uint32_t privileges_` - Privilege bitmask
- `ObjectType object_type_` - Object type
- `StringPool::StringId object_name_` - Object name
- `GranteeType grantee_type_` - Grantee type
- `StringPool::StringId grantee_name_` - Grantee name
- `RevokeBehavior revoke_behavior_` - CASCADE or RESTRICT
- `std::vector<StringPool::StringId> column_names_` - Column-level permissions (Security Phase 3.3.3)

**Methods:**
- `privileges()` - Returns privilege bitmask
- `objectType()` - Returns object type
- `objectName()` - Returns object name
- `granteeType()` - Returns grantee type
- `granteeName()` - Returns grantee name
- `revokeBehavior()` - Returns revoke behavior
- `columnNames()` - Returns column names (Security Phase 3.3.3)
- `hasColumnList()` - Returns true if has column list

**Syntax:** `REVOKE privilege ON object FROM grantee [CASCADE | RESTRICT]`

#### GrantRoleStmt
**Purpose:** GRANT role statement
**Inherits:** Statement
**ASTKind:** GRANT_ROLE
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId rolename_` - Role name
- `GranteeType grantee_type_` - Grantee type (USER or ROLE)
- `StringPool::StringId grantee_name_` - Grantee name
- `bool with_admin_option_` - WITH ADMIN OPTION flag

**Methods:**
- `rolename()` - Returns role name
- `granteeType()` - Returns grantee type
- `granteeName()` - Returns grantee name
- `withAdminOption()` - Returns WITH ADMIN OPTION flag

**Syntax:** `GRANT role TO user/role [WITH ADMIN OPTION]`

#### RevokeRoleStmt
**Purpose:** REVOKE role statement
**Inherits:** Statement
**ASTKind:** REVOKE_ROLE
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId rolename_` - Role name
- `GranteeType grantee_type_` - Grantee type (USER or ROLE)
- `StringPool::StringId grantee_name_` - Grantee name
- `RevokeBehavior revoke_behavior_` - CASCADE or RESTRICT

**Methods:**
- `rolename()` - Returns role name
- `granteeType()` - Returns grantee type
- `granteeName()` - Returns grantee name
- `revokeBehavior()` - Returns revoke behavior

**Syntax:** `REVOKE role FROM user/role [CASCADE | RESTRICT]`

#### SetRoleStmt
**Purpose:** SET ROLE statement
**Inherits:** Statement
**ASTKind:** SET_ROLE
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId rolename_` - Role name (0 for RESET)
- `bool is_reset_` - True if RESET ROLE

**Methods:**
- `rolename()` - Returns role name
- `isReset()` - Returns true if RESET

**Syntax:** `SET ROLE rolename` or `RESET ROLE`

#### SetSessionAuthStmt
**Purpose:** SET SESSION AUTHORIZATION statement
**Inherits:** Statement
**ASTKind:** SET_SESSION_AUTH
**Phase:** ALPHA Phase 1 - Security System Phase 2

**Fields:**
- `StringPool::StringId username_` - Username (0 for RESET)
- `bool is_reset_` - True if RESET

**Methods:**
- `username()` - Returns username
- `isReset()` - Returns true if RESET

**Syntax:** `SET SESSION AUTHORIZATION username` or `RESET SESSION AUTHORIZATION`

#### SetConstraintsStmt
**Purpose:** SET CONSTRAINTS statement
**Inherits:** Statement
**ASTKind:** SET_CONSTRAINTS
**Phase:** P2-7

**Fields:**
- `bool all_constraints_` - True if ALL was specified
- `std::vector<StringPool::StringId> constraint_names_` - Constraint names (empty if all)
- `bool deferred_` - True = DEFERRED, false = IMMEDIATE

**Methods:**
- `allConstraints()` - Returns true if ALL
- `constraintNames()` - Returns constraint names
- `isDeferred()` - Returns true if DEFERRED

**Syntax:** `SET CONSTRAINTS {ALL | constraint_name [, ...]} {DEFERRED | IMMEDIATE}`

#### CreatePolicyStmt
**Purpose:** CREATE POLICY statement for Row Level Security
**Inherits:** Statement
**ASTKind:** CREATE_POLICY
**Phase:** Security Phase 3.4

**Fields:**
- `StringPool::StringId policy_name_` - Policy name
- `StringPool::StringId table_name_` - Table name
- `PolicyCommand command_` - Command type (ALL/SELECT/INSERT/UPDATE/DELETE)
- `std::vector<StringPool::StringId> roles_` - Roles (empty = applies to all)
- `Expression* using_expr_` - USING expression (can be nullptr)
- `Expression* with_check_expr_` - WITH CHECK expression (can be nullptr)

**Methods:**
- `policyName()` - Returns policy name
- `tableName()` - Returns table name
- `command()` - Returns command type
- `roles()` - Returns roles
- `usingExpr()` - Returns USING expression
- `withCheckExpr()` - Returns WITH CHECK expression
- `appliesToAllRoles()` - Returns true if applies to all roles
- `hasUsingExpr()` - Returns true if has USING
- `hasWithCheckExpr()` - Returns true if has WITH CHECK

**Syntax:**
```sql
CREATE POLICY policy_name ON table_name
  [FOR {ALL | SELECT | INSERT | UPDATE | DELETE}]
  [TO {role_name [, ...] | PUBLIC}]
  [USING (expression)]
  [WITH CHECK (expression)]
```

#### DropPolicyStmt
**Purpose:** DROP POLICY statement
**Inherits:** Statement
**ASTKind:** DROP_POLICY
**Phase:** Security Phase 3.4

**Fields:**
- `StringPool::StringId policy_name_` - Policy name
- `StringPool::StringId table_name_` - Table name
- `bool if_exists_` - IF EXISTS flag
- `DropBehavior drop_behavior_` - CASCADE or RESTRICT

**Methods:**
- `policyName()` - Returns policy name
- `tableName()` - Returns table name
- `ifExists()` - Returns IF EXISTS flag
- `dropBehavior()` - Returns drop behavior

**Syntax:** `DROP POLICY [IF EXISTS] policy_name ON table_name [CASCADE | RESTRICT]`

#### AlterTableRLSStmt
**Purpose:** ALTER TABLE ... ROW LEVEL SECURITY statement
**Inherits:** Statement
**ASTKind:** ALTER_TABLE_RLS
**Phase:** Security Phase 3.4

**Fields:**
- `StringPool::StringId table_name_` - Table name
- `RLSAction action_` - Action (ENABLE/DISABLE/FORCE/NO_FORCE)

**Methods:**
- `tableName()` - Returns table name
- `action()` - Returns action

**Syntax:**
- `ALTER TABLE table_name ENABLE ROW LEVEL SECURITY`
- `ALTER TABLE table_name DISABLE ROW LEVEL SECURITY`
- `ALTER TABLE table_name FORCE ROW LEVEL SECURITY`
- `ALTER TABLE table_name NO FORCE ROW LEVEL SECURITY`

### Session Control Statements

#### SetSqlDialectStmt
**Purpose:** SET SQL DIALECT statement (Firebird ISQL compatibility)
**Inherits:** Statement
**ASTKind:** SET_SQL_DIALECT

**Fields:**
- `uint8_t dialect_` - Dialect number (1, 2, or 3)

**Methods:**
- `dialect()` - Returns dialect number

**Syntax:** `SET SQL DIALECT N` (where N = 1, 2, or 3)

#### SetNamesStmt
**Purpose:** SET NAMES statement (connection character set)
**Inherits:** Statement
**ASTKind:** SET_NAMES

**Fields:**
- `StringPool::StringId charset_name_` - Character set name

**Methods:**
- `charsetName()` - Returns character set name

**Syntax:** `SET NAMES 'charset'`

#### SetLocalTimeoutStmt
**Purpose:** SET LOCAL_TIMEOUT statement (statement timeout)
**Inherits:** Statement
**ASTKind:** SET_LOCAL_TIMEOUT

**Fields:**
- `uint32_t timeout_seconds_` - Timeout in seconds

**Methods:**
- `timeoutSeconds()` - Returns timeout in seconds

**Syntax:** `SET LOCAL_TIMEOUT N`

### Trigger Statements

#### CreateTriggerStmt
**Purpose:** CREATE TRIGGER statement
**Inherits:** Statement
**ASTKind:** CREATE_TRIGGER
**Phase:** Phase 2 Wave 2 - Agent C, P2-8

**Fields:**
- `StringPool::StringId trigger_name_` - Trigger name
- `StringPool::StringId table_name_` - Table name
- `TriggerTiming timing_` - BEFORE or AFTER
- `TriggerEvent event_` - INSERT/UPDATE/DELETE
- `TriggerGranularity granularity_` - FOR_EACH_ROW or FOR_EACH_STATEMENT
- `StringPool::StringId procedure_name_` - Procedure to execute
- `TransitionTableNames transition_tables_` - Transition tables (P2-8)
- `Expression* when_condition_` - WHEN condition (P2-8)

**Methods:**
- `triggerName()` - Returns trigger name
- `tableName()` - Returns table name
- `timing()` - Returns timing
- `event()` - Returns event
- `granularity()` - Returns granularity
- `procedureName()` - Returns procedure name
- `transitionTables()` - Returns transition tables (P2-8)
- `hasOldTable()`, `hasNewTable()` - Transition table checks
- `oldTableName()`, `newTableName()` - Transition table names
- `whenCondition()` - Returns WHEN condition (P2-8)

**Constructors:**
- Original: `CreateTriggerStmt(span, trigger_name, table_name, timing, event, granularity, procedure_name)`
- P2-8: `CreateTriggerStmt(span, trigger_name, table_name, timing, event, granularity, procedure_name, transition_tables, when_condition)`

#### DropTriggerStmt
**Purpose:** DROP TRIGGER statement
**Inherits:** Statement
**ASTKind:** DROP_TRIGGER
**Phase:** Phase 2 Wave 2 - Agent C

**Fields:**
- `StringPool::StringId trigger_name_` - Trigger name
- `bool if_exists_` - IF EXISTS flag

**Methods:**
- `triggerName()` - Returns trigger name
- `ifExists()` - Returns IF EXISTS flag

#### CreateDatabaseTriggerStmt
**Purpose:** CREATE DATABASE TRIGGER statement (Firebird-style)
**Inherits:** Statement
**ASTKind:** CREATE_DATABASE_TRIGGER

**Fields:**
- `StringPool::StringId trigger_name_` - Trigger name
- `DatabaseTriggerEvent event_` - Event type
- `bool active_` - ACTIVE or INACTIVE
- `int32_t position_` - Execution order (0 = default, lower executes first)
- `StringPool::StringId procedure_name_` - Procedure to execute

**Methods:**
- `triggerName()` - Returns trigger name
- `event()` - Returns event type
- `isActive()` - Returns ACTIVE flag
- `position()` - Returns position
- `procedureName()` - Returns procedure name

**Syntax:** `CREATE TRIGGER name [ACTIVE | INACTIVE] ON event [POSITION n] AS ...`

### Procedural Language Statements

#### CreateFunctionStmt
**Purpose:** CREATE FUNCTION statement
**Inherits:** Statement
**ASTKind:** CREATE_FUNCTION
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId name_` - Function name
- `std::vector<Parameter*> parameters_` - Parameters
- `TypeName* return_type_` - Return type
- `bool or_replace_` - OR REPLACE flag
- `BlockStmt* body_` - Function body
- `SqlSecurity sql_security_` - SQL SECURITY mode (Phase 3.1)

**Methods:**
- `name()` - Returns function name
- `parameters()` - Returns parameters
- `returnType()` - Returns return type
- `orReplace()` - Returns OR REPLACE flag
- `body()` - Returns function body
- `sqlSecurity()` - Returns SQL SECURITY mode

#### CreateProcedureStmt
**Purpose:** CREATE PROCEDURE statement
**Inherits:** Statement
**ASTKind:** CREATE_PROCEDURE
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId name_` - Procedure name
- `std::vector<Parameter*> parameters_` - Parameters
- `bool or_replace_` - OR REPLACE flag
- `BlockStmt* body_` - Procedure body
- `SqlSecurity sql_security_` - SQL SECURITY mode (Phase 3.1)

**Methods:**
- `name()` - Returns procedure name
- `parameters()` - Returns parameters
- `orReplace()` - Returns OR REPLACE flag
- `body()` - Returns procedure body
- `sqlSecurity()` - Returns SQL SECURITY mode

#### BlockStmt
**Purpose:** BEGIN...END block
**Inherits:** Statement
**ASTKind:** BLOCK
**Phase:** Phase 2 Task 10.2

**Fields:**
- `std::vector<VarDeclarationStmt*> declarations_` - Variable declarations
- `std::vector<Statement*> statements_` - Statements
- `std::vector<ExceptionHandler*> exception_handlers_` - Exception handlers

**Methods:**
- `declarations()` - Returns declarations
- `statements()` - Returns statements
- `exceptionHandlers()` - Returns exception handlers

#### VarDeclarationStmt
**Purpose:** Variable declaration statement
**Inherits:** Statement
**ASTKind:** VAR_DECLARATION
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId name_` - Variable name
- `TypeName* type_` - Variable type
- `bool is_constant_` - CONSTANT flag
- `Expression* default_value_` - Default value

**Methods:**
- `name()` - Returns variable name
- `type()` - Returns variable type
- `isConstant()` - Returns CONSTANT flag
- `defaultValue()` - Returns default value

#### AssignmentStmt
**Purpose:** Variable assignment statement
**Inherits:** Statement
**ASTKind:** ASSIGNMENT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId variable_` - Variable name
- `Expression* value_` - Value expression

**Methods:**
- `variable()` - Returns variable name
- `value()` - Returns value expression

#### IfStmt
**Purpose:** IF statement
**Inherits:** Statement
**ASTKind:** IF_STMT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `Expression* condition_` - IF condition
- `std::vector<Statement*> then_stmts_` - THEN statements
- `std::vector<ElsIfClause*> elsif_clauses_` - ELSIF clauses
- `std::vector<Statement*> else_stmts_` - ELSE statements

**Methods:**
- `condition()` - Returns condition
- `thenStatements()` - Returns THEN statements
- `elsifClauses()` - Returns ELSIF clauses
- `elseStatements()` - Returns ELSE statements

#### LoopStmt
**Purpose:** LOOP statement
**Inherits:** Statement
**ASTKind:** LOOP_STMT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId label_` - Loop label
- `std::vector<Statement*> statements_` - Loop statements

**Methods:**
- `label()` - Returns loop label
- `statements()` - Returns loop statements

#### WhileStmt
**Purpose:** WHILE statement
**Inherits:** Statement
**ASTKind:** WHILE_STMT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId label_` - Loop label
- `Expression* condition_` - WHILE condition
- `std::vector<Statement*> statements_` - Loop statements

**Methods:**
- `label()` - Returns loop label
- `condition()` - Returns condition
- `statements()` - Returns loop statements

#### ExitStmt
**Purpose:** EXIT statement
**Inherits:** Statement
**ASTKind:** EXIT_STMT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId label_` - Loop label (0 = exit innermost loop)
- `Expression* when_condition_` - WHEN condition

**Methods:**
- `label()` - Returns loop label
- `whenCondition()` - Returns WHEN condition

#### ReturnStmt
**Purpose:** RETURN statement
**Inherits:** Statement
**ASTKind:** RETURN_STMT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `Expression* return_value_` - Return value

**Methods:**
- `returnValue()` - Returns return value

#### RaiseStmt
**Purpose:** RAISE statement
**Inherits:** Statement
**ASTKind:** RAISE_STMT
**Phase:** Phase 2 Task 10.2

**Fields:**
- `Level level_` - Severity level
- `Expression* message_` - Message expression
- `std::vector<Expression*> args_` - Message arguments

**Methods:**
- `level()` - Returns severity level
- `message()` - Returns message
- `args()` - Returns arguments

#### CallStmt
**Purpose:** CALL procedure statement
**Inherits:** Statement
**ASTKind:** CALL
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId procedure_name_` - Procedure name
- `std::vector<Expression*> arguments_` - Arguments

**Methods:**
- `procedureName()` - Returns procedure name
- `arguments()` - Returns arguments

**Syntax:** `CALL procedure_name(arg1, arg2, ...)`

---

## Supporting Structures

### TypeName
**Purpose:** Type name representation (backward compatibility during migration)

**Fields:**
- `DataType type` - Data type
- `uint32_t precision` - For VARCHAR(n), CHAR(n)
- `uint32_t scale` - For DECIMAL(p,s)
- `bool with_timezone` - For TIMESTAMP WITH TIME ZONE
- `uint16_t timezone_hint` - Timezone ID for display

**Methods:**
- `toTypeInfo()` - Convert to TypeInfo

**Constructor:**
- `TypeName(type, precision=0, scale=0, with_timezone=false, timezone_hint=0)`

### SourceSpan
**Purpose:** Source location for error reporting

**Fields:**
- `SourceLocation start` - Start location
- `SourceLocation end` - End location

**Constructors:**
- Default: `SourceSpan()`
- With locations: `SourceSpan(start, end)`

### ColumnDef
**Purpose:** Column definition for CREATE TABLE
**Inherits:** ASTNode
**ASTKind:** COLUMN_DEF

**Fields:**
- `StringPool::StringId name_` - Column name
- `TypeName type_` - Column type
- `bool nullable_` - NULL/NOT NULL
- `StringPool::StringId charset_` - CHARACTER SET clause
- `StringPool::StringId collation_` - COLLATE clause
- `Expression* default_value_` - DEFAULT clause expression
- `Expression* check_expr_` - CHECK constraint expression
- `bool is_unique_` - UNIQUE constraint flag
- `bool is_primary_key_` - PRIMARY KEY constraint flag
- `StringPool::StringId fk_table_` - REFERENCES table name
- `std::vector<StringPool::StringId> fk_columns_` - REFERENCES column list
- `StringPool::StringId fk_on_delete_` - ON DELETE action
- `StringPool::StringId fk_on_update_` - ON UPDATE action
- `bool is_identity_` - IDENTITY column flag (ALPHA Phase 1)
- `bool identity_always_` - true=ALWAYS, false=BY DEFAULT (ALPHA Phase 1)
- `GeneratedColumnStorage generated_storage_` - GENERATED column storage type (ALPHA Phase 1)
- `Expression* generation_expr_` - GENERATED ALWAYS AS expression (ALPHA Phase 1)

**Methods:**
- `name()` - Returns column name
- `type()` - Returns column type
- `nullable()` - Returns nullable flag
- `charset()` - Returns character set
- `collation()` - Returns collation
- `default_value()` - Returns default value
- `check_expr()` - Returns check expression
- `isUnique()` - Returns UNIQUE flag
- `isPrimaryKey()` - Returns PRIMARY KEY flag
- `fk_table()`, `fk_columns()` - Foreign key accessors
- `fk_on_delete()`, `fk_on_update()` - Foreign key actions
- `isIdentity()` - Returns IDENTITY flag (ALPHA Phase 1)
- `identityAlways()` - Returns ALWAYS/BY DEFAULT flag (ALPHA Phase 1)
- `generatedStorage()` - Returns generated storage type (ALPHA Phase 1)
- `generationExpr()` - Returns generation expression (ALPHA Phase 1)
- `isGenerated()` - Returns true if generated column (ALPHA Phase 1)

### TableConstraint
**Purpose:** Base class for table-level constraints
**Inherits:** ASTNode
**ASTKind:** COLUMN_DEF (reused)
**Phase:** ALPHA Phase C - Composite FK

**Fields:**
- `ConstraintType type_` - Constraint type
- `StringPool::StringId name_` - Optional constraint name

**Methods:**
- `constraintType()` - Returns constraint type
- `name()` - Returns constraint name

**Constraint Types:**
- `FOREIGN_KEY` - Foreign key constraint
- `PRIMARY_KEY` - Primary key constraint
- `UNIQUE` - Unique constraint
- `CHECK` - Check constraint

### ForeignKeyConstraint
**Purpose:** Foreign key table constraint
**Inherits:** TableConstraint
**Phase:** ALPHA Phase C - Composite FK

**Fields:**
- `std::vector<StringPool::StringId> child_columns_` - Child columns
- `StringPool::StringId parent_table_` - Parent table
- `std::vector<StringPool::StringId> parent_columns_` - Parent columns
- `StringPool::StringId on_delete_` - ON DELETE action
- `StringPool::StringId on_update_` - ON UPDATE action
- `bool is_deferrable_` - DEFERRABLE flag (ALPHA Phase 1)
- `bool initially_deferred_` - INITIALLY DEFERRED flag (ALPHA Phase 1)

**Methods:**
- `childColumns()` - Returns child columns
- `parentTable()` - Returns parent table
- `parentColumns()` - Returns parent columns
- `onDelete()` - Returns ON DELETE action
- `onUpdate()` - Returns ON UPDATE action
- `isDeferrable()` - Returns DEFERRABLE flag (ALPHA Phase 1)
- `initiallyDeferred()` - Returns INITIALLY DEFERRED flag (ALPHA Phase 1)

### UniqueConstraint
**Purpose:** UNIQUE table constraint
**Inherits:** TableConstraint

**Fields:**
- `std::vector<StringPool::StringId> columns_` - Columns

**Methods:**
- `columns()` - Returns columns

### PrimaryKeyConstraint
**Purpose:** PRIMARY KEY table constraint
**Inherits:** TableConstraint

**Fields:**
- `std::vector<StringPool::StringId> columns_` - Columns

**Methods:**
- `columns()` - Returns columns

### CheckTableConstraint
**Purpose:** CHECK table constraint
**Inherits:** TableConstraint
**Phase:** WP-6 PARSE-1

**Fields:**
- `Expression* check_expr_` - Check expression

**Methods:**
- `checkExpr()` - Returns check expression

### SelectItem
**Purpose:** SELECT list item

**Fields:**
- `bool is_star` - True if SELECT *
- `Expression* expr` - Expression
- `StringPool::StringId alias` - Optional AS alias

**Constructors:**
- Star: `SelectItem()` (sets `is_star = true`)
- Expression: `SelectItem(expr, alias=0)`

### TableRef
**Purpose:** Table reference in FROM clause

**Fields:**
- `StringPool::StringId table_name` - Table name
- `StringPool::StringId alias` - Optional table alias
- `FunctionCallExpr* table_function` - Table-valued function
- `SelectStmt* subquery` - Derived table subquery
- `bool is_lateral` - LATERAL subquery flag

**Constructors:**
- Table: `TableRef(table_name, alias=0)`
- Function: `TableRef(table_function, alias=0, lateral=false)`
- Subquery: `TableRef(subquery, alias=0, lateral=false)`

**Methods:**
- `isTableName()` - Returns true if table name
- `isTableFunction()` - Returns true if table function
- `isSubquery()` - Returns true if subquery
- `isLateral()` - Returns true if LATERAL

**Supports:**
- table_name
- (SELECT ...) alias - derived table
- function_call(args) alias - table-valued function
- LATERAL subquery - can reference preceding FROM items

### JoinClause
**Purpose:** JOIN clause representation

**Fields:**
- `JoinType join_type` - JOIN type
- `bool natural` - NATURAL JOIN modifier
- `TableRef right_table` - Right table
- `JoinConditionType condition_type` - Condition type
- `Expression* on_condition` - ON condition
- `std::vector<StringPool::StringId> using_columns` - USING clause columns

**Constructor:**
- `JoinClause(join_type, is_natural, right_table, condition_type, on_condition=nullptr)`

### FromClause
**Purpose:** FROM clause with table and optional joins

**Fields:**
- `TableRef base_table` - Base table
- `std::vector<JoinClause> joins` - JOIN clauses

**Constructors:**
- Default: `FromClause()`
- With base: `FromClause(base_table)`

### CTEDefinition
**Purpose:** Common Table Expression definition
**Phase:** Phase 2 Wave 2

**Fields:**
- `StringPool::StringId name` - CTE name
- `SelectStmt* query` - CTE query
- `std::vector<StringPool::StringId> column_aliases` - Optional column aliases
- `bool recursive` - True if recursive CTE

**Constructor:**
- `CTEDefinition(name, query, column_aliases={}, is_recursive=false)`

### WithClause
**Purpose:** WITH clause (CTE support)
**Phase:** Phase 2 Wave 2

**Fields:**
- `std::vector<CTEDefinition> ctes_` - CTEs
- `bool recursive_` - True if WITH RECURSIVE

**Constructor:**
- `WithClause(ctes, is_recursive=false)`

**Methods:**
- `ctes()` - Returns CTEs
- `isRecursive()` - Returns true if recursive

### OrderByItem
**Purpose:** ORDER BY item
**Phase:** Phase 1 Task 5.1

**Fields:**
- `Expression* expr` - Expression to sort by
- `SortOrder order` - ASC or DESC
- `NullsOrder nulls_order` - NULLS FIRST/LAST

**Constructor:**
- `OrderByItem(expr, order=ASC, nulls_order=DEFAULT)`

### GroupByClause
**Purpose:** GROUP BY clause
**Phase:** Phase 1 Task 4.1, Phase 3: Advanced Grouping

**Fields:**
- `GroupingType type` - Grouping type
- `std::vector<Expression*> grouping_exprs` - Simple GROUP BY expressions
- `std::vector<std::vector<Expression*>> grouping_sets` - For GROUPING SETS
- `Expression* having_clause` - Optional HAVING condition

**Constructors:**
- Default: `GroupByClause()`
- Simple: `GroupByClause(exprs, having=nullptr)`
- Advanced: `GroupByClause(grp_type, exprs, having=nullptr)`
- Explicit sets: `GroupByClause(sets, having=nullptr)`

### OnConflictClause
**Purpose:** ON CONFLICT clause for INSERT ... ON CONFLICT (UPSERT)

**Fields:**
- `OnConflictAction action` - Action type
- `std::vector<StringPool::StringId> conflict_columns` - ON CONFLICT (col1, col2)
- `std::vector<StringPool::StringId> update_columns` - SET col = ...
- `std::vector<Expression*> update_values` - ... = value
- `Expression* where_clause` - Optional WHERE clause for DO UPDATE

**Constructor:**
- `OnConflictClause()` (sets `action = NONE`)

### Assignment
**Purpose:** Assignment for UPDATE SET clause

**Fields:**
- `StringPool::StringId column_name` - Column name
- `Expression* value` - Value expression

**Constructor:**
- `Assignment(column_name, value)`

### TableReservation
**Purpose:** Table reservation for RESERVING clause

**Fields:**
- `StringPool::StringId table_name` - Table name
- `TableLockMode lock_mode` - Lock mode
- `bool for_write` - For write flag

**Constructor:**
- `TableReservation(table_name, lock_mode, for_write)`

### FrameBoundary
**Purpose:** Window frame boundary
**Phase:** Phase 1 Task 6

**Fields:**
- `FrameBoundaryType type` - Boundary type
- `Expression* offset` - For PRECEDING/FOLLOWING with offset

**Constructors:**
- Default: `FrameBoundary()` (sets `type = CURRENT_ROW`)
- With type: `FrameBoundary(type, offset=nullptr)`

### TransitionTableNames
**Purpose:** Transition table names for statement-level triggers
**Phase:** P2-8

**Fields:**
- `StringPool::StringId old_table_name` - OLD TABLE name (0 = not specified)
- `StringPool::StringId new_table_name` - NEW TABLE name (0 = not specified)

### Parameter
**Purpose:** Parameter declaration for functions/procedures
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId name` - Parameter name
- `TypeName* type` - Parameter type
- `ParameterMode mode` - IN/OUT/INOUT
- `Expression* default_value` - Optional default value

**Constructor:**
- `Parameter(name, type, mode=IN, default_value=nullptr)`

### ElsIfClause
**Purpose:** ELSIF clause for IF statement
**Phase:** Phase 2 Task 10.2

**Fields:**
- `Expression* condition` - ELSIF condition
- `std::vector<Statement*> statements` - Statements

**Constructor:**
- `ElsIfClause(condition, statements)`

### ExceptionHandler
**Purpose:** Exception handler for BEGIN...END blocks
**Phase:** Phase 2 Task 10.2

**Fields:**
- `StringPool::StringId exception_name` - Exception name (0 for OTHERS)
- `std::vector<Statement*> statements` - Handler statements

**Constructor:**
- `ExceptionHandler(exception_name, statements)`

### TablespaceAlteration
**Purpose:** Tablespace alteration specification
**Phase:** Phase 2 Task 2.2

**Fields:**
- `TablespaceAlterationType type` - Alteration type
- `bool autoextend_enabled` - For SET_AUTOEXTEND
- `uint32_t size_value` - For SET_AUTOEXTEND_SIZE and SET_MAXSIZE
- `StringPool::StringId new_name` - For RENAME_TO

**Constructor:**
- `TablespaceAlteration(type)` (initializes other fields to defaults)

### CaseExpr::WhenClause
**Purpose:** WHEN clause for CASE expression
**Phase:** Phase 1 Task 8

**Fields:**
- `Expression* condition` - WHEN condition
- `Expression* result` - THEN result

### MergeStmt::WhenClause
**Purpose:** WHEN clause for MERGE statement
**Phase:** Alpha 1 - Advanced SQL

**Fields:**
- `Type type` - MATCHED/NOT_MATCHED/NOT_MATCHED_BY_SOURCE
- `Expression* condition` - Optional additional condition
- `std::vector<Assignment> assignments` - For UPDATE
- `std::vector<StringPool::StringId> insert_columns` - For INSERT
- `std::vector<Expression*> insert_values` - For INSERT

**Type enum:**
- `MATCHED` - WHEN MATCHED THEN UPDATE
- `NOT_MATCHED` - WHEN NOT MATCHED THEN INSERT
- `NOT_MATCHED_BY_SOURCE` - WHEN NOT MATCHED BY SOURCE THEN DELETE

---

## Visitor Pattern

### ASTVisitor
**Purpose:** Abstract base class for AST visitor pattern

**Statement Visit Methods:**
- `visit(CreateTableStmt*)` - CREATE TABLE
- `visit(CreateIndexStmt*)` - CREATE INDEX (Phase 2 Task 2.3)
- `visit(DropTableStmt*)` - DROP TABLE (ALPHA Phase 1)
- `visit(DropIndexStmt*)` - DROP INDEX (ALPHA Phase 1)
- `visit(TruncateTableStmt*)` - TRUNCATE TABLE (ALPHA Phase 1)
- `visit(AlterTableStmt*)` - ALTER TABLE (ALPHA Phase 1)
- `visit(CreateSequenceStmt*)` - CREATE SEQUENCE (ALPHA Phase 1)
- `visit(AlterSequenceStmt*)` - ALTER SEQUENCE (ALPHA Phase 1)
- `visit(DropSequenceStmt*)` - DROP SEQUENCE (ALPHA Phase 1)
- `visit(CreateViewStmt*)` - CREATE VIEW (ALPHA Phase 1)
- `visit(DropViewStmt*)` - DROP VIEW (ALPHA Phase 1)
- `visit(RefreshMaterializedViewStmt*)` - REFRESH MATERIALIZED VIEW (ALPHA Phase 1)
- `visit(CreateTablespaceStmt*)` - CREATE TABLESPACE (Phase 2 Task 2.1)
- `visit(AlterTablespaceStmt*)` - ALTER TABLESPACE (Phase 2 Task 2.2)
- `visit(AlterTableSetTablespaceStmt*)` - ALTER TABLE ... SET TABLESPACE (Phase 4)
- `visit(DropTablespaceStmt*)` - DROP TABLESPACE (Phase 2 Task 2.1)
- `visit(AttachTablespaceStmt*)` - ATTACH TABLESPACE (Phase 6)
- `visit(DetachTablespaceStmt*)` - DETACH TABLESPACE (Phase 6)
- `visit(InsertStmt*)` - INSERT
- `visit(SelectStmt*)` - SELECT
- `visit(SetOperationStmt*)` - UNION/INTERSECT/EXCEPT
- `visit(UpdateStmt*)` - UPDATE (Phase 1 Task 2.1)
- `visit(DeleteStmt*)` - DELETE (Phase 1 Task 2.2)
- `visit(MergeStmt*)` - MERGE (Alpha 1)
- `visit(AnalyzeStmt*)` - ANALYZE (Phase 1 Task 1.1.2)
- `visit(ExplainStmt*)` - EXPLAIN (Phase 1 Task 1.5)
- `visit(StartTransactionStmt*)` - START TRANSACTION (Phase 2 Task 2.6)
- `visit(SetTransactionStmt*)` - SET TRANSACTION (Phase 3 Task 3.6)
- `visit(CommitStmt*)` - COMMIT (Phase 2 Task 2.6)
- `visit(RollbackStmt*)` - ROLLBACK (Phase 2 Task 2.6)
- `visit(SweepStmt*)` - SWEEP (Phase 3 Task 3.3)
- `visit(ShowStmt*)` - SHOW (ALPHA Phase 1)
- `visit(DescribeStmt*)` - DESCRIBE (ALPHA Phase 1)
- `visit(CreateTriggerStmt*)` - CREATE TRIGGER (Phase 2 Wave 2)
- `visit(DropTriggerStmt*)` - DROP TRIGGER (Phase 2 Wave 2)
- `visit(CreateDatabaseTriggerStmt*)` - CREATE DATABASE TRIGGER

**PSQL - Stored Procedures and Functions (Phase 2 Task 10.2):**
- `visit(CreateFunctionStmt*)` - CREATE FUNCTION
- `visit(CreateProcedureStmt*)` - CREATE PROCEDURE
- `visit(BlockStmt*)` - BEGIN...END block
- `visit(VarDeclarationStmt*)` - Variable declaration
- `visit(AssignmentStmt*)` - Assignment
- `visit(IfStmt*)` - IF statement
- `visit(LoopStmt*)` - LOOP statement
- `visit(WhileStmt*)` - WHILE loop
- `visit(ExitStmt*)` - EXIT statement
- `visit(ReturnStmt*)` - RETURN statement
- `visit(RaiseStmt*)` - RAISE exception
- `visit(CallStmt*)` - CALL procedure

**Security Statements (ALPHA Phase 1 - Security System Phase 2):**
- `visit(CreateUserStmt*)` - CREATE USER
- `visit(AlterUserStmt*)` - ALTER USER
- `visit(DropUserStmt*)` - DROP USER
- `visit(CreateRoleStmt*)` - CREATE ROLE
- `visit(DropRoleStmt*)` - DROP ROLE
- `visit(CreateGroupStmt*)` - CREATE GROUP
- `visit(DropGroupStmt*)` - DROP GROUP
- `visit(GrantPrivilegeStmt*)` - GRANT privilege
- `visit(RevokePrivilegeStmt*)` - REVOKE privilege
- `visit(GrantRoleStmt*)` - GRANT role
- `visit(RevokeRoleStmt*)` - REVOKE role
- `visit(SetRoleStmt*)` - SET ROLE
- `visit(SetSessionAuthStmt*)` - SET SESSION AUTHORIZATION
- `visit(SetConstraintsStmt*)` - SET CONSTRAINTS (P2-7)
- `visit(SetSqlDialectStmt*)` - SET SQL DIALECT (Firebird ISQL)
- `visit(SetNamesStmt*)` - SET NAMES
- `visit(SetLocalTimeoutStmt*)` - SET LOCAL_TIMEOUT
- `visit(CreatePolicyStmt*)` - CREATE POLICY (Security Phase 3.4)
- `visit(DropPolicyStmt*)` - DROP POLICY (Security Phase 3.4)
- `visit(AlterTableRLSStmt*)` - ALTER TABLE ... RLS (Security Phase 3.4)

**Transaction Control - SAVEPOINT:**
- `visit(SavepointStmt*)` - SAVEPOINT
- `visit(ReleaseSavepointStmt*)` - RELEASE SAVEPOINT
- `visit(RollbackToSavepointStmt*)` - ROLLBACK TO SAVEPOINT

**User Defined Types:**
- `visit(CreateTypeStmt*)` - CREATE TYPE
- `visit(CreateDomainStmt*)` - CREATE DOMAIN

**Expression Visit Methods:**
- `visit(LiteralExpr*)` - Literal
- `visit(IdentifierExpr*)` - Identifier
- `visit(BinaryOpExpr*)` - Binary operation
- `visit(CastExpr*)` - CAST
- `visit(FunctionCallExpr*)` - Function call
- `visit(SequenceFunctionExpr*)` - Sequence function (ALPHA Phase 1)
- `visit(ExtractExpr*)` - EXTRACT
- `visit(AggregateExpr*)` - Aggregate function (Phase 1 Task 4.1)
- `visit(WindowFuncExpr*)` - Window function (Phase 1 Task 6)
- `visit(WindowSpec*)` - Window specification (Phase 1 Task 6)
- `visit(JSONFuncExpr*)` - JSON function (Phase 1 Task 7)
- `visit(CoalesceExpr*)` - COALESCE (Phase 1 Task 8)
- `visit(NullIfExpr*)` - NULLIF (Phase 1 Task 8)
- `visit(CaseExpr*)` - CASE (Phase 1 Task 8)
- `visit(GroupingExpr*)` - GROUPING() (Phase 3)
- `visit(ArrayLiteral*)` - Array literal (Phase 2 Task 12)
- `visit(SubqueryExpr*)` - Subquery (Phase 2 Wave 2)

**Other Node Visit Methods:**
- `visit(ColumnDef*)` - Column definition

### ASTPrinter
**Purpose:** AST printer for debugging
**Inherits:** ASTVisitor

**Fields:**
- `std::ostream& out_` - Output stream
- `const StringPool& pool_` - String pool
- `int indent_` - Indentation level

**Methods:**
- All visitor methods implemented for printing
- `printIndent()` - Print indentation
- `increaseIndent()` - Increase indentation
- `decreaseIndent()` - Decrease indentation

**Constructor:**
- `ASTPrinter(out, pool)`

**Note:** Not all visitor methods are implemented in ASTPrinter. Some are stubs.

---

## Arena Allocator

### ASTArena
**Purpose:** Arena allocator for AST nodes

**Methods:**
- `make<T>(args...)` - Allocate and construct object of type T
- `reset()` - Reset arena (calls destructors and clears memory)

**Fields:**
- `std::vector<Block> blocks_` - Memory blocks
- `std::vector<std::pair<void*, void(*)(void*)>> destructors_` - Objects needing destruction
- `static constexpr size_t BLOCK_SIZE = 64 * 1024` - 64KB blocks

**Sub-structure:**
- `Block` - Memory block
  - `std::unique_ptr<uint8_t[]> data` - Block data
  - `size_t size` - Block size
  - `size_t used` - Bytes used

**Methods:**
- `allocate(size)` - Allocate memory (private)
- `registerDestructor(obj, destructor)` - Register destructor (private)

---

## Summary Statistics

**Total AST Node Classes:** 100+

**Statement Types:** 70+
- DDL: 20+
- DML: 8
- Transaction: 10
- Security: 19
- Procedural: 11
- Utility: 5

**Expression Types:** 19

**Enumerations:** 30+

**Supporting Structures:** 20+

**Visitor Methods:** 70+

---

## Notes

1. **Phase Tracking:** Many nodes are annotated with their implementation phase (e.g., "Phase 1 Task 4.1", "ALPHA Phase 1", etc.)

2. **Firebird Compatibility:** Several features support Firebird ISQL compatibility (SET SQL DIALECT, SHOW commands, database triggers)

3. **Security System:** Comprehensive security system with users, roles, groups, privileges, policies, and Row Level Security (RLS)

4. **Procedural Language:** Full procedural language support (PSQL-compatible) with functions, procedures, control flow, and exception handling

5. **Advanced SQL:** Support for advanced SQL features including CTEs, window functions, JSON operations, set operations, and MERGE statements

6. **Visitor Pattern:** Complete visitor pattern implementation with both interface (ASTVisitor) and printer (ASTPrinter)

7. **Arena Allocation:** Custom arena allocator for efficient AST node memory management

8. **Type System:** Unified type system with DataType and TypeInfo, transitioning from legacy TypeName

9. **Extensibility:** Well-structured for future extensions with clear separation of concerns and comprehensive coverage

10. **Backward Compatibility:** Many constructors and methods maintain backward compatibility while adding new features

---

## References

- **Source File:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h`
- **Total Lines:** 4,793
- **Last Modified:** Recent (as of 2025-12-06)
- **Related Files:**
  - `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/token.h` - Token definitions
  - `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/types.h` - Type system
  - `/home/dcalford/CliWork/ScratchBird/src/parser/ast.cpp` - AST implementation

---

**End of AST Node Reference**
