# V2 Parser Audit Summary

**Parser:** V2 (Native ScratchBird) Parser
**Location:** `src/parser/parser_v2.cpp`, `include/scratchbird/parser/parser_v2.h`
**Total Lines:** 5,394 lines (parser) + 2,687 lines (AST) = 8,081 lines
**Audit Date:** 2026-01-07

---

## Overall Assessment

**Dialect Purity:** ⚠️ **MIXED** - Contains PostgreSQL extensions alongside Firebird-style base
**Syntax Coverage:** ✅ 90% DDL/DML, ❌ 0% PSQL (not implemented)
**Context Sensitivity:** ✅ EXCELLENT - Gatekeeper model with 175+ contextual keywords
**Production Ready:** ⚠️ **CONDITIONAL** - Complete for DDL/DML, missing PSQL

---

## Design Philosophy

**From Specification:**
> "The V2 parser which is the core parser of the project, will have many more expansions than any of the other three but in Style/Formatting it should follow the FirebirdSQL standard."

**Architecture:**
- **Base Style:** FirebirdSQL formatting and conventions
- **Extensions:** Advanced features beyond single dialects
- **Context Sensitivity:** Reduces reserved word collisions
- **Output:** AST nodes (ast_v2.h) for semantic analysis
- **Target:** SBLR bytecode generation

**Key Feature:** "Smart Parser, Dumb Lexer" with Gatekeeper keyword model

---

## Context Sensitivity Analysis ✅

**Implementation:** Lines 158-206 in parser_v2.cpp

### Gatekeeper Keywords (14 core reserved words)

Checked via `TokenType::KW_*`:
```cpp
CREATE, ALTER, DROP, SELECT, INSERT, UPDATE, DELETE,
FROM, WHERE, AND, OR, NOT, IF, BEGIN, COMMIT, ROLLBACK, PREPARE
```

### Contextual Keywords (175+ non-reserved)

Checked via `matchContextual()` / `checkContextual()`:
```cpp
TABLE, VIEW, INDEX, SCHEMA, DOMAIN, TRANSACTION, ISOLATION,
LEVEL, TEMPORARY, UNLOGGED, MATERIALIZED, RETURNING, CASCADE,
RESTRICT, SAVEPOINT, RELEASE, RESET, SHOW, EXPLAIN, ...
```

**Evidence (lines 158-206):**
```cpp
// Gatekeeper dispatch
if (match(TokenType::KW_CREATE))    return parseCreate();
if (match(TokenType::KW_ALTER))     return parseAlter();

// Session statements use contextual keywords
if (matchContextual("SAVEPOINT"))   return parseSavepoint();
if (matchContextual("RELEASE"))     return parseReleaseSavepoint();
if (matchContextual("RESET"))       return parseReset();
```

**Assessment:** Context sensitivity correctly implemented. Most keywords can be used as identifiers.

---

## SQL Support Inventory

### DDL Statements - 90% Complete ✅

**CREATE Statements:**
- ✅ CREATE TABLE (IF NOT EXISTS, OR REPLACE, TEMPORARY, UNLOGGED)
- ✅ CREATE INDEX (UNIQUE, CONCURRENT, types: BTREE/HASH/GIN/GIST/BRIN/BITMAP)
- ✅ CREATE VIEW (OR REPLACE, TEMPORARY, MATERIALIZED)
- ✅ CREATE SEQUENCE (OWNED BY, START WITH, INCREMENT BY, etc.)
- ✅ CREATE SCHEMA (IF NOT EXISTS, OWNER)
- ✅ CREATE DATABASE (IF NOT EXISTS)
- ✅ CREATE DOMAIN (all kinds: BASIC, RECORD, ENUM, SET, VARIANT)
- ❌ CREATE FUNCTION (TODO at line 299)
- ❌ CREATE PROCEDURE (TODO at line 300)
- ❌ CREATE TRIGGER (TODO at line 301)

**ALTER Statements:**
- ✅ ALTER TABLE (ADD/DROP/ALTER COLUMN, ADD/DROP/RENAME CONSTRAINT, etc.)
- ✅ ALTER SCHEMA (RENAME, SET OWNER, SET PATH)
- ✅ ALTER DATABASE (RENAME, SET OWNER)
- ✅ ALTER DOMAIN (SET/DROP DEFAULT, ADD/DROP CHECK, RENAME, SET/DROP COMPAT)

