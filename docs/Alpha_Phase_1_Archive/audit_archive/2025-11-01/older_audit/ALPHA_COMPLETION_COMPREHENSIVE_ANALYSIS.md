# SCRATCHBIRD ALPHA COMPLETION - COMPREHENSIVE ANALYSIS

**Project**: ScratchBird Database Engine
**Target**: Complete Alpha Release (Embedded Mode with Full Firebird Compatibility)
**Date**: October 16, 2025
**Version**: Alpha 1.2 → Alpha 2.0 (Feature Complete)

---

## EXECUTIVE SUMMARY

This document provides a comprehensive analysis for completing the ScratchBird Alpha release. The goal is to achieve **full Firebird SQL compatibility** with an **embedded engine** that can be accessed via:

1. **API Layer** - Direct C++ API for programmatic access
2. **SBLR Bytecode** - Compiled stored procedures, functions, triggers
3. **SQL Parser** - Firebird-compatible SQL parser → SBLR → Engine
4. **Command-Line Tool** - isql-compatible CLI using the parser + embedded engine

### Current Status

**Storage Engine**: ✅ 100% Complete (MGA, MVCC, indexes, TOAST, vacuum)
**Transaction System**: ✅ 100% Complete (Firebird-style with 4 isolation levels)
**SQL Coverage**: ⚠️ ~25% Complete (basic CREATE TABLE, INSERT, SELECT only)
**Firebird Compatibility**: ⚠️ ~20% Complete (missing 80% of SQL features)

### Work Required

- **227 features** identified for Alpha completion
- **89 CRITICAL** features (core SQL, DDL, DML)
- **76 HIGH** priority features (advanced SQL, PSQL)
- **45 MEDIUM** priority features (system functions, monitoring)
- **17 LOW** priority features (nice-to-have)

**Estimated Effort**: 12-16 weeks (full-time) or 24-32 weeks (half-time)

---

## 1. FIREBIRD FEATURE REQUIREMENTS

### 1.1 Core DDL (Data Definition Language)

#### CRITICAL Priority (Must Have for Alpha)

**TABLE Management** (5 features)
- ✅ CREATE TABLE (exists, needs PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK constraints)
- ❌ ALTER TABLE (add/drop/modify columns, add/drop constraints)
- ❌ DROP TABLE (with CASCADE/RESTRICT)
- ❌ TRUNCATE TABLE
- ❌ CREATE TABLE AS SELECT (CTAS)

**INDEX Management** (4 features)
- ❌ CREATE INDEX (ASC/DESC, UNIQUE)
- ❌ CREATE INDEX ... COMPUTED BY (expression indexes)
- ❌ DROP INDEX
- ❌ ALTER INDEX (ACTIVE/INACTIVE)

**VIEW Management** (4 features)
- ❌ CREATE VIEW
- ❌ CREATE OR REPLACE VIEW
- ❌ DROP VIEW
- ❌ ALTER VIEW

**DOMAIN Management** (4 features)
- ❌ CREATE DOMAIN
- ❌ ALTER DOMAIN
- ❌ DROP DOMAIN
- ⚠️ Domain constraints (CHECK, NOT NULL, DEFAULT) - partially in domain_manager.cpp

**SEQUENCE Management** (6 features)
- ❌ CREATE SEQUENCE (START WITH, INCREMENT BY)
- ❌ ALTER SEQUENCE (RESTART WITH)
- ❌ DROP SEQUENCE
- ❌ GEN_ID(generator, increment) function
- ❌ NEXT VALUE FOR sequence_name
- ❌ Integration with IDENTITY columns

**TRIGGER Management** (5 features)
- ❌ CREATE TRIGGER (BEFORE/AFTER INSERT/UPDATE/DELETE)
- ❌ CREATE TRIGGER ... POSITION n
- ❌ ALTER TRIGGER (ACTIVE/INACTIVE)
- ❌ DROP TRIGGER
- ❌ RECREATE TRIGGER

**PROCEDURE Management** (5 features)
- ❌ CREATE PROCEDURE
- ❌ ALTER PROCEDURE
- ❌ DROP PROCEDURE
- ❌ RECREATE PROCEDURE
- ❌ CREATE OR ALTER PROCEDURE

**FUNCTION Management** (4 features)
- ❌ CREATE FUNCTION (RETURNS type, RETURNS TABLE)
- ❌ ALTER FUNCTION
- ❌ DROP FUNCTION
- ❌ CREATE OR ALTER FUNCTION

#### HIGH Priority (Important for Completeness)

**EXCEPTION Management** (3 features)
- ❌ CREATE EXCEPTION
- ❌ ALTER EXCEPTION
- ❌ DROP EXCEPTION

**COLLATION Management** (3 features)
- ⚠️ CREATE COLLATION (structure exists, needs implementation)
- ❌ DROP COLLATION
- ❌ ALTER CHARACTER SET (set default collation)

**PACKAGE Management** (Firebird 3.0+) (3 features)
- ❌ CREATE PACKAGE (package specification)
- ❌ CREATE PACKAGE BODY (implementation)
- ❌ DROP PACKAGE

---

### 1.2 Core DML (Data Manipulation Language)

#### CRITICAL Priority

**Basic DML** (3 features)
- ✅ INSERT INTO ... VALUES (exists)
- ❌ UPDATE ... SET ... WHERE
- ❌ DELETE FROM ... WHERE

**Multi-row Operations** (3 features)
- ❌ INSERT INTO ... VALUES (...), (...), ... (multi-row insert)
- ❌ INSERT INTO ... SELECT ...
- ❌ UPDATE ... SET ... FROM ... WHERE (Firebird 5.0+)

**MERGE** (2 features)
- ❌ MERGE INTO ... USING ... WHEN MATCHED/NOT MATCHED
- ❌ UPDATE OR INSERT (Firebird-specific shorthand)

**RETURNING Clause** (2 features)
- ❌ INSERT ... RETURNING
- ❌ UPDATE ... RETURNING
- ❌ DELETE ... RETURNING

#### SELECT Statement Extensions

**JOIN Types** (CRITICAL - 5 features)
- ❌ INNER JOIN
- ❌ LEFT [OUTER] JOIN
- ❌ RIGHT [OUTER] JOIN
- ❌ FULL [OUTER] JOIN
- ❌ CROSS JOIN

**Subqueries** (CRITICAL - 4 features)
- ❌ Scalar subqueries (SELECT (SELECT ...) FROM ...)
- ❌ IN subqueries (WHERE x IN (SELECT ...))
- ❌ EXISTS subqueries (WHERE EXISTS (SELECT ...))
- ❌ Correlated subqueries

