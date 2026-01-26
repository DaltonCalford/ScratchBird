# Firebird-V2 Feature Parity Specification

**Version:** 1.0
**Date:** 2026-01-07
**Status:** DRAFT - Implementation Required
**Related:** `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`, `/docs/audit/parsers/COMPARISON_MATRIX.md`

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

---

## Executive Summary

This specification ensures that:
1. **Firebird parser** implements all Firebird SQL features completely
2. **V2 parser** supports all Firebird functionality (V2 is a superset of Firebird)
3. **Context variables** are properly implemented in both parsers
4. **MGA transaction semantics** are correctly documented and enforced

**Critical Finding from Research:** Firebird MGA does **NOT** support dirty reads (READ UNCOMMITTED). The audit documents incorrectly implied this might be supported.

---

## Table of Contents

1. [Transaction Isolation Levels (MGA Clarification)](#transaction-isolation-levels-mga-clarification)
2. [Context Variables](#context-variables)
3. [Generator/Sequence Functions](#generatorsequence-functions)
4. [Missing DDL Features](#missing-ddl-features)
5. [Missing DML Features](#missing-dml-features)
6. [PSQL (Procedural SQL)](#psql-procedural-sql)
7. [Implementation Requirements](#implementation-requirements)

---

## Transaction Isolation Levels (MGA Clarification)

### Research Summary

Based on authoritative sources ([Firebird ACID Documentation](https://firebirdsql.org/file/documentation/papers_presentations/html/paper-fbent-acid.html), [IBSurgeon Transactions Guide](https://ib-aid.com/en/transactions-in-firebird-acid-isolation-levels-deadlocks-and-update-conflicts-resolution/)), Firebird's Multi-Generational Architecture (MGA) provides the following isolation levels:

### Supported Isolation Levels

| Level | Firebird Name | SQL Standard Equivalent | Dirty Reads | Non-Repeatable Reads | Phantom Reads |
|-------|---------------|------------------------|-------------|---------------------|---------------|
| 0 | READ COMMITTED | READ COMMITTED | ❌ NO | ✅ YES | ✅ YES |
| 2 | SNAPSHOT | REPEATABLE READ | ❌ NO | ❌ NO | ❌ NO |
| 3 | SNAPSHOT TABLE STABILITY | SERIALIZABLE (approx) | ❌ NO | ❌ NO | ❌ NO |

### NOT Supported

**READ UNCOMMITTED (Dirty Reads):** ❌ **FIREBIRD DOES NOT SUPPORT DIRTY READS**

From [IBExpert Documentation](http://ibexpert.com/docu/doku.php?id=01-documentation:01-05-database-technology:database-technology-articles:firebird-interbase-server:transaction-options-explained):

> "Even though in theory Firebird/InterBase can use its MGA to offer this level, it doesn't support it. Read uncommitted (called Dirty Read) allows one transaction to read all changes made by other transactions to any record, even those changes that are uncommitted."

### READ COMMITTED Variants (Firebird-Specific)

Firebird provides two variants of READ COMMITTED:

1. **READ COMMITTED RECORD_VERSION** (default)
   - Uses MGA to read previous committed version of row
   - Never blocks on locked records
   - Returns most recent committed version

2. **READ COMMITTED NO_RECORD_VERSION**
   - Imitates locking database behavior
   - Blocks when encountering locked record
   - Waits for lock release

### Parser Requirements

**Firebird Parser:**
```sql
SET TRANSACTION
    READ COMMITTED
    [RECORD_VERSION | NO_RECORD_VERSION]
    [WAIT | NO WAIT]
    [LOCK TIMEOUT n]
    [RESERVING table [FOR SHARED | PROTECTED] [READ | WRITE]]
```

**V2 Parser:**
Must support all Firebird transaction syntax plus any V2 extensions.

**Implementation Status:**
- ✅ Firebird parser: Correctly implements all variants
- ⚠️ V2 parser: Needs verification for RECORD_VERSION/NO_RECORD_VERSION support

---

## Context Variables

### Standard SQL Context Variables

These are **zero-argument functions** that return session/connection context:

| Variable | Firebird | V2 | Returns | SBLR Opcode |
|----------|----------|-------|---------|-------------|
| CURRENT_DATE | ✅ | ✅ | Current date | Context var |
| CURRENT_TIME | ✅ | ✅ | Current time | EXT_FUNC_CURRENT_TIME |
| CURRENT_TIMESTAMP | ✅ | ✅ | Current timestamp | Context var |
| CURRENT_USER | ✅ | ✅ | Current user name | Context var |
| CURRENT_ROLE | ✅ | ✅ | Current role name | Context var |
| CURRENT_CONNECTION | ✅ | ⚠️ | Connection ID (INTEGER) | Context var |
| CURRENT_TRANSACTION | ✅ | ⚠️ | Transaction ID (INTEGER) | Context var |

**Current Implementation:**
- ✅ Firebird parser handles all 7 variables (lines 810-850 in firebird_parser.cpp)
- ⚠️ V2 parser needs verification for CURRENT_CONNECTION and CURRENT_TRANSACTION

**Parser Treatment:**
These are parsed as `FunctionCallExpr` with zero arguments:
```cpp
auto* expr = allocate<ast::FunctionCallExpr>();
expr->function_path.components.push_back(string_pool_.intern("CURRENT_DATE"));
```

### Firebird Context Functions (RDB$GET_CONTEXT / RDB$SET_CONTEXT)

Based on [Firebird Language Reference](https://firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-functions.html):

#### RDB$GET_CONTEXT(namespace, variable)

**Signature:**
```sql
RDB$GET_CONTEXT(namespace VARCHAR(80), varname VARCHAR(80)) RETURNS VARCHAR(255)
```

**Namespaces:**

1. **SYSTEM** (read-only) - Connection and transaction context:
   - `NETWORK_PROTOCOL` - 'TCPv4', 'TCPv6', 'XNET', or NULL
   - `CLIENT_ADDRESS` - IP address for TCP connections
   - `CLIENT_HOST` - Remote client hostname
   - `CLIENT_PID` - Process ID of client
   - `CLIENT_PROCESS` - Process name of client
   - `DB_NAME` - Database path or alias
   - `ENGINE_VERSION` - Firebird server version
   - `ISOLATION_LEVEL` - 'READ COMMITTED', 'SNAPSHOT', or 'CONSISTENCY'
   - `LOCK_TIMEOUT` - Lock timeout value
   - `READ_ONLY` - 'TRUE' or 'FALSE'
   - `SNAPSHOT_NUMBER` - Current snapshot number
   - `SESSION_TIMEZONE` - Session timezone (Firebird 4.0+)
   - `REPLICA_MODE` - 'none', 'read-only', 'read-write' (Firebird 4.0+)
   - `PARALLEL_WORKERS` - Parallel workers setting (Firebird 5.0+)
   - `DECFLOAT_ROUND`, `DECFLOAT_TRAPS` - DECFLOAT settings (Firebird 5.0+)

2. **USER_SESSION** (read-write) - User-defined session variables
   - Initially empty
   - Persist for duration of connection
   - Set with RDB$SET_CONTEXT('USER_SESSION', 'var', 'value')

3. **USER_TRANSACTION** (read-write) - User-defined transaction variables
   - Initially empty
   - Persist for duration of transaction
   - Set with RDB$SET_CONTEXT('USER_TRANSACTION', 'var', 'value')

4. **DDL_TRIGGER** (read-only, Firebird 5.0+) - DDL trigger context
   - Only valid inside DDL triggers
   - Contains metadata about DDL operation

**Examples:**
```sql
SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB$DATABASE;
SELECT RDB$GET_CONTEXT('USER_SESSION', 'my_variable') FROM RDB$DATABASE;
```

#### RDB$SET_CONTEXT(namespace, variable, value)

**Signature:**
```sql
RDB$SET_CONTEXT(namespace VARCHAR(80), varname VARCHAR(80), value VARCHAR(255)) RETURNS INTEGER
```

**Valid Namespaces:** USER_SESSION, USER_TRANSACTION only (SYSTEM and DDL_TRIGGER are read-only)

**Returns:** 1 on success

**Examples:**
```sql
SELECT RDB$SET_CONTEXT('USER_SESSION', 'department', 'Sales') FROM RDB$DATABASE;
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'batch_id', '12345') FROM RDB$DATABASE;
```

**Implementation Requirements:**

**Firebird Parser:**
- ❌ **CRITICAL BUG**: `RDB_GET_CONTEXT` and `RDB_SET_CONTEXT` are defined as keywords but NOT in `isNonReservedKeyword()` list
- ❌ Cannot currently parse as function calls
- ✅ Keywords defined in lexer (lines 322, 326 of firebird_lexer.cpp)

**Fix Required:**
```cpp
// In firebird_parser.cpp, isNonReservedKeyword() function (around line 221):
case TokenType::KW_RDB_GET_CONTEXT:  // ADD THIS
case TokenType::KW_RDB_SET_CONTEXT:  // ADD THIS
```

**V2 Parser:**
- ⚠️ Needs implementation - should support RDB$GET_CONTEXT/RDB$SET_CONTEXT as regular functions

### Special Firebird Pseudo-Columns

These are **NOT context variables** but special record-level values:

| Pseudo-Column | Type | Description | Usage |
|--------------|------|-------------|-------|
| RDB$DB_KEY | CHAR(8) | Unique physical row identifier | `SELECT RDB$DB_KEY, * FROM table` |
| RDB$RECORD_VERSION | INTEGER | Record version number (MGA) | `SELECT RDB$RECORD_VERSION, * FROM table` |

**Implementation:**
- ✅ Firebird lexer has keywords (lines 320, 324)
- ❌ Parser handling needs verification
- These should be treated as special column references, not functions

---

## Generator/Sequence Functions

### GEN_ID() Function

**Signature:**
```sql
GEN_ID(generator_name, increment) RETURNS BIGINT
```

**Purpose:** Generate next value from a sequence/generator

**Examples:**
```sql
SELECT GEN_ID(GEN_CUSTOMER_ID, 1) FROM RDB$DATABASE;  -- Increment by 1
SELECT GEN_ID(GEN_ORDER_ID, 0) FROM RDB$DATABASE;     -- Get current value (no increment)
SELECT GEN_ID(GEN_BATCH_ID, 100) FROM RDB$DATABASE;   -- Increment by 100
```

**Current Status:**
- ✅ Firebird lexer: Keyword defined (line 507)
- ❌ **CRITICAL BUG**: Not in `isNonReservedKeyword()` list - cannot parse
- ⚠️ V2 parser: Needs implementation

**Fix Required:**
```cpp
// In firebird_parser.cpp, isNonReservedKeyword():
case TokenType::KW_GEN_ID:       // ADD THIS
case TokenType::KW_GEN_UUID:     // ADD THIS
```

### GEN_UUID() Function

**Signature:**
```sql
GEN_UUID() RETURNS CHAR(16) CHARACTER SET OCTETS
```

**Purpose:** Generate a universally unique identifier (UUID/GUID)

**Example:**
```sql
SELECT GEN_UUID() FROM RDB$DATABASE;
INSERT INTO orders (order_id, ...) VALUES (GEN_UUID(), ...);
```

**Current Status:**
- ✅ Firebird lexer: Keyword defined (line 508)
- ❌ **CRITICAL BUG**: Not in `isNonReservedKeyword()` list
- ⚠️ V2 parser: Needs implementation

### NEXT VALUE FOR (SQL Standard Alternative)

Firebird 3.0+ supports SQL standard syntax:

```sql
SELECT NEXT VALUE FOR generator_name FROM RDB$DATABASE;
```

**Implementation:**
- ⚠️ Both parsers need to support this syntax
- Equivalent to `GEN_ID(generator_name, 1)`

---

## Missing DDL Features

### CREATE SEQUENCE / CREATE GENERATOR

Firebird supports both SQL standard and legacy syntax:

```sql
-- SQL standard (Firebird 3.0+)
CREATE SEQUENCE seq_name [START WITH n] [INCREMENT BY n];

-- Legacy Firebird (still supported)
CREATE GENERATOR gen_name;
SET GENERATOR gen_name TO n;

-- Firebird 3.0+ unified
ALTER SEQUENCE seq_name RESTART WITH n;
```

**Current Status:**
- ⚠️ Firebird parser: Needs verification
- ⚠️ V2 parser: Needs implementation

### DROP SEQUENCE / DROP GENERATOR

```sql
DROP SEQUENCE seq_name;
DROP GENERATOR gen_name;  -- Legacy
```

**Current Status:**
- ❌ Both parsers: Not implemented per audit findings

### RECREATE SEQUENCE

```sql
RECREATE SEQUENCE seq_name [START WITH n];
```

**Current Status:**
- ❌ Needs implementation

---

## Missing DML Features

### UPDATE OR INSERT (Firebird Upsert)

Firebird-specific upsert syntax:

```sql
UPDATE OR INSERT INTO table (col1, col2, ...)
VALUES (val1, val2, ...)
[MATCHING (col1, col2, ...)]
[RETURNING col1, col2, ...];
```

**Current Status:**
- ⚠️ Firebird parser: Defined but needs verification
- ❌ V2 parser: Not implemented

### MERGE Statement

Firebird 5.0+ supports SQL standard MERGE:

```sql
MERGE INTO target
USING source ON condition
WHEN MATCHED THEN UPDATE SET ...
WHEN NOT MATCHED THEN INSERT ...
[WHEN NOT MATCHED BY SOURCE THEN DELETE];
```

**Current Status:**
- ✅ Firebird parser: Implemented
- ❌ V2 parser: Not implemented (CRITICAL GAP per audit)
- ❌ Executor: No support for EXT_MERGE_* opcodes

### RETURNING Clause (All DML)

```sql
INSERT INTO ... RETURNING col1, col2, ...;
UPDATE ... RETURNING col1, col2, ...;
DELETE ... RETURNING col1, col2, ...;
UPDATE OR INSERT ... RETURNING col1, col2, ...;
```

**Current Status:**
- ✅ Firebird parser: Implemented
- ⚠️ V2 parser: Partial implementation

---

## PSQL (Procedural SQL)

### CREATE FUNCTION / CREATE PROCEDURE

**CRITICAL GAP:** Neither Firebird nor V2 parser fully implements PSQL parsing.

**Firebird Syntax:**
```sql
CREATE [OR ALTER] FUNCTION func_name [(param1 type, ...)]
RETURNS return_type
[DETERMINISTIC]
AS
BEGIN
  -- Function body
  RETURN value;
END;

CREATE [OR ALTER] PROCEDURE proc_name [(param1 type [, param2 type]...)]
[RETURNS (output1 type [, output2 type]...)]
AS
BEGIN
  -- Procedure body
  [SUSPEND;]  -- For selectable procedures
END;
```

**Current Status:**
- ✅ Firebird parser: Stubs exist (lines 299-302 in parser_v2.cpp show TODOs)
- ❌ V2 parser: Not implemented (CRITICAL per audit)
- ✅ SBLR opcodes: Defined (EXT_FUNCTION=0x90, EXT_PROCEDURE=0x91)

### EXECUTE BLOCK

Anonymous PSQL block execution:

```sql
EXECUTE BLOCK [(param type = ?, ...)]
[RETURNS (output1 type, ...)]
AS
DECLARE VARIABLE var1 type;
BEGIN
  -- Block body
  [SUSPEND;]
END;
```

**Current Status:**
- ❌ Firebird parser: Not implemented
- ❌ V2 parser: Not implemented
- ✅ SBLR opcode: EXT_BLOCK=0x92

### PSQL Control Structures

Required for both parsers:

```sql
-- Variable declaration
DECLARE [VARIABLE] var_name type [= initial_value];

-- Assignment
var_name = expression;

-- Conditional
IF (condition) THEN
  statement;
[ELSE
  statement;]

-- CASE (statement-level)
CASE <expr>
  WHEN <expr> THEN statement;
  [WHEN <expr> THEN statement;]*
  [ELSE statement;]
END CASE;

CASE
  WHEN <condition> THEN statement;
  [WHEN <condition> THEN statement;]*
  [ELSE statement;]
END CASE;

-- Loops
WHILE (condition) DO
  statement;

FOR SELECT col1, col2 INTO :var1, :var2
FROM table
DO
  statement;

FOR variable = start_value TO end_value [BY step] DO
  statement;

-- Exception handling
WHEN exception_name DO
  statement;

BEGIN
  statement;
WHEN ANY DO
  -- Error handler
END;
```

**Current Status:**
- ❌ Both parsers: Not implemented
- ✅ AST nodes exist in V2 (ExecuteBlockStmt, IfStmt, WhileStmt, ForSelectStmt)
- ✅ SBLR opcodes exist (0x93-0xA8 range)

### CREATE TRIGGER

```sql
CREATE TRIGGER trigger_name
FOR table_name
[ACTIVE | INACTIVE]
{BEFORE | AFTER} {INSERT | UPDATE | DELETE}
[POSITION number]
AS
BEGIN
  -- Trigger body
  -- Access to NEW and OLD context variables
END;
```

**Current Status:**
- ❌ Both parsers: Not implemented
- ✅ SBLR opcodes: EXT_CREATE_TRIGGER=0x70

---

## Parsed But Not Implemented Features (CRITICAL GAPS)

### Overview

This section documents features that are **ACCEPTED BY PARSERS** but have **NO ACTUAL IMPLEMENTATION** in the bytecode generator, executor, or catalog layer. These are silent failures where syntax is parsed correctly but functionality does not work.

**Impact:** Users receive no error but features don't work as expected.

---

### 1. TEMPORARY TABLES (CRITICAL - All Parsers)

**Status:** ❌ **PARSED BUT NOT IMPLEMENTED**

#### Current Behavior

All four parsers accept temporary table syntax but create **PERMANENT** tables:

```sql
-- Firebird/V2
CREATE TEMPORARY TABLE test (id INT);           -- Creates PERMANENT table
CREATE GLOBAL TEMPORARY TABLE test (id INT)     -- Creates PERMANENT table
    ON COMMIT DELETE ROWS;

-- PostgreSQL
CREATE TEMP TABLE test (id INT);                -- Creates PERMANENT table
CREATE TEMPORARY TABLE test (id INT);           -- Creates PERMANENT table

-- MySQL
CREATE TEMPORARY TABLE test (id INT);           -- Creates PERMANENT table
```

#### Parser Implementation Status

| Parser | Syntax Parsing | Flag Set | Bytecode Emitted | Executor Handles | Result |
|--------|---------------|----------|------------------|------------------|---------|
| **Firebird** | ✅ Lines 1607-1620 | ✅ `stmt->temporary = true` | ❌ NO | ❌ NO | PERMANENT table |
| **V2** | ✅ Lines 265-269 | ✅ `stmt->temporary = true` | ❌ NO | ❌ NO | PERMANENT table |
| **PostgreSQL** | ✅ Line 158 | ⚠️ `is_temp` read but **DISCARDED** | ❌ NO | ❌ NO | PERMANENT table |
| **MySQL** | ✅ Line 2703 | ⚠️ Matched but **DISCARDED** | ❌ NO | ❌ NO | PERMANENT table |

**File Locations:**
- Firebird parser: `src/parser/firebird/firebird_parser.cpp:1607-1620, 1912-1915`
- V2 parser: `src/parser/parser_v2.cpp:265-269`
- PostgreSQL parser: `src/parser/postgresql/pg_parser_ddl.cpp:158`
- MySQL parser: `src/parser/mysql/mysql_parser.cpp:2703`
- AST definition: `include/scratchbird/parser/ast_v2.h:458` (`bool temporary`)
- Bytecode generator: `src/sblr/bytecode_generator_v2.cpp:619-677` (❌ **IGNORES** `stmt->temporary`)
- Executor: `src/sblr/executor.cpp:4152+` (❌ **NO HANDLING**)
- Catalog: `include/scratchbird/core/catalog_manager.h:1520` (❌ **NO PARAMETER**)

#### ON COMMIT Clause (Firebird-Specific)

Firebird parser **parses but discards** ON COMMIT clause:

**File:** `src/parser/firebird/firebird_parser.cpp:1943-1952`
```cpp
// Optional ON COMMIT clause for GTT
if (temporary && matchKeyword(TokenType::KW_ON)) {
    consume(TokenType::KW_COMMIT, "Expected COMMIT after ON");
    if (matchKeyword(TokenType::KW_DELETE)) {
        // ON COMMIT DELETE ROWS (default for Firebird)
    } else if (matchKeyword(TokenType::KW_PRESERVE)) {
        // ON COMMIT PRESERVE ROWS
    }
    matchKeyword(TokenType::KW_ROWS);  // Optional ROWS keyword
}
```

**Status:** Parsed but **NOT STORED** in AST or emitted to bytecode.

#### Required Implementation

**Phase 1: AST Extension**
```cpp
// In include/scratchbird/parser/ast_v2.h, CreateTableStmt class
enum class TemporaryTableType : uint8_t {
    NONE = 0,           // Not a temporary table
    SESSION = 1,        // Session-scoped (PostgreSQL TEMP, MySQL TEMPORARY)
    TRANSACTION = 2,    // Transaction-scoped (Firebird GTT with DELETE ROWS)
    GLOBAL = 3,         // Global temporary (Firebird GLOBAL TEMPORARY with PRESERVE ROWS)
};

enum class OnCommitAction : uint8_t {
    NONE = 0,           // Not specified
    DELETE_ROWS = 1,    // Truncate on commit (Firebird default)
    PRESERVE_ROWS = 2,  // Keep rows on commit (PostgreSQL default)
    DROP = 3,           // Drop table on commit (PostgreSQL only)
};

// Add to CreateTableStmt:
TemporaryTableType temporary_type = TemporaryTableType::NONE;
OnCommitAction on_commit = OnCommitAction::NONE;
```

**Phase 2: Bytecode Format**
```cpp
// Extend CREATE_TABLE opcode payload:
// [CREATE_TABLE] [flags_byte] [TABLE_REF] ...
//
// flags_byte bits:
// bit 0: if_not_exists
// bit 1: or_replace
// bit 2-3: temporary_type (0=none, 1=session, 2=transaction, 3=global)
// bit 4-5: on_commit (0=none, 1=delete_rows, 2=preserve_rows, 3=drop)
// bit 6-7: reserved
```

**Phase 3: Catalog Extension**
```cpp
// In CatalogManager::createTable(), add parameters:
auto createTable(
    const ID &schema_id,
    const std::string &table_name,
    const std::vector<ColumnInfo> &columns,
    ID &table_id,
    uint16_t tablespace_id = 0,
    TemporaryTableType temp_type = TemporaryTableType::NONE,  // NEW
    OnCommitAction on_commit = OnCommitAction::NONE,          // NEW
    ConnectionContext* conn_ctx = nullptr,                    // NEW - for session tracking
    ErrorContext *ctx = nullptr
) -> Status;

// Store in catalog:
// - is_temporary flag
// - temporary_type (session/transaction/global)
// - on_commit action
// - creating_session_id (for session-scoped cleanup)
// - creating_transaction_id (for transaction-scoped cleanup)
```

**Phase 4: Session/Transaction Tracking**
```cpp
// In ConnectionContext, add:
std::unordered_set<ID> session_temp_tables;    // Clean up on disconnect
std::unordered_set<ID> transaction_temp_tables; // Clean up on commit/rollback

// In TransactionContext, add:
void onCommit() {
    // Handle ON COMMIT DELETE ROWS (truncate)
    // Handle ON COMMIT DROP (drop table)
    // Clear transaction_temp_tables
}

void onRollback() {
    // Drop transaction-scoped temp tables
    // Clear transaction_temp_tables
}
```

**Phase 5: Executor Implementation**
- Read temporary flags from bytecode
- Pass to CatalogManager::createTable()
- Enforce visibility rules (temp tables only visible to creating session)
- Register for cleanup on session disconnect or transaction end

**Phase 6: Storage**
- Session/transaction temp tables: Use in-memory storage OR separate temp tablespace
- Do NOT persist to main storage engine
- Do NOT write to the optional write-after log stream

#### Testing Requirements

```sql
-- Test 1: Session-scoped temporary table
CREATE TEMPORARY TABLE session_temp (id INT, value TEXT);
INSERT INTO session_temp VALUES (1, 'test');
SELECT * FROM session_temp;  -- Should return 1 row
-- Disconnect and reconnect
SELECT * FROM session_temp;  -- Should fail (table doesn't exist)

-- Test 2: Transaction-scoped with DELETE ROWS (Firebird)
CREATE GLOBAL TEMPORARY TABLE tx_temp (id INT) ON COMMIT DELETE ROWS;
BEGIN;
INSERT INTO tx_temp VALUES (1);
SELECT * FROM tx_temp;  -- Should return 1 row
COMMIT;
SELECT * FROM tx_temp;  -- Should return 0 rows (truncated)

-- Test 3: Transaction-scoped with PRESERVE ROWS (Firebird)
CREATE GLOBAL TEMPORARY TABLE tx_temp2 (id INT) ON COMMIT PRESERVE ROWS;
BEGIN;
INSERT INTO tx_temp2 VALUES (1);
COMMIT;
SELECT * FROM tx_temp2;  -- Should return 1 row (preserved)

-- Test 4: Visibility isolation
-- Session 1:
CREATE TEMPORARY TABLE my_temp (id INT);
-- Session 2:
SELECT * FROM my_temp;  -- Should fail (not visible to other sessions)

-- Test 5: ON COMMIT DROP (PostgreSQL)
CREATE TEMPORARY TABLE drop_on_commit (id INT) ON COMMIT DROP;
BEGIN;
INSERT INTO drop_on_commit VALUES (1);
COMMIT;
SELECT * FROM drop_on_commit;  -- Should fail (table dropped)
```

**Priority:** ❌ **CRITICAL** - Feature is advertised by parser but doesn't work

---

### 2. UNLOGGED TABLES (V2 and PostgreSQL Parsers)

**Status:** ❌ **PARSED BUT NOT IMPLEMENTED**

#### Current Behavior

V2 and PostgreSQL parsers accept UNLOGGED keyword but ignore it:

```sql
-- V2 Parser
CREATE UNLOGGED TABLE test (id INT);  -- Creates normal logged table

-- PostgreSQL Parser
CREATE UNLOGGED TABLE test (id INT);  -- Creates normal logged table
```

#### Parser Implementation Status

**V2 Parser:**
- File: `src/parser/parser_v2.cpp:237-241`
- Reads `unlogged` flag: `stmt->unlogged = unlogged;` (line 268)
- AST field: `include/scratchbird/parser/ast_v2.h:459` (`bool unlogged`)
- Bytecode: ❌ **NOT EMITTED** in `src/sblr/bytecode_generator_v2.cpp`

**PostgreSQL Parser:**
- File: `src/parser/postgresql/pg_parser_ddl.cpp:160-161`
- Reads flag: `bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);`
- Status: ⚠️ **DISCARDED** (never used)

#### What UNLOGGED Tables Should Do

**Purpose:** Optimize performance by skipping optional write-after log writes

**MGA Note:** ScratchBird does not use WAL for recovery, so UNLOGGED tables are effectively identical to regular tables today. If a write-after log (WAL, optional post-gold) is introduced later (replication/PITR), UNLOGGED tables can bypass that stream.

**Characteristics (PostgreSQL semantics):**
- NOT written to write-after log (WAL, optional)
- Faster INSERT/UPDATE/DELETE (no optional post-gold write-after log overhead)
- NOT crash-safe (truncated on crash recovery)
- Use case: Temporary data, staging tables, caches

#### Required Implementation

1. **Bytecode Extension:** Add `unlogged` flag to CREATE_TABLE opcode
2. **Catalog Storage:** Store `is_unlogged` flag in table metadata
3. **Storage Engine:** Skip optional post-gold write-after log (if introduced)
4. **Durability/PITR:** Truncate unlogged tables only if an optional post-gold write-after log durability path exists

**Priority:** 🟡 **MEDIUM** - Performance feature, not critical for correctness

---

### 3. PARTITIONED TABLES (V2 Parser)

**Status:** ⚠️ **PARSED, OPCODE EXISTS, EXECUTOR UNKNOWN**

#### Current Behavior

V2 parser fully parses PARTITION BY clause:

```sql
-- V2 Parser
CREATE TABLE orders (
    order_id INT,
    order_date DATE
) PARTITION BY RANGE (order_date);  -- Parsed but unknown if executor supports
```

#### Parser Implementation Status

**V2 Parser:**
- File: `src/parser/parser_v2.cpp:384-397`
- Fully parsed: partition type, partition columns
- AST fields: `is_partitioned`, `partition_by`, `partition_columns`
- Opcode: ✅ **EXISTS** - `PARTITION_BY = 0xD8` in `include/scratchbird/sblr/opcodes.h:217`

**Unknown:**
- ❓ Does bytecode generator emit PARTITION_BY?
- ❓ Does executor handle PARTITION_BY?
- ❓ Does catalog store partition metadata?

**Action Required:** Audit bytecode generator and executor for partition support

**Priority:** 🟡 **MEDIUM** - May already be implemented, needs verification

---

### 4. VIEW OPTIONS (V2 Parser)

#### 4a. WITH CHECK OPTION

**Status:** ❌ **PARSED BUT NOT IMPLEMENTED**

```sql
CREATE VIEW active_users AS
    SELECT * FROM users WHERE active = true
WITH CHECK OPTION;  -- Parsed but not enforced
```

**Parser Implementation:**
- File: `src/parser/parser_v2.cpp:1251` (line mentioned but WITH CHECK OPTION parsing present)
- AST fields: `with_check_option`, `check_option_local` (`include/scratchbird/parser/ast_v2.h:555-556`)
- Status: ❌ **NOT EMITTED** to bytecode

**What It Should Do:**
- Enforce that INSERT/UPDATE through view must satisfy view's WHERE clause
- `WITH LOCAL CHECK OPTION`: Check only this view's condition
- `WITH CASCADED CHECK OPTION`: Check this view and all underlying views (default)

**Priority:** 🟡 **MEDIUM** - SQL standard feature for data integrity

#### 4b. TEMPORARY VIEWS

**Status:** ❌ **PARSED BUT NOT IMPLEMENTED**

```sql
CREATE TEMPORARY VIEW temp_view AS SELECT * FROM table;  -- Parsed but creates permanent view
```

**Parser Implementation:**
- AST field: `CreateViewStmt::temporary` (`include/scratchbird/parser/ast_v2.h:545`)
- Flag set: `stmt->temporary = temporary;` (`src/parser/parser_v2.cpp:284`)
- Status: ❌ Same as temporary tables - not implemented

**Priority:** 🟡 **MEDIUM** - Should be implemented with temporary tables

#### 4c. MATERIALIZED VIEWS

**Status:** ⚠️ **PARTIAL IMPLEMENTATION**

```sql
CREATE MATERIALIZED VIEW mat_view AS SELECT * FROM table;
REFRESH MATERIALIZED VIEW mat_view;
```

**Parser Implementation:**
- AST field: `CreateViewStmt::materialized` (`include/scratchbird/parser/ast_v2.h:546`)
- Bytecode: ✅ **FLAGS EMITTED** (`src/sblr/bytecode_generator_v2.cpp:795, 808-809`)
- Refresh opcode: ✅ **EXISTS** - `REFRESH_MATERIALIZED_VIEW = 0x2B`

**Unknown:**
- ❓ Does executor support materialized views?
- ❓ Does catalog store materialized view metadata?
- ❓ Does REFRESH MATERIALIZED VIEW work?

**Action Required:** Verify executor implementation

**Priority:** 🟢 **LOW** - May already work, needs testing

---

### 5. TEMPORARY SEQUENCES (V2 and Firebird Parsers)

**Status:** ❌ **PARSED BUT NOT IMPLEMENTED**

```sql
CREATE TEMPORARY SEQUENCE temp_seq;  -- Parsed but creates permanent sequence
```

**Parser Implementation:**
- AST field: `CreateSequenceStmt::temporary` (`include/scratchbird/parser/ast_v2.h:573`)
- Flag set: `stmt->temporary = temporary;` (`src/parser/parser_v2.cpp:294`)
- Status: ❌ Same as temporary tables - not implemented

**Priority:** 🟡 **MEDIUM** - Should be implemented with temporary tables

---

### 6. CONSTRAINT DEFERRABILITY (V2 Parser)

**Status:** ⚠️ **PARTIALLY IMPLEMENTED**

```sql
ALTER TABLE orders ADD CONSTRAINT fk_customer
    FOREIGN KEY (customer_id) REFERENCES customers(id)
    DEFERRABLE INITIALLY DEFERRED;  -- Parsed, bytecode emitted, but constraint storage unknown
```

**Parser Implementation:**
- Parsed: `src/parser/parser_v2.cpp:4062-4071, 4498-4507`
- AST fields: `deferrable`, `not_deferrable` (`include/scratchbird/parser/ast_v2.h:2068-2069, 2218-2219`)
- Bytecode: ✅ **EMITTED** for transactions (`src/sblr/bytecode_generator_v2.cpp:1342, 1367, 1465, 1492, 3051`)

**Unknown:**
- ❓ Is deferrability stored in FK constraint metadata?
- ❓ Does executor enforce deferred checking?
- ❓ Can constraints be SET DEFERRED/IMMEDIATE per transaction?

**Action Required:** Verify catalog stores deferrability and executor enforces it

**Priority:** 🟡 **MEDIUM** - Important for complex transactions

---

### 7. MYSQL TABLE OPTIONS (MySQL Parser)

**Status:** ⚠️ **PARSED, UNKNOWN IF IMPLEMENTED**

MySQL parser accepts 20+ table options:

```sql
CREATE TABLE test (id INT)
    ENGINE=InnoDB
    CHARSET=utf8mb4
    COLLATE=utf8mb4_unicode_ci
    COMMENT='Test table'
    AUTO_INCREMENT=1000
    ROW_FORMAT=COMPRESSED;
```

**Parser Implementation:**
- File: `src/parser/mysql/mysql_parser.cpp:2858+`
- Parsed options: ENGINE, AUTO_INCREMENT, CHARSET, COLLATE, COMMENT, ROW_FORMAT, and 15+ more
- Lines: 2944-3000 (table options), 3173-3197 (column options)

**Unknown:**
- ❓ Are these options emitted to bytecode?
- ❓ Does catalog store them?
- ❓ Does executor honor them?

**Action Required:** Audit MySQL parser bytecode emission

**Priority:** 🟡 **MEDIUM** - Important for MySQL compatibility

---

## Summary of Parsed-But-Not-Implemented Features

| Feature | Parsers Affected | AST Field | Bytecode | Executor | Priority |
|---------|-----------------|-----------|----------|----------|----------|
| **TEMPORARY TABLES** | All 4 | ✅ | ❌ | ❌ | ❌ **CRITICAL** |
| **ON COMMIT clause** | Firebird | ❌ | ❌ | ❌ | ❌ **CRITICAL** |
| **UNLOGGED TABLES** | V2, PostgreSQL | ✅ | ❌ | ❌ | 🟡 MEDIUM |
| **PARTITION BY** | V2 | ✅ | ❓ | ❓ | 🟡 MEDIUM (verify) |
| **WITH CHECK OPTION** | V2 | ✅ | ❌ | ❌ | 🟡 MEDIUM |
| **TEMPORARY VIEWS** | V2, Firebird | ✅ | ❌ | ❌ | 🟡 MEDIUM |
| **MATERIALIZED VIEWS** | V2 | ✅ | ✅ | ❓ | 🟢 LOW (verify) |
| **TEMPORARY SEQUENCES** | V2, Firebird | ✅ | ❌ | ❌ | 🟡 MEDIUM |
| **DEFERRABLE constraints** | V2 | ✅ | ✅ | ❓ | 🟡 MEDIUM (verify) |
| **MySQL TABLE OPTIONS** | MySQL | ✅ | ❓ | ❓ | 🟡 MEDIUM (verify) |

**Verification Needed (marked ❓):**
- PARTITION BY: Check if bytecode generator emits and executor handles
- MATERIALIZED VIEWS: Check executor and catalog support
- DEFERRABLE constraints: Check catalog storage and executor enforcement
- MySQL TABLE OPTIONS: Check bytecode emission

---

## Implementation Requirements

### Phase 1: Critical Bug Fixes (Firebird Parser)

**Priority:** CRITICAL - Blocks basic Firebird functionality

1. **Fix `isNonReservedKeyword()` in firebird_parser.cpp**

```cpp
// Add to isNonReservedKeyword() function (line ~221):
case TokenType::KW_GEN_ID:           // Generator function
case TokenType::KW_GEN_UUID:         // UUID generation
case TokenType::KW_RDB_GET_CONTEXT:  // Context retrieval
case TokenType::KW_RDB_SET_CONTEXT:  // Context setting
```

2. **Verify RDB$DB_KEY and RDB$RECORD_VERSION Handling**
   - Ensure these parse as column references
   - Add tests for `SELECT RDB$DB_KEY FROM table`

3. **Test All Context Variables**
   - CURRENT_DATE through CURRENT_TRANSACTION (already implemented)
   - GEN_ID(), GEN_UUID()
   - RDB$GET_CONTEXT(), RDB$SET_CONTEXT()

### Phase 2: V2 Parser Feature Parity

**Priority:** HIGH - V2 must be superset of Firebird

1. **Implement Context Variables in V2**
   - CURRENT_CONNECTION
   - CURRENT_TRANSACTION
   - RDB$GET_CONTEXT (as regular function)
   - RDB$SET_CONTEXT (as regular function)
   - GEN_ID (as regular function)
   - GEN_UUID (as regular function)

2. **Implement Generator DDL**
   - CREATE SEQUENCE
   - DROP SEQUENCE
   - ALTER SEQUENCE
   - SET GENERATOR (legacy support)

3. **Implement Firebird DML Extensions**
   - UPDATE OR INSERT
   - MERGE (complete implementation)
   - RETURNING clause (complete all DML)

### Phase 3: PSQL Implementation (Both Parsers)

**Priority:** HIGH - Major feature gap

1. **CREATE FUNCTION / PROCEDURE Parsing**
   - Uncomment TODOs in parser_v2.cpp (lines 299-302)
   - Implement function/procedure body parsing
   - Populate existing AST nodes (ExecuteBlockStmt, etc.)

2. **Control Structure Parsing**
   - DECLARE VARIABLE
   - IF/THEN/ELSE
   - WHILE loops
   - FOR loops
   - FOR SELECT loops
   - Exception handling (WHEN...DO)

3. **EXECUTE BLOCK**
   - Parse anonymous blocks
   - Support parameter passing
   - Support RETURNS clause

4. **CREATE TRIGGER**
   - Parse trigger definition
   - Handle BEFORE/AFTER timing
   - Handle INSERT/UPDATE/DELETE events
   - Parse trigger body

### Phase 4: Testing and Validation

**Priority:** HIGH - Ensure correctness

1. **Context Variable Tests**
```sql
-- Test CURRENT_* variables
SELECT CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP FROM RDB$DATABASE;
SELECT CURRENT_USER, CURRENT_ROLE, CURRENT_CONNECTION, CURRENT_TRANSACTION FROM RDB$DATABASE;

-- Test RDB$GET_CONTEXT
SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB$DATABASE;
SELECT RDB$GET_CONTEXT('SYSTEM', 'ISOLATION_LEVEL') FROM RDB$DATABASE;

-- Test RDB$SET_CONTEXT
SELECT RDB$SET_CONTEXT('USER_SESSION', 'test', 'value') FROM RDB$DATABASE;
SELECT RDB$GET_CONTEXT('USER_SESSION', 'test') FROM RDB$DATABASE;

-- Test GEN_ID
SELECT GEN_ID(GEN_TEST, 1) FROM RDB$DATABASE;
SELECT GEN_ID(GEN_TEST, 0) FROM RDB$DATABASE;  -- Current value

-- Test GEN_UUID
SELECT GEN_UUID() FROM RDB$DATABASE;
```

2. **Generator/Sequence Tests**
```sql
CREATE SEQUENCE seq_test START WITH 100 INCREMENT BY 10;
SELECT NEXT VALUE FOR seq_test FROM RDB$DATABASE;
ALTER SEQUENCE seq_test RESTART WITH 500;
DROP SEQUENCE seq_test;
```

3. **PSQL Tests**
```sql
CREATE PROCEDURE test_proc (param1 INTEGER)
RETURNS (result VARCHAR(100))
AS
DECLARE VARIABLE local_var INTEGER;
BEGIN
  local_var = param1 * 2;
  result = 'Value: ' || CAST(local_var AS VARCHAR(10));
  SUSPEND;
END;

EXECUTE BLOCK (p1 INTEGER = ?)
RETURNS (output VARCHAR(100))
AS
BEGIN
  output = 'Input was: ' || CAST(p1 AS VARCHAR(10));
  SUSPEND;
END;
```

4. **Transaction Isolation Tests**
```sql
-- Verify READ COMMITTED variants
SET TRANSACTION READ COMMITTED RECORD_VERSION;
SET TRANSACTION READ COMMITTED NO_RECORD_VERSION WAIT;
SET TRANSACTION READ COMMITTED NO_RECORD_VERSION NO WAIT;
SET TRANSACTION READ COMMITTED LOCK TIMEOUT 5;

-- Verify SNAPSHOT
SET TRANSACTION SNAPSHOT;

-- Verify SNAPSHOT TABLE STABILITY
SET TRANSACTION SNAPSHOT TABLE STABILITY;

-- Verify RESERVING (Firebird-specific)
SET TRANSACTION
  SNAPSHOT
  RESERVING table1 FOR SHARED READ,
            table2 FOR PROTECTED WRITE;
```

---

## Compliance Matrix

| Feature | Firebird Parser | V2 Parser | SBLR Opcode | Executor | Priority |
|---------|----------------|-----------|-------------|----------|----------|
| **Context Variables** |
| CURRENT_DATE/TIME/TIMESTAMP | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| CURRENT_USER/ROLE | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| CURRENT_CONNECTION/TRANSACTION | ✅ | ⚠️ | ✅ | ✅ | HIGH |
| GEN_ID() | ❌ BUG | ❌ | ✅ | ⚠️ | **CRITICAL** |
| GEN_UUID() | ❌ BUG | ❌ | ✅ | ⚠️ | **CRITICAL** |
| RDB$GET_CONTEXT() | ❌ BUG | ❌ | ✅ | ⚠️ | **CRITICAL** |
| RDB$SET_CONTEXT() | ❌ BUG | ❌ | ✅ | ⚠️ | **CRITICAL** |
| RDB$DB_KEY | ⚠️ | ⚠️ | ✅ | ⚠️ | HIGH |
| RDB$RECORD_VERSION | ⚠️ | ⚠️ | ✅ | ⚠️ | HIGH |
| **Generators/Sequences** |
| CREATE SEQUENCE | ⚠️ | ❌ | ✅ | ⚠️ | HIGH |
| DROP SEQUENCE | ❌ | ❌ | ✅ | ⚠️ | HIGH |
| ALTER SEQUENCE | ⚠️ | ❌ | ✅ | ⚠️ | HIGH |
| NEXT VALUE FOR | ❌ | ❌ | ✅ | ⚠️ | MEDIUM |
| **DML Extensions** |
| UPDATE OR INSERT | ⚠️ | ❌ | ✅ | ⚠️ | HIGH |
| MERGE | ✅ | ❌ | ✅ | ❌ | **CRITICAL** |
| RETURNING (all DML) | ✅ | ⚠️ | ✅ | ⚠️ | HIGH |
| **PSQL** |
| CREATE FUNCTION | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| CREATE PROCEDURE | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| EXECUTE BLOCK | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| CREATE TRIGGER | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| DECLARE VARIABLE | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| IF/WHILE/FOR | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| CASE (PSQL statement) | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| Exception Handling | ❌ | ❌ | ✅ | ⚠️ | **CRITICAL** |
| **Transaction Control** |
| RECORD_VERSION variant | ✅ | ⚠️ | ✅ | ✅ | HIGH |
| NO_RECORD_VERSION variant | ✅ | ⚠️ | ✅ | ✅ | HIGH |
| WAIT/NO WAIT | ✅ | ⚠️ | ✅ | ✅ | HIGH |
| LOCK TIMEOUT | ✅ | ⚠️ | ✅ | ✅ | HIGH |
| RESERVING clause | ⚠️ | ❌ | ⚠️ | ⚠️ | MEDIUM |
| **Parsed-But-Not-Implemented (CRITICAL GAPS)** |
| TEMPORARY TABLES | ⚠️ PARSED | ⚠️ PARSED | ❌ | ❌ | **CRITICAL** |
| ON COMMIT (GTT) | ⚠️ PARSED | ❌ | ❌ | ❌ | **CRITICAL** |
| UNLOGGED TABLES | ❌ | ⚠️ PARSED | ❌ | ❌ | MEDIUM |
| PARTITION BY | ❌ | ⚠️ PARSED | ⚠️ | ❓ | MEDIUM |
| WITH CHECK OPTION | ❌ | ⚠️ PARSED | ❌ | ❌ | MEDIUM |
| TEMPORARY VIEWS | ⚠️ PARSED | ⚠️ PARSED | ❌ | ❌ | MEDIUM |
| MATERIALIZED VIEWS | ❌ | ⚠️ PARSED | ✅ | ❓ | LOW (verify) |
| TEMPORARY SEQUENCES | ⚠️ PARSED | ⚠️ PARSED | ❌ | ❌ | MEDIUM |
| DEFERRABLE constraints | ❌ | ⚠️ PARSED | ✅ | ❓ | MEDIUM (verify) |

**Legend:**
- ✅ Fully implemented
- ⚠️ Partial or needs verification
- ⚠️ PARSED - Parser accepts syntax but feature not implemented
- ❌ Not implemented
- ❌ BUG - Implemented but broken
- ❓ Unknown - Needs verification

---

## Critical Bug Summary

**Immediate fixes required in Firebird parser (firebird_parser.cpp):**

1. **Line ~221** - Add to `isNonReservedKeyword()`:
   - `KW_GEN_ID`
   - `KW_GEN_UUID`
   - `KW_RDB_GET_CONTEXT`
   - `KW_RDB_SET_CONTEXT`

**Impact:** Without this fix, the following Firebird SQL statements **WILL NOT PARSE**:
```sql
SELECT GEN_ID(gen_test, 1) FROM RDB$DATABASE;  -- FAILS
SELECT GEN_UUID() FROM RDB$DATABASE;            -- FAILS
SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB$DATABASE;  -- FAILS
```

---

## References

### Firebird Documentation
- [Firebird ACID Compliance](https://firebirdsql.org/file/documentation/papers_presentations/html/paper-fbent-acid.html)
- [Firebird 5.0 Language Reference - Functions](https://www.firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-functions.html)
- [RDB$GET_CONTEXT() Documentation](https://firebirdsql.org/refdocs/langrefupd20-get-context.html)
- [RDB$SET_CONTEXT() Documentation](https://www.firebirdsql.org/refdocs/langrefupd20-set-context.html)
- [Context Variables Reference](https://firebirdsql.org/refdocs/langrefupd25-variables.html)
- [Firebird 5.0 Release Notes](https://firebirdsql.org/file/documentation/release_notes/html/en/5_0/rlsnotes50.html)

### Transaction Documentation
- [IBSurgeon: Transactions in Firebird](https://ib-aid.com/en/transactions-in-firebird-acid-isolation-levels-deadlocks-and-update-conflicts-resolution/)
- [IBExpert: Transaction Options Explained](http://ibexpert.com/docu/doku.php?id=01-documentation:01-05-database-technology:database-technology-articles:firebird-interbase-server:transaction-options-explained)
- [IBPhoenix: Understanding Firebird Transactions](https://www.ibphoenix.com/articles/art-00000400)

### Project Documentation
- `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `/docs/audit/parsers/FirebirdSQL/SUMMARY.md`
- `/docs/audit/parsers/V2/SUMMARY.md`
- `/docs/audit/parsers/COMPARISON_MATRIX.md`
- `/docs/audit/parsers/SBLR_OPCODE_MAPPING.md`

---

## Implementation Checklist

### Immediate (Critical Bugs - Parser Fixes)
- [ ] Fix Firebird parser `isNonReservedKeyword()` - Add GEN_ID, GEN_UUID, RDB$GET_CONTEXT, RDB$SET_CONTEXT
- [ ] Test GEN_ID() parsing and execution
- [ ] Test GEN_UUID() parsing and execution
- [ ] Test RDB$GET_CONTEXT() parsing and execution
- [ ] Test RDB$SET_CONTEXT() parsing and execution

### Critical (Parsed-But-Not-Implemented - SILENT FAILURES)
- [ ] **TEMPORARY TABLES** - Implement full stack (AST → bytecode → executor → catalog)
  - [ ] Extend AST with TemporaryTableType and OnCommitAction enums
  - [ ] Update bytecode format with temporary flags
  - [ ] Extend CatalogManager::createTable() with temporary parameters
  - [ ] Implement session/transaction tracking in ConnectionContext
  - [ ] Implement executor support for temporary table semantics
  - [ ] Implement ON COMMIT DELETE ROWS / PRESERVE ROWS / DROP
  - [ ] Add visibility isolation (temp tables only visible to creating session)
  - [ ] Implement cleanup on session disconnect and transaction commit/rollback
  - [ ] Add comprehensive test suite (see specification section)
- [ ] **ON COMMIT clause** (Firebird GTT) - Store in AST and emit to bytecode
  - [ ] Fix Firebird parser to store ON COMMIT action in AST
  - [ ] Integrate with temporary table implementation above

### High Priority (Feature Parity)
- [ ] Implement CURRENT_CONNECTION in V2 parser
- [ ] Implement CURRENT_TRANSACTION in V2 parser
- [ ] Implement context functions in V2 (as regular functions, not keywords)
- [ ] Implement CREATE/DROP/ALTER SEQUENCE in both parsers
- [ ] Implement UPDATE OR INSERT in V2 parser
- [ ] Complete MERGE implementation (both parsers + executor)
- [ ] Complete RETURNING clause (all DML in V2)

### Critical (PSQL)
- [ ] Implement CREATE FUNCTION parsing (both parsers)
- [ ] Implement CREATE PROCEDURE parsing (both parsers)
- [ ] Implement EXECUTE BLOCK parsing (both parsers)
- [ ] Implement CREATE TRIGGER parsing (both parsers)
- [ ] Implement PSQL control structures (DECLARE, IF, CASE, WHILE, FOR, exception handling)
- [ ] Connect parsers to existing AST nodes (ExecuteBlockStmt, IfStmt, etc.)
- [ ] Implement executor support for PSQL opcodes

### Medium Priority (Other Parsed-But-Not-Implemented Features)
- [ ] **UNLOGGED TABLES** - Implement optional post-gold write-after log bypass for performance
  - [ ] Emit unlogged flag in bytecode
  - [ ] Store in catalog metadata
  - [ ] Skip optional post-gold write-after log writes in storage engine
  - [ ] Truncate unlogged tables for durability/PITR if optional post-gold write-after log is introduced
- [ ] **WITH CHECK OPTION** (views) - Enforce view constraints on INSERT/UPDATE
  - [ ] Emit WITH CHECK OPTION flags to bytecode
  - [ ] Store in view metadata
  - [ ] Implement enforcement in executor
  - [ ] Support LOCAL vs CASCADED variants
- [ ] **TEMPORARY VIEWS** - Implement alongside temporary tables
- [ ] **TEMPORARY SEQUENCES** - Implement alongside temporary tables
- [ ] Verify **PARTITION BY** implementation (may already work)
  - [ ] Check bytecode generator emits PARTITION_BY opcode
  - [ ] Check executor handles partition metadata
  - [ ] Check catalog stores partition information
  - [ ] Add tests if working, or implement if not
- [ ] Verify **MATERIALIZED VIEWS** implementation (may already work)
  - [ ] Test CREATE MATERIALIZED VIEW
  - [ ] Test REFRESH MATERIALIZED VIEW
  - [ ] Check catalog stores materialized view metadata
  - [ ] Add tests if working, or implement if not
- [ ] Verify **DEFERRABLE constraints** implementation (may already work)
  - [ ] Check catalog stores deferrability flags
  - [ ] Check executor enforces deferred checking
  - [ ] Test SET CONSTRAINTS DEFERRED/IMMEDIATE
  - [ ] Add tests if working, or implement if not
- [ ] Verify **MySQL TABLE OPTIONS** implementation
  - [ ] Audit MySQL parser bytecode emission
  - [ ] Check if ENGINE, CHARSET, COLLATE, COMMENT are stored
  - [ ] Document which options are supported vs ignored

### Testing
- [ ] Create comprehensive context variable test suite
- [ ] Create generator/sequence test suite
- [ ] Create PSQL test suite
- [ ] Create transaction isolation test suite
- [ ] **Create temporary table test suite** (see specification for test cases)
- [ ] **Create UNLOGGED table test suite**
- [ ] **Create partitioned table test suite** (if implemented)
- [ ] **Create materialized view test suite** (if implemented)
- [ ] **Create deferrable constraints test suite** (if implemented)
- [ ] Verify all Firebird 5.0 features work
- [ ] Verify V2 is superset of Firebird

---

**End of Specification**
**Next Steps:** Prioritize fixes, assign implementation tasks, create GitHub issues