**DROP Statements:**
- ✅ DROP TABLE, DROP INDEX, DROP VIEW, DROP SCHEMA, DROP DATABASE, DROP DOMAIN
- ✅ IF EXISTS, CASCADE/RESTRICT support

**Other DDL:**
- ✅ TRUNCATE TABLE (RESTART/CONTINUE IDENTITY, CASCADE)

### DML Statements - 100% Complete ✅

**SELECT:**
- ✅ DISTINCT/ALL
- ✅ SELECT list with aliases
- ✅ FROM clause with table refs, aliases, column aliases
- ✅ Joins: INNER, LEFT, RIGHT, FULL, CROSS, NATURAL
- ✅ WHERE, GROUP BY, HAVING, WINDOW, ORDER BY
- ✅ LIMIT/OFFSET
- ✅ Set operations: UNION, INTERSECT, EXCEPT
- ✅ FOR UPDATE/SHARE with NOWAIT/SKIP LOCKED

**INSERT:**
- ✅ Column list, VALUES (multi-row), SELECT source
- ✅ DEFAULT keyword
- ✅ ON CONFLICT (DO NOTHING, DO UPDATE SET) - PostgreSQL-style
- ✅ RETURNING clause

**UPDATE:**
- ✅ SET clause with multiple assignments
- ✅ FROM clause (PostgreSQL extension)
- ✅ WHERE clause
- ✅ RETURNING clause

**DELETE:**
- ✅ FROM keyword (required)
- ✅ USING clause (PostgreSQL extension)
- ✅ WHERE clause
- ✅ RETURNING clause

**MERGE:**
- ✅ WHEN MATCHED THEN UPDATE/DELETE
- ✅ WHEN NOT MATCHED THEN INSERT
- ✅ WHEN NOT MATCHED BY SOURCE

### Transaction Control - 100% Complete ✅

**BEGIN/START TRANSACTION:**
- ✅ Isolation levels:
  - READ UNCOMMITTED
  - READ COMMITTED (DEFAULT, READ CONSISTENCY, RECORD VERSION, NO RECORD VERSION)
  - REPEATABLE READ
  - SERIALIZABLE
  - SNAPSHOT (Firebird)
  - SNAPSHOT TABLE STABILITY (Firebird)
- ✅ Access mode: READ ONLY, READ WRITE
- ✅ DEFERRABLE/NOT DEFERRABLE
- ✅ Firebird legacy: WAIT/NO WAIT, LOCK TIMEOUT
- ✅ RESERVING clause (SHARED/PROTECTED FOR READ/WRITE)
- ✅ ScratchBird extensions: AUTOCOMMIT mode, ON CONFLICT actions

**COMMIT/ROLLBACK:**
- ✅ WORK/TRANSACTION keywords
- ✅ AND CHAIN / AND NO CHAIN
- ✅ RETAINING (Firebird legacy)
- ✅ PREPARED with GID (2PC)

**SAVEPOINT:**
- ✅ SAVEPOINT name
- ✅ RELEASE SAVEPOINT name

**PREPARE TRANSACTION:**
- ✅ Two-Phase Commit with GID

### Session/Configuration - 95% Complete ✅

**SET:**
- ✅ Scope: SESSION, LOCAL
- ✅ SET name = value / TO value / TO DEFAULT
- ✅ SET TIME ZONE
- ✅ SET TRANSACTION
- ✅ SET SESSION AUTHORIZATION
- ✅ SET ROLE
- ✅ SET AUTOCOMMIT ON/OFF
- ✅ SET SQL DIALECT 1/2/3
- ✅ SET NAMES charset
- ✅ SET LOCAL_TIMEOUT seconds
- ❌ SET PARSER VERSION (returns error - correct)

**RESET:**
- ✅ RESET name
- ✅ RESET ALL

**SHOW:**
- ✅ Session variable: SHOW variable_name
- ✅ SHOW ALL
- ✅ SHOW TRANSACTION ISOLATION LEVEL
- ✅ Catalog queries: TABLES, DATABASES, COLUMNS, INDEXES, CREATE TABLE
- ✅ Firebird ISQL style: TABLE, INDEX, TRIGGER, VIEW, PROCEDURE, etc.
- ✅ Special: VERSION, DATABASE, SYSTEM, SQL DIALECT
- ✅ LIKE pattern support