**Aggregation** (CRITICAL - 5 features)
- ❌ GROUP BY (with multiple columns)
- ❌ HAVING clause
- ❌ Aggregate functions (SUM, AVG, MIN, MAX, COUNT) - parsed but not executed
- ❌ GROUP BY with ROLLUP (subtotals)
- ❌ GROUP BY with CUBE (cross-tabulation)

**Sorting and Limiting** (CRITICAL - 4 features)
- ❌ ORDER BY (ASC/DESC, NULLS FIRST/LAST)
- ❌ DISTINCT
- ❌ LIMIT / OFFSET (or ROWS / FETCH FIRST in Firebird)
- ❌ FOR UPDATE / WITH LOCK (row locking for SELECT)

**Set Operations** (HIGH - 3 features)
- ❌ UNION [ALL]
- ❌ INTERSECT [ALL]
- ❌ EXCEPT [ALL]

**Common Table Expressions** (HIGH - 2 features)
- ❌ WITH cte_name AS (...)
- ❌ WITH RECURSIVE (recursive CTEs)

**Window Functions** (MEDIUM - 6 features)
- ❌ OVER (PARTITION BY ... ORDER BY ...)
- ❌ ROW_NUMBER(), RANK(), DENSE_RANK()
- ❌ LEAD(), LAG(), FIRST_VALUE(), LAST_VALUE()
- ❌ NTILE(n)
- ❌ Window frame specification (ROWS BETWEEN ... AND ...)
- ❌ Aggregate functions as window functions (SUM() OVER ...)

---

### 1.3 PSQL (Procedural SQL)

#### CRITICAL Priority

**Control Flow** (7 features)
- ❌ IF ... THEN ... ELSE ... END IF
- ❌ WHILE ... DO ... END WHILE
- ❌ FOR ... DO ... END FOR
- ❌ FOR SELECT ... INTO ... DO ... END FOR
- ❌ CASE ... WHEN ... THEN ... END CASE
- ❌ EXIT, BREAK, CONTINUE
- ❌ LEAVE (exit labeled block)

**Variables** (4 features)
- ❌ DECLARE variables
- ❌ SET variable assignments
- ❌ SELECT ... INTO variable
- ❌ Variable %TYPE and %ROWTYPE

**Cursors** (5 features)
- ❌ DECLARE CURSOR
- ❌ OPEN cursor
- ❌ FETCH cursor INTO
- ❌ CLOSE cursor
- ❌ Cursor attributes (FOUND, NOTFOUND)

**Exception Handling** (4 features)
- ❌ BEGIN ... WHEN exception THEN ... END
- ❌ EXCEPTION statement (raise user-defined exception)
- ❌ GDSCODE, SQLCODE, SQLSTATE (error code variables)
- ❌ POST_EVENT (async event notifications)

**Autonomous Transactions** (2 features)
- ❌ IN AUTONOMOUS TRANSACTION (Firebird 4.0+)
- ❌ Nested transaction control

---

### 1.4 Built-in Functions

#### CRITICAL Priority

**Aggregate Functions** (12 functions)
- ⚠️ COUNT(*), COUNT(expr) - parsed, needs execution
- ⚠️ SUM(expr) - parsed, needs execution
- ⚠️ AVG(expr) - parsed, needs execution
- ⚠️ MIN(expr) - parsed, needs execution
- ⚠️ MAX(expr) - parsed, needs execution
- ❌ LIST(expr [, separator]) - string aggregation
- ❌ STDDEV(), VARIANCE() - statistical functions
- ❌ FIRST(), LAST() - first/last value in group
- ❌ ANY(), ALL(), SOME() - quantified comparisons
- ❌ COVAR_POP(), COVAR_SAMP() - covariance
- ❌ CORR() - correlation coefficient
- ❌ REGR_* functions (linear regression)

**String Functions** (18 functions)
- ✅ SUBSTRING(str FROM start [FOR length])
- ✅ UPPER(str), LOWER(str)
- ✅ TRIM([LEADING|TRAILING|BOTH] [chars] FROM str)
- ✅ LENGTH(str), CHAR_LENGTH(str), OCTET_LENGTH(str)
- ❌ POSITION(substr IN str)
- ❌ LPAD(str, length [, fill]), RPAD(str, length [, fill])
- ❌ LEFT(str, n), RIGHT(str, n)
- ❌ REVERSE(str)
- ❌ ASCII_VAL(str), ASCII_CHAR(code)
- ❌ REPLACE(str, find, replace)
- ❌ OVERLAY(str PLACING newstr FROM start [FOR length])
- ❌ HASH(expr) - hash value
- ❌ ENCRYPT(str), DECRYPT(str) - encryption
- ❌ BASE64_ENCODE(blob), BASE64_DECODE(str)
- ❌ HEX_ENCODE(blob), HEX_DECODE(str)
- ❌ UUID_TO_CHAR(uuid), CHAR_TO_UUID(str)
- ❌ BIN_AND(val1, val2), BIN_OR(), BIN_XOR(), BIN_NOT(), BIN_SHL(), BIN_SHR()

**Date/Time Functions** (15 functions)
- ✅ CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP
- ✅ NOW() (alias for CURRENT_TIMESTAMP)
- ❌ EXTRACT(part FROM datetime) - YEAR, MONTH, DAY, HOUR, etc.
- ❌ DATEADD(amount, unit, date)
- ❌ DATEDIFF(unit, date1, date2)
- ❌ CAST(str AS DATE), CAST(str AS TIME), CAST(str AS TIMESTAMP)
- ❌ LOCALTIME, LOCALTIMESTAMP
- ❌ DATEPART(part, date) (alias for EXTRACT)
- ❌ DATE_TRUNC(unit, timestamp)
- ❌ MAKE_DATE(year, month, day)
- ❌ MAKE_TIME(hour, minute, second)
- ❌ MAKE_TIMESTAMP(year, month, day, hour, minute, second)
- ❌ AGE(timestamp1, timestamp2)
- ❌ ISDATE(str), ISTIME(str), ISTIMESTAMP(str)

**Math Functions** (18 functions)
- ❌ ABS(x), SIGN(x)
- ❌ CEIL(x), CEILING(x), FLOOR(x)
- ❌ ROUND(x [, precision]), TRUNC(x [, precision])
- ❌ SQRT(x), POWER(x, y), EXP(x)
- ❌ LN(x), LOG(base, x), LOG10(x)
- ❌ SIN(x), COS(x), TAN(x)
- ❌ ASIN(x), ACOS(x), ATAN(x), ATAN2(y, x)
- ❌ SINH(x), COSH(x), TANH(x)
- ❌ PI(), RAND(), RANDOM()
- ❌ MOD(x, y)
- ❌ DIV(x, y) - integer division

**Conversion Functions** (5 functions)
- ✅ CAST(expr AS type)
- ⚠️ TRY_CAST(expr AS type) - exists, needs NULL on failure
- ❌ CONVERT(type, expr)
- ❌ TO_CHAR(value, format)
- ❌ TO_DATE(str, format), TO_TIMESTAMP(str, format)

**Conditional Functions** (4 functions)
- ❌ NULLIF(expr1, expr2)
- ❌ COALESCE(val1, val2, ...)
- ❌ IIF(condition, true_val, false_val)
- ❌ CASE WHEN ... THEN ... ELSE ... END

**System/Context Functions** (12 functions)
- ❌ CURRENT_USER, CURRENT_ROLE
- ❌ CURRENT_CONNECTION, CURRENT_TRANSACTION
- ❌ RDB$GET_CONTEXT(namespace, variable)
- ❌ RDB$SET_CONTEXT(namespace, variable, value)
- ❌ GEN_UUID() - generate UUIDv4
- ❌ INSERTING, UPDATING, DELETING (trigger context)
- ❌ NEW.*, OLD.* (trigger context variables)
- ❌ ROW_COUNT (number of rows affected)
- ❌ SQLCODE, SQLSTATE, GDSCODE
- ❌ RDB$ERROR(code) - error message lookup

---

### 1.5 System Tables and Metadata

#### CRITICAL Priority (15 tables)

**Core Catalog**
- ✅ RDB$DATABASE (database header info) - exists as monitoring table MON_DATABASE
- ✅ RDB$RELATIONS (tables/views) - exists as pg_class in catalog
- ✅ RDB$RELATION_FIELDS (columns) - exists as pg_attribute
- ✅ RDB$INDICES (indexes) - exists as pg_index
- ❌ RDB$INDEX_SEGMENTS (index columns)
- ⚠️ RDB$FIELDS (domain/type definitions) - partial
- ❌ RDB$GENERATORS (sequences) - structure defined but no operations
- ❌ RDB$TRIGGERS (trigger definitions) - structure defined but no operations
- ❌ RDB$PROCEDURES (stored procedures) - structure defined but no operations
- ❌ RDB$PROCEDURE_PARAMETERS (proc/func parameters)
- ❌ RDB$FUNCTIONS (stored functions) - structure defined but no operations
- ❌ RDB$FUNCTION_ARGUMENTS (function parameters)
- ❌ RDB$EXCEPTIONS (user-defined exceptions)
- ❌ RDB$DEPENDENCIES (object dependencies) - structure not defined
- ❌ RDB$FORMATS (table format versions for schema evolution)

#### HIGH Priority (10 tables)

**Security & Permissions**
- ❌ RDB$USERS (user accounts)
- ❌ RDB$ROLES (database roles)
- ⚠️ RDB$USER_PRIVILEGES (permissions/ACL) - structure defined but no operations
- ❌ RDB$TYPES (enumeration of system types)
- ❌ RDB$COLLATIONS (collation definitions) - structure exists
- ❌ RDB$CHARACTER_SETS (character set definitions) - structure exists

**Transaction & Monitoring**
- ✅ MON$TRANSACTIONS (active transactions) - exists as MON_ACTIVE_TRANSACTIONS
- ✅ MON$ATTACHMENTS (connections) - partial
- ✅ MON$STATEMENTS (active statements) - partial
- ✅ MON$IO_STATS (I/O statistics) - partial

---

### 1.6 Transaction Control

#### CRITICAL Priority (all exist, verify completeness)

**Transaction Management**
- ✅ SET TRANSACTION (isolation level, READ ONLY/WRITE, WAIT/NO WAIT, LOCK TIMEOUT)
- ✅ START TRANSACTION (with parameters)
- ✅ COMMIT [WORK] [RETAIN]
- ✅ ROLLBACK [WORK] [RETAIN]
- ⚠️ SAVEPOINT name - implemented in connection_context.cpp, needs SQL exposure
- ⚠️ RELEASE SAVEPOINT name - implemented in connection_context.cpp, needs SQL exposure
- ⚠️ ROLLBACK TO SAVEPOINT name - implemented in connection_context.cpp, needs SQL exposure

**Isolation Levels** (all 4 exist)
- ✅ READ UNCOMMITTED (snapshot isolation)
- ✅ READ COMMITTED [READ CONSISTENCY | RECORD VERSION]
- ✅ REPEATABLE READ (snapshot isolation)
- ✅ SERIALIZABLE

**Table Reservation** (exists, verify)
- ✅ RESERVING table [FOR SHARED READ/WRITE | PROTECTED READ/WRITE]

---

## 2. CURRENT IMPLEMENTATION STATUS

### 2.1 Parser (src/parser/)

**Implemented:**
- ✅ CREATE TABLE (basic)
- ✅ INSERT INTO ... VALUES (single row)
- ✅ SELECT [columns] FROM table [WHERE]
- ✅ START TRANSACTION, SET TRANSACTION, COMMIT, ROLLBACK
- ✅ SWEEP DATABASE
- ✅ Expression parsing (operators, function calls, CAST, LIKE/ILIKE)

**Missing (89 CRITICAL features):**
- ❌ ALTER TABLE, DROP TABLE, TRUNCATE TABLE
- ❌ CREATE INDEX, DROP INDEX
- ❌ CREATE VIEW, DROP VIEW
- ❌ CREATE DOMAIN, ALTER DOMAIN, DROP DOMAIN
- ❌ CREATE SEQUENCE, ALTER SEQUENCE, DROP SEQUENCE, NEXTVAL, GEN_ID
- ❌ CREATE TRIGGER, ALTER TRIGGER, DROP TRIGGER
- ❌ CREATE PROCEDURE, CREATE FUNCTION, DROP PROCEDURE, DROP FUNCTION
- ❌ UPDATE, DELETE
- ❌ MERGE, UPDATE OR INSERT
- ❌ Multi-row INSERT, INSERT ... SELECT
- ❌ JOIN (all types)
- ❌ Subqueries
- ❌ GROUP BY, HAVING
- ❌ ORDER BY, DISTINCT, LIMIT/OFFSET
- ❌ UNION, INTERSECT, EXCEPT
- ❌ WITH (CTEs), WITH RECURSIVE
- ❌ Window functions (OVER clause)
- ❌ FOR UPDATE / WITH LOCK
- ❌ RETURNING clause
- ❌ IF/THEN/ELSE, WHILE, FOR loops
- ❌ DECLARE variables, SET, SELECT INTO
- ❌ Cursors (DECLARE, OPEN, FETCH, CLOSE)
- ❌ Exception handling (BEGIN...WHEN...END, EXCEPTION)
- ❌ SAVEPOINT, ROLLBACK TO SAVEPOINT, RELEASE SAVEPOINT (SQL exposure)