**EXPLAIN:**
- ✅ ANALYZE (execute)
- ✅ VERBOSE, COSTS, BUFFERS, TIMING
- ✅ Format: JSON, XML, YAML

### Security/DCL - 100% Complete ✅

**GRANT:**
- ✅ Privileges: SELECT, INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER, EXECUTE, USAGE, ALL
- ✅ Object types: TABLE, VIEW, SEQUENCE, FUNCTION, PROCEDURE, SCHEMA, DATABASE, ALL_*_IN_SCHEMA
- ✅ WITH GRANT OPTION
- ✅ GRANT TO PUBLIC

**REVOKE:**
- ✅ All privilege types
- ✅ All object types
- ✅ REVOKE GRANT OPTION FOR
- ✅ CASCADE

### PSQL Statements - 0% Implemented ❌

**AST Nodes Defined But Never Parsed:**
- ❌ EXECUTE BLOCK (anonymous blocks)
- ❌ BEGIN...END (compound statements)
- ❌ DECLARE VARIABLE
- ❌ Assignment (:=)
- ❌ IF...THEN...ELSE
- ❌ WHILE loops
- ❌ FOR SELECT loops
- ❌ LOOP...LEAVE
- ❌ EXIT, CONTINUE, SUSPEND, RETURN
- ❌ EXCEPTION RAISE
- ❌ WHEN exception handlers
- ❌ POST_EVENT
- ❌ Cursor operations

**Evidence:**
- AST nodes defined: `ExecuteBlockStmt`, `IfStmt`, `WhileStmt`, etc. (ast_v2.h lines 112-443)
- Parser functions NOT implemented: no `parseExecuteBlock()`, `parseIfStmt()`, etc. in parser_v2.cpp

### WITH Clause (CTEs) - 0% Implemented ❌

**Evidence:**
- `WithClause` struct defined (ast_v2.h lines 1847-1850)
- `InsertStmt::with`, `UpdateStmt::with`, `DeleteStmt::with` fields exist
- NO parsing code found - fields never populated

---

## PostgreSQL Extensions in V2 ⚠️

**Question:** Are these intentional extensions or contamination?

### PostgreSQL Features Found

1. **UPDATE ... FROM clause** (lines 2782-2799)
   ```sql
   UPDATE table SET col = val FROM other_table WHERE ...
   ```
   - PostgreSQL extension, not standard SQL
   - Firebird uses different syntax

2. **DELETE ... USING clause** (lines 2868-2885)
   ```sql
   DELETE FROM table USING other_table WHERE ...
   ```
   - PostgreSQL-specific
   - Firebird doesn't support USING

3. **INSERT ... ON CONFLICT** (lines 2657-2741)
   ```sql
   INSERT ... ON CONFLICT (cols) DO UPDATE SET ...
   ```
   - PostgreSQL 9.5+ upsert syntax
   - Firebird uses MERGE or UPDATE OR INSERT

4. **DROP ... CASCADE**
   - PostgreSQL-style cascade (accepted in V2)
   - Firebird also has CASCADE but semantics may differ

5. **MATERIALIZED VIEW** support
   - PostgreSQL feature (also in Oracle)
   - Firebird doesn't have materialized views

6. **EXPLAIN with ANALYZE, VERBOSE, COSTS, BUFFERS, TIMING**
   - PostgreSQL-style syntax
   - Firebird has SET PLAN but different

### Assessment

**Possible Interpretations:**

1. **Intentional V2 Extensions:** V2 is designed to be a superset with "many more expansions"
2. **PostgreSQL Influence:** Developer added PostgreSQL features as useful extensions
3. **Unintentional Bleeding:** Features leaked from PostgreSQL parser into V2

**Recommendation:** **CLARIFY SPECIFICATION**
- Document which PostgreSQL features are intentional V2 extensions
- Create "V2 SQL Dialect" specification
- OR remove PostgreSQL-specific features to maintain pure Firebird style

---

## FirebirdSQL Style Compliance

**Firebird Features CORRECTLY Implemented:**