### 2.2 SBLR Bytecode Generator (src/sblr/)

**Implemented:**
- ✅ CREATE TABLE bytecode generation
- ✅ INSERT bytecode generation
- ✅ SELECT bytecode generation (simple queries)
- ✅ Transaction control bytecode
- ✅ Expression compilation (all operators, function calls, CAST)

**Missing Opcodes/Generation:**
- ❌ All DDL beyond CREATE TABLE
- ❌ UPDATE, DELETE bytecode
- ❌ MERGE bytecode
- ❌ JOIN bytecode (no opcodes defined)
- ❌ Subquery bytecode
- ❌ GROUP BY, ORDER BY bytecode
- ❌ Aggregate execution context bytecode
- ❌ Window function bytecode
- ❌ CTE bytecode
- ❌ Procedural control flow bytecode (IF, WHILE, FOR)
- ❌ Variable declaration and assignment bytecode
- ❌ Cursor bytecode
- ❌ Exception handling bytecode

### 2.3 SBLR Executor (src/sblr/)

**Implemented:**
- ✅ CREATE TABLE execution (writes to catalog)
- ✅ INSERT execution (tuple serialization, storage engine integration)
- ✅ SELECT execution (heap scan, WHERE filtering, projection)
- ✅ Transaction control execution
- ✅ Expression evaluation (arithmetic, comparisons, string/date functions, CAST)
- ✅ Pattern matching (LIKE, ILIKE)
- ✅ Monitoring table queries (MON_DATABASE, MON_SWEEP, MON_GC, MON_ACTIVE_TRANSACTIONS)

**Missing Execution:**
- ❌ UPDATE, DELETE
- ❌ MERGE
- ❌ Multi-row INSERT, INSERT ... SELECT
- ❌ JOIN algorithms (nested loop, hash join, merge join)
- ❌ Subquery evaluation
- ❌ GROUP BY / aggregation execution
- ❌ HAVING filtering
- ❌ ORDER BY sorting
- ❌ LIMIT/OFFSET
- ❌ DISTINCT deduplication
- ❌ UNION, INTERSECT, EXCEPT
- ❌ CTE materialization
- ❌ Window function execution
- ❌ Index-backed query execution (indexes exist but not used in SELECT)
- ❌ FOR UPDATE row locking
- ❌ Procedural execution (IF, loops, variables, cursors, exceptions)

### 2.4 Storage Engine API (src/core/storage_engine.cpp)

**Implemented:**
- ✅ insertTuple() - with MVCC
- ✅ getTuple() - fetch by TID
- ✅ deleteTuple() - mark deleted with xmax
- ✅ updateTuple() - MGA back-versioning with version chains
- ✅ createScan(), sequentialScan() - heap scan
- ✅ createIndexScan() - B-tree traversal
- ✅ isVisible() - MVCC visibility

**Missing:**
- ❌ Bulk operations (batch INSERT/UPDATE/DELETE)
- ❌ Constraint enforcement (PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK)
- ❌ Index-backed query execution path (use indexes for WHERE clauses)
- ❌ Schema modification (ALTER TABLE operations)
- ❌ Sequence operations (NEXTVAL, CURRVAL, SETVAL)

### 2.5 Catalog System (src/core/catalog_manager.cpp)

**Implemented:**
- ✅ createSchema(), getSchema(), listSchemas()
- ✅ createTable(), getTable(), listTables()
- ✅ getColumns(), getColumn()
- ✅ createIndex(), getIndex(), listIndexesForTable()
- ⚠️ createCharset(), createCollation() - basic write, no read operations
- ✅ In-memory caches (schema_cache_, table_cache_, column_cache_, index_cache_)

**Missing:**
- ❌ ALTER operations (schema, table, domain, etc.)
- ❌ DROP operations (except deleteTimezone)
- ❌ createConstraint(), getConstraint(), dropConstraint() (PRIMARY KEY, FOREIGN KEY, etc.)
- ❌ createSequence(), getSequence(), alterSequence(), dropSequence()
- ❌ createView(), getView(), dropView()
- ❌ createTrigger(), getTrigger(), alterTrigger(), dropTrigger()
- ❌ createProcedure(), getProcedure(), dropProcedure()
- ❌ createFunction(), getFunction(), dropFunction()
- ❌ createException(), getException(), dropException()
- ❌ createDomain(), getDomain(), alterDomain(), dropDomain()
- ❌ Dependency tracking (pg_depend equivalent)
- ❌ Permission/ACL management
- ❌ Statistics management

---

## 3. GAP ANALYSIS SUMMARY

### 3.1 Critical Gaps (Must Fix for Alpha)

| Category | Features Missing | Complexity | Est. Effort |
|----------|------------------|------------|-------------|
| **DDL Parsing** | 37 statements | Medium | 3-4 weeks |
| **DML Parsing** | 15 statements | Medium | 2-3 weeks |
| **PSQL Parsing** | 20 constructs | High | 3-4 weeks |
| **DDL Bytecode** | 37 opcodes | Low-Medium | 2 weeks |
| **DML Bytecode** | 15 opcodes | Medium | 2 weeks |
| **PSQL Bytecode** | 20 opcodes | High | 3 weeks |
| **DDL Execution** | 37 operations | Medium | 3-4 weeks |
| **DML Execution** | 15 operations | High | 4-5 weeks |
| **PSQL Execution** | 20 operations | High | 4-5 weeks |
| **Catalog Operations** | 25 operations | Medium | 3 weeks |
| **Built-in Functions** | 60 functions | Low-Medium | 2-3 weeks |
| **System Tables** | 15 tables | Medium | 2 weeks |
| **Query Optimizer** | Basic cost model | High | 3-4 weeks |
| **Index Query Path** | Use indexes in SELECT | Medium | 2 weeks |
| **Constraint Enforcement** | PK, FK, UNIQUE, CHECK | Medium | 2-3 weeks |

**Total Estimated Effort**: 40-53 weeks (conservatively 12-16 months for one developer)

**With Focused 3-Person Team**: 4-5 months for core features, 6-8 months for complete Alpha

---

## 4. PRIORITIZED FEATURE LIST

### Phase 1: Core DML Execution (4-6 weeks)

**Goal**: Make UPDATE, DELETE, and basic JOIN work

1. **UPDATE Statement** (CRITICAL)
   - Parser: UPDATE table SET col=val WHERE condition
   - Bytecode: OP_UPDATE opcode
   - Executor: Use storage_engine.updateTuple() with visibility checks
   - Estimated: 1 week

2. **DELETE Statement** (CRITICAL)
   - Parser: DELETE FROM table WHERE condition
   - Bytecode: OP_DELETE opcode
   - Executor: Use storage_engine.deleteTuple() with visibility checks
   - Estimated: 1 week

3. **INNER JOIN** (CRITICAL)
   - Parser: JOIN syntax with ON clause
   - Bytecode: OP_NESTED_LOOP_JOIN opcode
   - Executor: Nested loop join algorithm
   - Estimated: 1.5 weeks

4. **LEFT JOIN** (CRITICAL)
   - Parser: LEFT JOIN syntax
   - Bytecode: Extend join opcode with null-generation flag
   - Executor: Outer join null generation
   - Estimated: 1 week

5. **ORDER BY** (CRITICAL)
   - Parser: ORDER BY col [ASC|DESC] [NULLS FIRST|LAST]
   - Bytecode: OP_SORT opcode with comparators
   - Executor: In-memory sort using std::sort with custom comparators
   - Estimated: 1 week

6. **LIMIT / OFFSET** (CRITICAL)
   - Parser: LIMIT n OFFSET m (or ROWS n TO m in Firebird)
   - Bytecode: OP_LIMIT opcode
   - Executor: Skip first OFFSET rows, return up to LIMIT rows
   - Estimated: 0.5 weeks

**Total Phase 1**: 6 weeks

---

### Phase 2: Aggregation & Grouping (3-4 weeks)

**Goal**: Make GROUP BY and aggregate functions operational

1. **GROUP BY** (CRITICAL)
   - Parser: GROUP BY col1, col2, ...
   - Bytecode: OP_GROUP_BY opcode with grouping keys
   - Executor: Hash-based grouping algorithm
   - Estimated: 1.5 weeks

2. **Aggregate Function Execution** (CRITICAL)
   - Executor: Implement SUM, AVG, MIN, MAX, COUNT execution
   - Bytecode: Mark aggregate functions in expression trees
   - Estimated: 1 week

3. **HAVING Clause** (CRITICAL)
   - Parser: HAVING condition
   - Bytecode: OP_HAVING opcode (filter after aggregation)
   - Executor: Post-aggregation filtering
   - Estimated: 0.5 weeks

4. **DISTINCT** (CRITICAL)
   - Parser: SELECT DISTINCT
   - Bytecode: OP_DISTINCT opcode
   - Executor: Hash-based deduplication
   - Estimated: 0.5 weeks

**Total Phase 2**: 3.5 weeks

---

### Phase 3: Subqueries & Set Operations (3-4 weeks)

**Goal**: Support subqueries and set operations

1. **Scalar Subqueries** (CRITICAL)
   - Parser: SELECT (SELECT ...) FROM ...
   - Bytecode: OP_SUBQUERY_SCALAR opcode
   - Executor: Execute subquery, return single value
   - Estimated: 1 week

2. **IN Subqueries** (CRITICAL)
   - Parser: WHERE col IN (SELECT ...)
   - Bytecode: OP_IN_SUBQUERY opcode
   - Executor: Execute subquery, build hash set, check membership
   - Estimated: 1 week

3. **EXISTS Subqueries** (CRITICAL)
   - Parser: WHERE EXISTS (SELECT ...)
   - Bytecode: OP_EXISTS_SUBQUERY opcode
   - Executor: Execute subquery, return boolean on first match
   - Estimated: 0.5 weeks

4. **UNION / INTERSECT / EXCEPT** (HIGH)
   - Parser: SELECT ... UNION [ALL] SELECT ...
   - Bytecode: OP_UNION, OP_INTERSECT, OP_EXCEPT opcodes
   - Executor: Set operations using hash sets
   - Estimated: 1.5 weeks

**Total Phase 3**: 4 weeks

---

### Phase 4: DDL Expansion (4-5 weeks)

**Goal**: Complete CREATE/ALTER/DROP for all object types

1. **ALTER TABLE** (CRITICAL)
   - Parser: ALTER TABLE ... ADD COLUMN, DROP COLUMN, ALTER COLUMN, RENAME
   - Bytecode: OP_ALTER_TABLE opcode with subcommands
   - Executor: Schema modification in catalog, table rewrite if needed
   - Estimated: 2 weeks

2. **DROP TABLE / INDEX / VIEW** (CRITICAL)
   - Parser: DROP TABLE [IF EXISTS] [CASCADE|RESTRICT]
   - Bytecode: OP_DROP_TABLE, OP_DROP_INDEX, OP_DROP_VIEW
   - Executor: Delete from catalog, free pages, handle dependencies
   - Estimated: 1 week

3. **CREATE INDEX** (CRITICAL)
   - Parser: CREATE [UNIQUE] INDEX ... ON table (cols) [ASC|DESC]
   - Bytecode: OP_CREATE_INDEX opcode
   - Executor: Call catalog.createIndex(), build index from heap scan
   - Estimated: 1 week

4. **CREATE VIEW** (CRITICAL)
   - Parser: CREATE VIEW ... AS SELECT ...
   - Bytecode: OP_CREATE_VIEW opcode
   - Executor: Store view definition in catalog, validate query
   - Estimated: 0.5 weeks

5. **CREATE/ALTER/DROP SEQUENCE** (CRITICAL)
   - Parser: CREATE SEQUENCE, ALTER SEQUENCE, DROP SEQUENCE, NEXT VALUE FOR, GEN_ID
   - Bytecode: OP_CREATE_SEQUENCE, OP_NEXTVAL, OP_GEN_ID
   - Executor: Catalog operations, atomic increment
   - Estimated: 1 week

**Total Phase 4**: 5.5 weeks

---

### Phase 5: Constraints & Indexes (3-4 weeks)