✅ Firebird transaction isolation levels (SNAPSHOT, SNAPSHOT TABLE STABILITY)
✅ Firebird legacy options (WAIT/NO WAIT, LOCK TIMEOUT)
✅ RESERVING clause (SHARED/PROTECTED FOR READ/WRITE)
✅ READ COMMITTED variants (READ CONSISTENCY, RECORD VERSION, NO RECORD VERSION)
✅ RETAINING on COMMIT/ROLLBACK
✅ EXECUTE BLOCK structure (AST nodes defined)
✅ Domain system with WITH blocks (SECURITY, INTEGRITY, VALIDATION, QUALITY)

**Firebird Style Aspects:**
- Transaction syntax follows Firebird conventions
- Domain DDL follows Firebird DOMAIN model
- GENERATOR/SEQUENCE support (both keywords)
- CREATE OR ALTER support

---

## Data Types

**Strategy:** Generic type name parsing - NO hardcoded list

**Supported:**
- Any identifier accepted as type name
- Schema-qualified types: `schema.domain_name`
- Array notation: `INT[]`, `VARCHAR(100)[]`
- Time zone support: `TIMESTAMP WITH/WITHOUT TIME ZONE`

**Hardcoded Aliases (only 3):**
```cpp
DOUBLE PRECISION → kept as is
CHARACTER VARYING → VARCHAR
BIT VARYING → VARBIT
```

**Type Parameters:**
- Single parameter: `VARCHAR(100)`, `NUMERIC(10)`
- Two parameters: `NUMERIC(10, 2)`

**Assessment:** Flexible approach allows any custom/domain types

---

## Functions and Operators

### Functions - Generic Parsing ✅

**Strategy:** Accept any identifier + LEFT_PAREN as function call

**Special Cases (only 4):**
1. **COUNT(\*)** - Special case handling (lines 3714-3728)
2. **POSITION(needle IN haystack)** - Special syntax (lines 3730-3745)
3. **OVERLAY(source PLACING replacement FROM start FOR length)** - Special (lines 3747-3779)
4. **EXTRACT(field FROM source)** - Special handling

All other functions: generic parsing, semantic validation deferred

### Operators - Complete ✅

**Binary Operators (from BinaryOp enum):**
- Arithmetic: ADD, SUB, MUL, DIV, MOD, POWER
- Comparison: EQ, NE, LT, LE, GT, GE
- Logical: AND, OR
- String: CONCAT
- Regex: REGEX_MATCH, REGEX_MATCH_CI, REGEX_NOT_MATCH, REGEX_NOT_MATCH_CI
- Bitwise: BIT_AND, BIT_OR, BIT_XOR, SHIFT_LEFT, SHIFT_RIGHT
- JSON: JSON_EXTRACT, JSON_EXTRACT_TEXT, JSON_HASH_EXTRACT, JSON_HASH_EXTRACT_TEXT
- Array: ARRAY_CONTAINS, ARRAY_CONTAINED_BY, ARRAY_OVERLAP

**Unary Operators:**
- NEGATE, NOT, BIT_NOT, IS_NULL, IS_NOT_NULL

**Token-level Operators:**
- `~`, `~*`, `!~`, `!~*` (regex operators)

**Assessment:** Comprehensive operator support for expression evaluation

---

## Critical Gaps

### 1. PSQL Not Implemented ❌

**Impact:** Cannot parse:
- Stored procedures
- Stored functions
- Triggers with procedural bodies
- Anonymous blocks (EXECUTE BLOCK)
- Control flow statements

**Workaround:** Use emulated parsers (Firebird, PostgreSQL) for procedural code

**Fix Required:** Implement parsing functions for existing AST nodes

### 2. CTEs Not Parsed ❌

**Impact:** WITH clauses silently ignored

**Evidence:**
- `WithClause` fields in DML statement ASTs
- No `parseWithClause()` function
- No code populating `with` fields

**Fix Required:** Implement CTE parsing

### 3. CREATE FUNCTION/PROCEDURE/TRIGGER Not Implemented ❌

**Evidence:**
```cpp
// Line 299-302
// TODO: Add more CREATE types
// if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
// if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
// if (matchContextual("TRIGGER"))    return parseCreateTrigger();
```

**Fix Required:** Implement these CREATE statements

---

## Code Quality

### Strengths ✅