**Goal**: Enforce PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK constraints; use indexes in queries

1. **PRIMARY KEY Enforcement** (CRITICAL)
   - Catalog: Add constraint metadata to pg_constraint
   - Executor: Check uniqueness on INSERT/UPDATE using B-tree index
   - Estimated: 1 week

2. **UNIQUE Constraint Enforcement** (CRITICAL)
   - Catalog: Register UNIQUE constraints
   - Executor: Check uniqueness using B-tree index
   - Estimated: 0.5 weeks

3. **FOREIGN KEY Enforcement** (HIGH)
   - Catalog: Register FK constraints with referenced table/columns
   - Executor: Check referential integrity on INSERT/UPDATE/DELETE
   - Estimated: 1.5 weeks

4. **CHECK Constraint Enforcement** (MEDIUM)
   - Catalog: Store CHECK expression in catalog
   - Executor: Evaluate CHECK expression on INSERT/UPDATE
   - Estimated: 0.5 weeks

5. **Index-Backed Query Execution** (CRITICAL)
   - Planner: Choose index scan over heap scan when WHERE clause matches index
   - Executor: Use storage_engine.createIndexScan() instead of sequentialScan()
   - Estimated: 1.5 weeks

**Total Phase 5**: 5 weeks

---

### Phase 6: PSQL Basics (5-6 weeks)

**Goal**: Implement core procedural language features

1. **Variables (DECLARE, SET, SELECT INTO)** (CRITICAL)
   - Parser: DECLARE @var type; SET @var = expr; SELECT col INTO @var FROM ...
   - Bytecode: OP_DECLARE_VAR, OP_SET_VAR, OP_SELECT_INTO
   - Executor: Symbol table for variables, assignment operations
   - Estimated: 1.5 weeks

2. **IF/THEN/ELSE** (CRITICAL)
   - Parser: IF condition THEN ... [ELSIF ...] [ELSE ...] END IF
   - Bytecode: OP_IF_THEN, OP_ELSIF, OP_ELSE, OP_END_IF with jump offsets
   - Executor: Conditional branching
   - Estimated: 1 week

3. **WHILE Loop** (CRITICAL)
   - Parser: WHILE condition DO ... END WHILE
   - Bytecode: OP_WHILE, OP_END_WHILE with jump offsets
   - Executor: Loop with condition check
   - Estimated: 0.5 weeks

4. **FOR Loop** (CRITICAL)
   - Parser: FOR i IN start..end DO ... END FOR
   - Bytecode: OP_FOR_RANGE, OP_END_FOR
   - Executor: Integer iteration
   - Estimated: 0.5 weeks

5. **FOR SELECT Loop** (HIGH)
   - Parser: FOR @rec IN SELECT ... DO ... END FOR
   - Bytecode: OP_FOR_SELECT, OP_END_FOR_SELECT
   - Executor: Cursor iteration over query result
   - Estimated: 1 week

6. **Exception Handling** (HIGH)
   - Parser: BEGIN ... WHEN exception_name THEN ... WHEN OTHERS THEN ... END
   - Bytecode: OP_TRY_BEGIN, OP_CATCH, OP_END_TRY with exception handlers
   - Executor: Exception stack, error propagation
   - Estimated: 1.5 weeks

**Total Phase 6**: 6 weeks

---

### Phase 7: Stored Procedures & Triggers (4-5 weeks)

**Goal**: CREATE PROCEDURE, CREATE FUNCTION, CREATE TRIGGER with execution

1. **CREATE PROCEDURE** (HIGH)
   - Parser: CREATE PROCEDURE name (params) AS BEGIN ... END
   - Catalog: Store procedure definition, parameters, SBLR bytecode
   - Executor: CALL procedure execution
   - Estimated: 2 weeks

2. **CREATE FUNCTION** (HIGH)
   - Parser: CREATE FUNCTION name (params) RETURNS type AS BEGIN ... RETURN expr; END
   - Catalog: Store function definition, return type, SBLR bytecode
   - Executor: Function call in expressions
   - Estimated: 1.5 weeks

3. **CREATE TRIGGER** (HIGH)
   - Parser: CREATE TRIGGER name FOR table BEFORE/AFTER INSERT/UPDATE/DELETE AS BEGIN ... END
   - Catalog: Store trigger definition, timing, events, SBLR bytecode
   - Executor: Trigger invocation during DML operations, NEW/OLD context variables
   - Estimated: 2 weeks

**Total Phase 7**: 5.5 weeks

---

### Phase 8: Advanced Features (3-4 weeks)

**Goal**: CTEs, window functions, advanced SQL

1. **Common Table Expressions (WITH)** (HIGH)
   - Parser: WITH cte_name AS (SELECT ...) SELECT ... FROM cte_name
   - Bytecode: OP_WITH_CTE, OP_CTE_SCAN
   - Executor: Materialize CTE into temp table, scan
   - Estimated: 1.5 weeks

2. **Recursive CTEs** (MEDIUM)
   - Parser: WITH RECURSIVE cte_name AS (base UNION ALL recursive)
   - Executor: Iterative evaluation until fixpoint
   - Estimated: 1.5 weeks

3. **Window Functions (Basic)** (MEDIUM)
   - Parser: OVER (PARTITION BY ... ORDER BY ...)
   - Bytecode: OP_WINDOW_FUNCTION opcode
   - Executor: ROW_NUMBER(), RANK(), DENSE_RANK() implementation
   - Estimated: 2 weeks

**Total Phase 8**: 5 weeks

---

### Phase 9: Built-in Functions (2-3 weeks)

**Goal**: Implement missing Firebird built-in functions

1. **Math Functions** (MEDIUM)
   - Functions: ABS, SIGN, CEIL, FLOOR, ROUND, TRUNC, SQRT, POWER, EXP, LN, LOG, SIN, COS, TAN, etc.
   - Executor: Add function implementations to function registry
   - Estimated: 1 week

2. **String Functions** (MEDIUM)
   - Functions: POSITION, LPAD, RPAD, LEFT, RIGHT, REVERSE, REPLACE, OVERLAY, HASH, etc.
   - Estimated: 1 week

3. **Date/Time Functions** (MEDIUM)
   - Functions: EXTRACT, DATEADD, DATEDIFF, DATE_TRUNC, MAKE_DATE, AGE, etc.
   - Estimated: 1 week

4. **Conditional & System Functions** (MEDIUM)
   - Functions: NULLIF, COALESCE, IIF, CURRENT_USER, CURRENT_ROLE, RDB$GET_CONTEXT, GEN_UUID, etc.
   - Estimated: 0.5 weeks

**Total Phase 9**: 3.5 weeks

---

### Phase 10: System Tables & Metadata (2-3 weeks)

**Goal**: Complete RDB$ system tables and information_schema

1. **Remaining RDB$ Tables** (MEDIUM)
   - Implement: RDB$INDEX_SEGMENTS, RDB$GENERATORS, RDB$TRIGGERS, RDB$PROCEDURES, RDB$FUNCTIONS, etc.
   - Catalog operations for each
   - Estimated: 1.5 weeks

2. **information_schema Views** (LOW)
   - Create standard information_schema views (TABLES, COLUMNS, CONSTRAINTS, etc.)
   - Map to internal catalog
   - Estimated: 1 week

**Total Phase 10**: 2.5 weeks

---

### Phase 11: Query Optimizer (3-4 weeks)

**Goal**: Basic cost-based query optimization

1. **Statistics Collection** (MEDIUM)
   - Implement ANALYZE command to collect table/index statistics
   - Store in pg_statistic equivalent
   - Estimated: 1 week

2. **Cost Model** (MEDIUM)
   - Implement cost estimation for heap scan, index scan, join algorithms
   - Choose lowest-cost plan
   - Estimated: 1.5 weeks

3. **Join Reordering** (LOW)
   - Implement basic join order optimization (dynamic programming for small queries)
   - Estimated: 1 week

**Total Phase 11**: 3.5 weeks

---

### Phase 12: Embedded Engine Integration (2-3 weeks)

**Goal**: Create embeddable library and CLI tool

1. **Embedded API** (HIGH)
   - Create C API wrapper around C++ engine
   - Functions: scratchbird_open(), scratchbird_execute(), scratchbird_fetch(), scratchbird_close()
   - Estimated: 1 week

2. **CLI Tool (isql-compatible)** (HIGH)
   - Create command-line interface using embedded API
   - Commands: CONNECT, INPUT, SHOW TABLES, EXIT
   - Interactive and batch modes
   - Estimated: 1.5 weeks

**Total Phase 12**: 2.5 weeks

---

## 5. TOTAL EFFORT ESTIMATION

### By Phase

| Phase | Focus | Duration (Weeks) | Cumulative |
|-------|-------|------------------|------------|
| 1 | Core DML (UPDATE, DELETE, JOIN, ORDER BY, LIMIT) | 6 | 6 |
| 2 | Aggregation (GROUP BY, aggregates, HAVING, DISTINCT) | 3.5 | 9.5 |
| 3 | Subqueries & Set Operations | 4 | 13.5 |
| 4 | DDL Expansion (ALTER, DROP, CREATE INDEX/VIEW/SEQUENCE) | 5.5 | 19 |
| 5 | Constraints & Index Queries | 5 | 24 |
| 6 | PSQL Basics (variables, control flow, exceptions) | 6 | 30 |
| 7 | Stored Procedures, Functions, Triggers | 5.5 | 35.5 |
| 8 | Advanced SQL (CTEs, window functions) | 5 | 40.5 |
| 9 | Built-in Functions | 3.5 | 44 |
| 10 | System Tables & Metadata | 2.5 | 46.5 |
| 11 | Query Optimizer | 3.5 | 50 |
| 12 | Embedded Engine & CLI | 2.5 | 52.5 |

**Total Estimated Effort**: 52.5 weeks (1 year + 3 months for one full-time developer)

### Realistic Timeline Scenarios

**Scenario 1: Solo Developer (Full-Time)**
- **Duration**: 12-14 months
- **Phases**: Sequential execution
- **Risk**: High (single point of failure, feature creep, burnout)

**Scenario 2: 2-Person Team (Full-Time)**
- **Duration**: 7-9 months
- **Parallelization**: Parser/Bytecode work in parallel with Executor work
- **Risk**: Medium (code review overhead, integration complexity)

**Scenario 3: 3-Person Team (Full-Time)**
- **Duration**: 5-7 months
- **Division**: Person 1 (Parser), Person 2 (Bytecode/Executor), Person 3 (Catalog/Functions)
- **Risk**: Low (good parallelization, manageable integration)

**Scenario 4: Part-Time (Nights/Weekends)**
- **Duration**: 24-32 months (2-2.5 years)
- **Velocity**: ~50% of full-time
- **Risk**: Very high (motivation, context switching, incomplete features)

---

## 6. DEPENDENCIES AND CRITICAL PATH

### Critical Path (No Parallelization)

```
Phase 1 (DML) → Phase 2 (Aggregation) → Phase 3 (Subqueries) →
Phase 4 (DDL) → Phase 5 (Constraints) → Phase 6 (PSQL) →
Phase 7 (Procs/Triggers) → Phase 8 (Advanced SQL) →
Phase 9 (Functions) → Phase 10 (Metadata) → Phase 11 (Optimizer) →
Phase 12 (Embedded)
```

**Duration**: 52.5 weeks

### Parallel Execution (3-Person Team)

**Track A (Parser Lead):**
- Weeks 1-6: Phase 1 (DML parsing)
- Weeks 7-10: Phase 2 (Aggregation parsing)
- Weeks 11-14: Phase 3 (Subquery parsing)
- Weeks 15-20: Phase 4 (DDL parsing)
- Weeks 21-26: Phase 6 (PSQL parsing)
- Weeks 27-32: Phase 7 (Procedure/Trigger parsing)
- Weeks 33-37: Phase 8 (Advanced SQL parsing)

**Track B (Executor Lead):**
- Weeks 1-6: Phase 1 (DML execution)
- Weeks 7-10: Phase 2 (Aggregation execution)
- Weeks 11-14: Phase 3 (Subquery execution)
- Weeks 15-19: Phase 5 (Constraint enforcement)
- Weeks 20-25: Phase 6 (PSQL execution)
- Weeks 26-31: Phase 7 (Procedure/Trigger execution)
- Weeks 32-36: Phase 8 (CTE/Window execution)

**Track C (Catalog/Functions Lead):**
- Weeks 1-5: Phase 4 (Catalog DDL operations)
- Weeks 6-9: Phase 9 (Built-in functions)
- Weeks 10-12: Phase 10 (System tables)
- Weeks 13-16: Phase 11 (Query optimizer)
- Weeks 17-19: Phase 12 (Embedded API & CLI)
- Weeks 20-36: Support Track A & B with integration, testing, bug fixes