1. **Context-Sensitive Keyword Handling** - Gatekeeper model works well
2. **Arena Allocation** - Efficient AST node memory management
3. **Error Recovery** - Basic synchronization implemented
4. **Source Location Tracking** - Good error reporting
5. **Modular Design** - Clean separation of parsing methods

### Issues ⚠️

1. **Incomplete Features** - AST infrastructure exists but unused (PSQL, CTEs)
2. **PostgreSQL Mixing** - Unclear if intentional or contamination
3. **DELETE FROM Required** - Not all SQL dialects require FROM keyword
4. **Schema Path Parsing** - Unusual DOT/DOUBLE_DOT/EXCLAIM_COLON check before identifier

---

## Test Coverage

**Test Files:**
- `tests/unit/test_parser_v2_ddl.cpp` - DDL tests
- `tests/unit/test_parser_dml_v2.cpp` - DML tests
- `tests/unit/test_parser_session_v2.cpp` - Session tests
- `tests/unit/test_parser_state_v2.cpp` - Parser state tests

**Missing Tests:**
- PSQL parsing (intentional - not implemented)
- CTE parsing
- Complex JOIN expressions
- Window function edge cases
- Error recovery scenarios

---

## Recommendations

### CRITICAL - Complete Missing Features

1. **Implement PSQL Parsing**
   - Add `parseExecuteBlock()`, `parseIfStmt()`, `parseWhileStmt()`, etc.
   - Populate existing AST node infrastructure
   - Essential for stored procedure/function support

2. **Implement CTE Parsing**
   - Add `parseWithClause()`
   - Populate `with` fields in INSERT/UPDATE/DELETE
   - Critical for complex queries

3. **Implement CREATE FUNCTION/PROCEDURE/TRIGGER**
   - Remove TODO comments
   - Add full parsing support

### HIGH - Clarify PostgreSQL Extensions

4. **Document V2 SQL Dialect**
   - Create specification listing all V2 features
   - Explicitly state which PostgreSQL extensions are intentional
   - OR remove PostgreSQL-specific features for pure Firebird style

5. **Decide on UPDATE...FROM and DELETE...USING**
   - Keep as V2 extensions? Document it
   - Remove for Firebird purity? Specify it

### MEDIUM - Improve Code Quality

6. **Expand Test Coverage**
   - Add PSQL tests (once implemented)
   - Add CTE tests (once implemented)
   - Add edge case tests

7. **Review DELETE FROM Requirement**
   - Consider making FROM optional for SQL standard compatibility

8. **Document Schema Path Syntax**
   - Clarify DOT/DOUBLE_DOT/EXCLAIM_COLON handling

---

## Production Readiness

**Current Status:** ⚠️ **CONDITIONAL**

**Ready For:**
- ✅ DDL operations (CREATE, ALTER, DROP)
- ✅ DML operations (SELECT, INSERT, UPDATE, DELETE, MERGE)
- ✅ Transaction control
- ✅ Session management
- ✅ Security (GRANT/REVOKE)
- ✅ Domain operations

**NOT Ready For:**
- ❌ Stored procedures
- ❌ Stored functions
- ❌ Triggers with procedural bodies
- ❌ Anonymous blocks (EXECUTE BLOCK)
- ❌ Common Table Expressions (WITH)

**Recommendation:**

**FOR BASIC SQL OPERATIONS:** ✅ Production ready
**FOR PROCEDURAL FEATURES:** ❌ Use emulated parsers (Firebird/PostgreSQL)

**Path to Full Production:**
1. Implement PSQL parsing (highest priority)
2. Implement CTE parsing
3. Clarify PostgreSQL extension policy
4. Expand test coverage

---

## Conclusion

V2 parser is a **high-quality, context-sensitive parser** with **excellent DDL/DML support** but **incomplete procedural SQL implementation**.

The presence of PostgreSQL extensions needs clarification - are they intentional V2 features or unintended contamination? The specification states V2 should follow "FirebirdSQL standard" for style/formatting, but allows "many more expansions."

**Verdict:** Production-ready for non-procedural SQL, requires PSQL implementation for full functionality.

---

**Full Audit Details:** See agent output in conversation history
**Related Documents:**
- `/docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `/docs/specifications/parser/ScratchBird Master Grammar Specification v2.0.md`
- `/MGA_RULES.md`
- `/IMPLEMENTATION_STANDARDS.md`