**Total Duration (Parallel)**: 37 weeks (~9 months with 3-person team)

---

## 7. RISKS AND MITIGATION

### High-Risk Items

1. **Query Optimizer Complexity** (Phase 11)
   - **Risk**: Cost model inaccuracies, join reordering bugs
   - **Mitigation**: Start with simple heuristics, defer advanced optimization to Beta

2. **Window Functions** (Phase 8)
   - **Risk**: Complex framing logic, performance issues
   - **Mitigation**: Implement basic functions first (ROW_NUMBER, RANK), defer advanced framing

3. **Foreign Key Enforcement** (Phase 5)
   - **Risk**: Cascading deletes/updates complexity, deadlock risks
   - **Mitigation**: Implement RESTRICT first, defer CASCADE to Beta

4. **Recursive CTEs** (Phase 8)
   - **Risk**: Infinite loops, performance issues
   - **Mitigation**: Add cycle detection, max recursion depth limit

5. **Trigger Execution** (Phase 7)
   - **Risk**: Infinite trigger chains, rollback complexity
   - **Mitigation**: Add max trigger depth limit, careful transaction handling

### Medium-Risk Items

1. **ALTER TABLE** (Phase 4)
   - **Risk**: Table rewrite for incompatible schema changes, data migration
   - **Mitigation**: Support simple cases first (ADD COLUMN at end), defer complex rewrites

2. **Exception Handling** (Phase 6)
   - **Risk**: Exception propagation through call stack, resource cleanup
   - **Mitigation**: Use C++ exceptions internally, careful RAII

3. **Subquery Correlation** (Phase 3)
   - **Risk**: Performance issues with nested loop execution
   - **Mitigation**: Document performance characteristics, defer optimization

---

## 8. TESTING STRATEGY

### Unit Tests (Per Phase)

- **Parser Tests**: Verify AST generation for each new statement
- **Bytecode Tests**: Verify correct opcode sequences
- **Executor Tests**: Verify correct results for each operation
- **Integration Tests**: End-to-end SQL execution

### Regression Tests

- **Test Suite**: Accumulate tests from each phase
- **CI Integration**: Run all tests on every commit
- **Coverage Target**: 80% code coverage

### Firebird Compatibility Tests

- **Test Suite**: Port Firebird test cases to ScratchBird
- **Verification**: Cross-check results against Firebird 5.0
- **Goal**: 95%+ compatibility for implemented features

---

## 9. SUCCESS CRITERIA FOR ALPHA

### Minimum Viable Alpha (MVP)

1. ✅ **DDL**: CREATE/ALTER/DROP for TABLE, INDEX, VIEW, SEQUENCE
2. ✅ **DML**: INSERT, UPDATE, DELETE, SELECT with JOIN, subqueries, GROUP BY, ORDER BY
3. ✅ **PSQL**: Variables, IF/WHILE/FOR, exception handling
4. ✅ **Procedures**: CREATE PROCEDURE, EXECUTE PROCEDURE
5. ✅ **Functions**: CREATE FUNCTION, use in expressions
6. ✅ **Triggers**: CREATE TRIGGER, fire on DML
7. ✅ **Constraints**: PRIMARY KEY, FOREIGN KEY, UNIQUE, CHECK enforcement
8. ✅ **Indexes**: Used in query execution
9. ✅ **Built-in Functions**: Core string, math, date, aggregate functions
10. ✅ **System Tables**: RDB$ tables for metadata access
11. ✅ **Embedded Engine**: C API + CLI tool (isql-compatible)

### Full Alpha (Feature Complete)

- All MVP features PLUS:
- ✅ **Advanced SQL**: CTEs, window functions, MERGE
- ✅ **Query Optimizer**: Basic cost-based optimization
- ✅ **Monitoring**: Complete MON$ tables
- ✅ **Packages**: CREATE PACKAGE support (if time allows)
- ✅ **80%+ Test Coverage**: Comprehensive test suite
- ✅ **Documentation**: User guide, API reference

---

## 10. RECOMMENDATIONS

### Immediate Next Steps (Week 1)

1. **Set Up Project Tracking**
   - Create GitHub project board with all 227 features as issues
   - Organize into 12 phases (milestones)
   - Assign priorities (CRITICAL/HIGH/MEDIUM/LOW)

2. **Establish Development Workflow**
   - Feature branches for each major task
   - Pull request reviews (if team > 1)
   - CI/CD pipeline with automated tests

3. **Start Phase 1: Core DML**
   - Begin with UPDATE statement (parser → bytecode → executor)
   - Target: Working UPDATE within 1 week

### Resource Allocation

**If Solo Developer:**
- Focus on CRITICAL features only (Phases 1-5, 6, 7)
- Defer MEDIUM/LOW features to Beta
- Timeline: 12-14 months

**If 2-Person Team:**
- Person 1: Parser + Bytecode (frontend)
- Person 2: Executor + Catalog (backend)
- Timeline: 7-9 months

**If 3-Person Team:**
- Person 1: Parser (all phases)
- Person 2: Executor (all phases)
- Person 3: Catalog, Functions, Optimizer, Embedded API
- Timeline: 5-7 months (recommended)

### Quality Gates

**After Each Phase:**
1. All phase features pass unit tests
2. Integration tests demonstrate end-to-end functionality
3. Code review completed (if team > 1)
4. Documentation updated

**Before Alpha Release:**
1. 80%+ test coverage
2. All CRITICAL and HIGH features implemented
3. Firebird compatibility test suite passes (95%+ for implemented features)
4. CLI tool demonstrates all features
5. User guide and API reference complete

---

## 11. CONCLUSION

ScratchBird has an **exceptional storage engine foundation** but requires **significant SQL layer development** to reach Alpha maturity. With **227 features** across **12 phases**, the project represents **52.5 weeks of focused development** for a solo developer or **5-7 months for a 3-person team**.

The recommended approach is to:
1. **Prioritize CRITICAL features** (Phases 1-7) for a Minimum Viable Alpha
2. **Defer MEDIUM/LOW features** to Beta or later releases
3. **Parallelize work** across Parser, Executor, and Catalog tracks if team size allows
4. **Maintain quality** with comprehensive testing and CI/CD

Upon completion, ScratchBird will be a **fully functional, Firebird-compatible embedded database** suitable for educational use, application embedding, and as a foundation for advanced features in Beta (networking, replication, etc.).

---

**Next Document**: ALPHA_COMPLETION_DETAILED_TODO.md (breaks down all 227 features into actionable tasks)
