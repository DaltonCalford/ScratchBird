# ScratchBird Parser - SET Commands Complete Audit

This document provides a comprehensive audit of all SET command implementations in the ScratchBird parser.

**Source Files:**
- Parser Implementation: `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp`
- AST Definitions: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h`
- Parser Header: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/parser.h`
- Token Definitions: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/token.h`

**Audit Date:** 2025-12-06

---

## Table of Contents

1. [SET TRANSACTION](#set-transaction)
2. [SET SQL DIALECT](#set-sql-dialect)
3. [SET NAMES](#set-names)
4. [SET LOCAL_TIMEOUT](#set-local_timeout)
5. [SET ROLE](#set-role)
6. [SET SESSION AUTHORIZATION](#set-session-authorization)
7. [SET CONSTRAINTS](#set-constraints)
8. [RESET Commands](#reset-commands)
9. [Dispatch Logic](#dispatch-logic)
10. [AST Nodes](#ast-nodes)
11. [Supporting Types](#supporting-types)

---

## SET TRANSACTION

**Parser Method:** `parseSetTransaction()` (lines 4301-4487)

**AST Node:** `SetTransactionStmt` (ast.h lines 3288-3332)

**AST Kind:** `ASTKind::SET_TRANSACTION`

### Syntax (BNF)

```bnf
<set_transaction> ::=
    SET TRANSACTION [ <transaction_mode> ]
                    [ <wait_mode> ]
                    [ <isolation_clause> ]
                    [ <lock_timeout_clause> ]
                    [ <reserving_clause> ]

<transaction_mode> ::=
    READ WRITE
  | READ ONLY
  | READ COMMITTED

<wait_mode> ::=
    NOT WAIT

<isolation_clause> ::=
    ISOLATION LEVEL <isolation_level>

<isolation_level> ::=
    READ COMMITTED
  | SNAPSHOT
  | SNAPSHOT TABLE STABILITY

<lock_timeout_clause> ::=
    LOCK TIMEOUT <timeout_seconds>

<timeout_seconds> ::= <integer_literal>

<reserving_clause> ::=
    RESERVING <table_reservation> { ',' <table_reservation> }

<table_reservation> ::=
    <table_name> FOR <lock_mode> <access_mode>

<lock_mode> ::=
    SHARED
  | PROTECTED

<access_mode> ::=
    READ
  | WRITE
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator)
- `KW_TRANSACTION` - TRANSACTION keyword (required)
- `KW_READ` - READ keyword (for mode and isolation)
- `KW_WRITE` - WRITE keyword (for mode and access)
- `KW_ONLY` - ONLY keyword (READ ONLY)
- `KW_COMMITTED` - COMMITTED keyword (READ COMMITTED)
- `KW_NOT` - NOT keyword (for NO WAIT - **NOTE: Bug/inconsistency**)
- `KW_WAIT` - WAIT keyword
- `KW_ISOLATION` - ISOLATION keyword
- `KW_LEVEL` - LEVEL keyword
- `KW_SNAPSHOT` - SNAPSHOT keyword
- `KW_TABLE` - TABLE keyword (SNAPSHOT TABLE STABILITY)
- `KW_STABILITY` - STABILITY keyword
- `KW_LOCK` - LOCK keyword
- `KW_TIMEOUT` - TIMEOUT keyword
- `KW_RESERVING` - RESERVING keyword
- `KW_SHARED` - SHARED keyword
- `KW_PROTECTED` - PROTECTED keyword
- `KW_FOR` - FOR keyword
- `INTEGER_LITERAL` - Integer token type (for timeout value)
- `IDENTIFIER` - Identifier token type (for table names)
- `COMMA` - Comma separator

### Default Values

- **Transaction Mode:** `TransactionMode::READ_WRITE`
- **Isolation Level:** `IsolationLevel::READ_COMMITTED`
- **Wait Mode:** `true` (WAIT is the default)
- **Lock Timeout:** `0` (no timeout)
- **Table Reservations:** Empty vector

### Special Parsing Rules

1. **Ambiguous READ Keyword:**
   - Lines 4324-4343: When `READ` is encountered, the parser must look ahead:
     - `READ WRITE` → transaction mode
     - `READ ONLY` → transaction mode
     - `READ COMMITTED` → isolation level (special handling)
   - If `READ COMMITTED` is detected without `ISOLATION LEVEL` prefix, the isolation level is set directly.

2. **NO WAIT vs NOT WAIT Bug:**
   - **Comment:** Says "NO WAIT" (lines 4303, 4346)
   - **Implementation:** Uses `KW_NOT` instead of `KW_NO` (line 4347)
   - This is an **inconsistency** - Firebird uses "NO WAIT" but the parser implements "NOT WAIT"

3. **Lock Timeout Validation:**
   - Lines 4409-4417: Must be an integer literal
   - Converted to `uint32_t`
   - Value is stored in seconds

4. **RESERVING Clause:**
   - Lines 4421-4482: Comma-separated list of table reservations
   - Each reservation requires: `table_name FOR lock_mode access_mode`
   - Lock mode must be SHARED or PROTECTED
   - Access mode must be READ or WRITE
   - Do-while loop allows multiple tables separated by commas

5. **Order Independence:**
   - Clauses can appear in any order
   - Parser uses `if` statements (not `else if`) for most clauses
   - Exception: READ COMMITTED can be parsed with or without ISOLATION LEVEL prefix

### Data Types

- **mode_:** `TransactionMode` enum (READ_WRITE = 0, READ_ONLY = 1)
- **isolation_:** `IsolationLevel` enum (READ_COMMITTED = 0, SNAPSHOT = 1, SNAPSHOT_TABLE_STABILITY = 2)
- **wait_:** `bool` (true = WAIT, false = NO WAIT)
- **lock_timeout_:** `uint32_t` (seconds, 0 = no wait, UINT32_MAX = wait forever per comment)
- **table_reservations_:** `std::vector<TableReservation>`

### Examples

```sql
-- Basic transaction mode
SET TRANSACTION READ ONLY;
SET TRANSACTION READ WRITE;

-- With isolation level
SET TRANSACTION ISOLATION LEVEL SNAPSHOT;
SET TRANSACTION ISOLATION LEVEL SNAPSHOT TABLE STABILITY;
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;

-- Combined (from tests line 239)
SET TRANSACTION READ WRITE ISOLATION LEVEL SNAPSHOT LOCK TIMEOUT 30;

-- With lock timeout (from tests line 211)
SET TRANSACTION LOCK TIMEOUT 45;

-- With RESERVING clause (from tests line 222)
SET TRANSACTION RESERVING orders FOR PROTECTED WRITE;

-- Complex example with all clauses (from tests line 239)
SET TRANSACTION READ WRITE
    ISOLATION LEVEL SNAPSHOT
    LOCK TIMEOUT 30
    RESERVING orders FOR SHARED READ,
              customers FOR PROTECTED WRITE;

-- With NO WAIT (actually NOT WAIT in implementation)
SET TRANSACTION NOT WAIT;
```

### Implementation Notes

1. **Phase:** Phase 3 Task 3.6
2. **Firebird Compatibility:** High - supports all major Firebird transaction options
3. **Bug:** Comment says "NO WAIT" but implementation uses "NOT WAIT" (KW_NOT instead of KW_NO)
4. **Dispatched From:** parseStatement() line 586 (fallback case after specific SET checks)

---

## SET SQL DIALECT

**Parser Method:** `parseSetSqlDialect()` (lines 4489-4528)

**AST Node:** `SetSqlDialectStmt` (ast.h lines 3336-3350)

**AST Kind:** `ASTKind::SET_SQL_DIALECT`

### Syntax (BNF)

```bnf
<set_sql_dialect> ::=
    SET SQL DIALECT <dialect_number>

<dialect_number> ::=
    1 | 2 | 3
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator)
- `KW_SQL` - SQL keyword (required)
- `KW_DIALECT` - DIALECT keyword (required)
- `INTEGER_LITERAL` - Integer token type (for dialect number)

### Validation Rules

1. **Dialect Range:** Lines 4519-4524
   - Must be an integer literal
   - Value must be 1, 2, or 3
   - Values outside this range produce an error

2. **Data Type:** Lines 4516, 4527
   - Parsed as `int64_t`
   - Stored as `uint8_t`
   - Explicit cast from int64_t to uint8_t

### Data Types

- **dialect_:** `uint8_t` (1, 2, or 3)

### Examples

```sql
-- Valid dialects
SET SQL DIALECT 1;
SET SQL DIALECT 2;
SET SQL DIALECT 3;

-- Invalid (will error)
SET SQL DIALECT 0;   -- Error: SQL dialect must be 1, 2, or 3
SET SQL DIALECT 4;   -- Error: SQL dialect must be 1, 2, or 3
SET SQL DIALECT abc; -- Error: Expected dialect number
```

### Implementation Notes

1. **Purpose:** Firebird ISQL compatibility (from comment line 3335)
2. **Scope:** Sets the SQL dialect for the current session
3. **Dispatched From:** parseStatement() line 574 (when KW_SQL follows KW_SET)
4. **Tests:** test_show_set_commands.cpp lines 453-455, 460-470

---

## SET NAMES

**Parser Method:** `parseSetNames()` (lines 4530-4563)

**AST Node:** `SetNamesStmt` (ast.h lines 3353-3367)

**AST Kind:** `ASTKind::SET_NAMES`

### Syntax (BNF)

```bnf
<set_names> ::=
    SET NAMES <charset_name>

<charset_name> ::=
    <string_literal>
  | <identifier>
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator)
- `KW_NAMES` - NAMES keyword (required)
- `STRING_LITERAL` - String literal token type (for charset name)
- `IDENTIFIER` - Identifier token type (for charset name)

### Parsing Rules

1. **Flexible Charset Format:** Lines 4542-4559
   - Accepts either string literal: `SET NAMES 'UTF8'`
   - Or identifier: `SET NAMES UTF8`
   - Both forms store the value as `StringPool::StringId`
   - Tried in order: STRING_LITERAL first, then IDENTIFIER

2. **No Validation:**
   - Parser does not validate charset name
   - Any string or identifier is accepted
   - Validation presumably happens at execution time

### Data Types

- **charset_name_:** `StringPool::StringId` (reference to interned string)

### Examples

```sql
-- With identifier (from tests line 481-483)
SET NAMES UTF8;
SET NAMES ISO8859_1;
SET NAMES WIN1252;

-- With string literal (also valid)
SET NAMES 'UTF8';
SET NAMES 'ISO8859_1';
SET NAMES 'WIN1252';
```

### Implementation Notes

1. **Purpose:** Sets the character set for the connection (from comment line 3352)
2. **Dispatched From:** parseStatement() line 578 (when KW_NAMES follows KW_SET)
3. **Tests:** test_show_set_commands.cpp lines 481-495
4. **Firebird Compatibility:** Standard Firebird syntax

---

## SET LOCAL_TIMEOUT

**Parser Method:** `parseSetLocalTimeout()` (lines 4565-4597)

**AST Node:** `SetLocalTimeoutStmt` (ast.h lines 3370-3384)

**AST Kind:** `ASTKind::SET_LOCAL_TIMEOUT`

### Syntax (BNF)

```bnf
<set_local_timeout> ::=
    SET LOCAL_TIMEOUT <timeout_seconds>

<timeout_seconds> ::= <non_negative_integer>
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator)
- `KW_LOCAL_TIMEOUT` - LOCAL_TIMEOUT keyword (required, single token)
- `INTEGER_LITERAL` - Integer token type (for timeout value)

### Validation Rules

1. **Type Check:** Lines 4578-4583
   - Must be an integer literal
   - Error if non-integer provided

2. **Range Check:** Lines 4588-4593
   - Value must be non-negative (>= 0)
   - Negative values produce error

3. **Data Type Conversion:** Lines 4585, 4596
   - Parsed as `int64_t`
   - Stored as `uint32_t`
   - Explicit cast from int64_t to uint32_t

### Data Types

- **timeout_seconds_:** `uint32_t` (timeout in seconds)

### Examples

```sql
-- Valid timeouts (from tests line 506-508)
SET LOCAL_TIMEOUT 30;
SET LOCAL_TIMEOUT 0;     -- No timeout
SET LOCAL_TIMEOUT 3600;  -- 1 hour

-- Invalid
SET LOCAL_TIMEOUT -1;   -- Error: Timeout value must be non-negative
SET LOCAL_TIMEOUT abc;  -- Error: Expected timeout value
```

### Implementation Notes

1. **Purpose:** Sets statement timeout in seconds (from comment line 3369)
2. **Dispatched From:** parseStatement() line 582 (when KW_LOCAL_TIMEOUT follows KW_SET)
3. **Tests:** test_show_set_commands.cpp lines 506-523
4. **Token Note:** LOCAL_TIMEOUT is a single keyword token, not two separate tokens

---

## SET ROLE

**Parser Method:** `parseSetRole()` (lines 9240-9268)

**AST Node:** `SetRoleStmt` (ast.h lines 4370-4390)

**AST Kind:** `ASTKind::SET_ROLE`

### Syntax (BNF)

```bnf
<set_role> ::=
    SET ROLE <role_name>
  | RESET ROLE

<role_name> ::= <identifier>
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator for SET ROLE)
- `KW_RESET` - RESET keyword (initiator for RESET ROLE)
- `KW_ROLE` - ROLE keyword (required)
- `IDENTIFIER` - Identifier token type (for role name)

### Parsing Rules

1. **Dual Entry Point:** Lines 9240-9268
   - Can be called after `SET` or `RESET`
   - Checks `previous().type` to determine which (line 9244)
   - `is_reset` flag set based on previous token

2. **SET ROLE:** Lines 9253-9263
   - Requires role name (identifier)
   - Stores role name as `StringPool::StringId`

3. **RESET ROLE:** Lines 9244, 9252
   - No role name required
   - `rolename_` set to 0
   - `is_reset_` flag set to true

### Data Types

- **rolename_:** `StringPool::StringId` (0 for RESET)
- **is_reset_:** `bool` (true for RESET ROLE, false for SET ROLE)

### Examples

```sql
-- Set a role (from tests line 393)
SET ROLE myrole;
SET ROLE admin_role;
SET ROLE public;

-- Reset role (from tests line 394)
RESET ROLE;
```

### Implementation Notes

1. **Dispatched From:**
   - parseStatement() line 562 (SET ROLE via check for KW_ROLE after KW_SET)
   - parseStatement() line 593 (RESET ROLE via check for KW_ROLE after KW_RESET)
2. **Purpose:** Switch to a different role or reset to default
3. **Tests:** test_security_phase2.cpp line 393-394
4. **Standard SQL:** Compatible with SQL standard SET ROLE

---

## SET SESSION AUTHORIZATION

**Parser Method:** `parseSetSessionAuth()` (lines 9270-9304)

**AST Node:** `SetSessionAuthStmt` (ast.h lines 4393-4413)

**AST Kind:** `ASTKind::SET_SESSION_AUTH`

### Syntax (BNF)

```bnf
<set_session_authorization> ::=
    SET SESSION AUTHORIZATION <username>
  | RESET SESSION AUTHORIZATION

<username> ::= <identifier>
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator for SET)
- `KW_RESET` - RESET keyword (initiator for RESET)
- `KW_SESSION` - SESSION keyword (required)
- `KW_AUTHORIZATION` - AUTHORIZATION keyword (required)
- `IDENTIFIER` - Identifier token type (for username)

### Parsing Rules

1. **Dual Entry Point:** Lines 9270-9304
   - Can be called after `SET` or `RESET`
   - Checks `previous().type` to determine which (line 9274)
   - `is_reset` flag set based on previous token

2. **Three-Keyword Sequence:**
   - Must consume `SESSION` (line 9276)
   - Must consume `AUTHORIZATION` (line 9282)
   - Keywords must appear in exact order

3. **SET SESSION AUTHORIZATION:** Lines 9288-9299
   - Requires username (identifier)
   - Stores username as `StringPool::StringId`

4. **RESET SESSION AUTHORIZATION:** Lines 9274, 9288
   - No username required
   - `username_` set to 0
   - `is_reset_` flag set to true

### Data Types

- **username_:** `StringPool::StringId` (0 for RESET)
- **is_reset_:** `bool` (true for RESET, false for SET)

### Examples

```sql
-- Set session authorization (from tests line 395)
SET SESSION AUTHORIZATION myuser;
SET SESSION AUTHORIZATION admin;
SET SESSION AUTHORIZATION john_doe;

-- Reset session authorization (from tests line 396)
RESET SESSION AUTHORIZATION;
```

### Implementation Notes

1. **Dispatched From:**
   - parseStatement() line 566 (SET SESSION via check for KW_SESSION after KW_SET)
   - parseStatement() line 597 (RESET SESSION via check for KW_SESSION after KW_RESET)
2. **Purpose:** Change the session's authorization identifier
3. **Tests:** test_security_phase2.cpp lines 395-396
4. **Security:** Likely requires special privileges to execute

---

## SET CONSTRAINTS

**Parser Method:** `parseSetConstraints()` (lines 9307-9360)

**AST Node:** `SetConstraintsStmt` (ast.h lines 4417-4441)

**AST Kind:** `ASTKind::SET_CONSTRAINTS`

### Syntax (BNF)

```bnf
<set_constraints> ::=
    SET CONSTRAINTS <constraint_target> <constraint_timing>

<constraint_target> ::=
    ALL
  | <constraint_name_list>

<constraint_name_list> ::=
    <constraint_name> { ',' <constraint_name> }

<constraint_name> ::= <identifier>

<constraint_timing> ::=
    DEFERRED
  | IMMEDIATE
```

### Keywords and Tokens

- `KW_SET` - SET keyword (initiator)
- `KW_CONSTRAINTS` - CONSTRAINTS keyword (required, plural)
- `KW_ALL` - ALL keyword (for all constraints)
- `KW_DEFERRED` - DEFERRED keyword (timing)
- `KW_IMMEDIATE` - IMMEDIATE keyword (timing)
- `IDENTIFIER` - Identifier token type (for constraint names)
- `COMMA` - Comma separator

### Parsing Rules

1. **Constraint Target:** Lines 9318-9339
   - If `ALL` is matched (line 9321): set `all_constraints = true`, empty list
   - Otherwise: parse comma-separated list of constraint names
   - Do-while loop (line 9328) allows multiple names

2. **Constraint Timing:** Lines 9341-9356
   - Must have either `DEFERRED` or `IMMEDIATE`
   - `DEFERRED` sets `deferred = true` (line 9345)
   - `IMMEDIATE` sets `deferred = false` (line 9349)
   - Missing timing keyword is an error (line 9353)

3. **Mutually Exclusive Target:**
   - Either `all_constraints = true` with empty list
   - Or `all_constraints = false` with non-empty constraint_names list

### Data Types

- **all_constraints_:** `bool` (true if ALL was specified)
- **constraint_names_:** `std::vector<StringPool::StringId>` (empty if all_constraints_ is true)
- **deferred_:** `bool` (true = DEFERRED, false = IMMEDIATE)

### Examples

```sql
-- All constraints
SET CONSTRAINTS ALL DEFERRED;
SET CONSTRAINTS ALL IMMEDIATE;

-- Specific constraints
SET CONSTRAINTS fk_orders_customer DEFERRED;
SET CONSTRAINTS pk_users, fk_posts_user IMMEDIATE;

-- Multiple constraints (inferred from syntax)
SET CONSTRAINTS c1, c2, c3 DEFERRED;
```

### Implementation Notes

1. **Purpose:** Controls when deferrable constraints are checked (from comment line 4416)
2. **Phase:** P2-7 (Priority 2, Task 7)
3. **Dispatched From:** parseStatement() line 570 (when KW_CONSTRAINTS follows KW_SET)
4. **Standard SQL:** Compatible with SQL standard SET CONSTRAINTS
5. **Scope:** Only affects constraints declared as DEFERRABLE
6. **Tests:** test_constraint_enforcement.cpp line 548 (reference)
7. **BNF Reference:** docs/specifications/00_GRAMMAR_BNF.md line 1217

---

## RESET Commands

RESET commands are special cases of SET commands that restore default values.

### Supported RESET Commands

1. **RESET ROLE** - Reset to default role
2. **RESET SESSION AUTHORIZATION** - Reset to original session user

### Dispatch Logic

Lines 589-604 in parseStatement():

```cpp
else if (match(TokenType::KW_RESET))
{
    if (check(TokenType::KW_ROLE))
    {
        stmt = parseSetRole();  // Will handle RESET internally
    }
    else if (check(TokenType::KW_SESSION))
    {
        stmt = parseSetSessionAuth();  // Will handle RESET internally
    }
    else
    {
        error("Expected ROLE or SESSION after RESET");
        synchronize();
    }
}
```

### Shared Implementation

- `parseSetRole()` handles both SET ROLE and RESET ROLE
- `parseSetSessionAuth()` handles both SET SESSION AUTHORIZATION and RESET SESSION AUTHORIZATION
- Both methods check `previous().type == TokenType::KW_RESET` to determine behavior
- RESET variant does not require an argument (name/username)
- AST nodes store `is_reset_` flag and set name/username to 0

### Not Supported

The following RESET commands are NOT implemented:
- RESET TRANSACTION (not a standard pattern)
- RESET SQL DIALECT
- RESET NAMES
- RESET LOCAL_TIMEOUT
- RESET CONSTRAINTS

---

## Dispatch Logic

The main `parseStatement()` method (lines 558-604) dispatches SET and RESET commands.

### SET Command Dispatch

```cpp
else if (match(TokenType::KW_SET))
{
    if (check(TokenType::KW_ROLE))
    {
        stmt = parseSetRole();                     // Line 562
    }
    else if (check(TokenType::KW_SESSION))
    {
        stmt = parseSetSessionAuth();              // Line 566
    }
    else if (check(TokenType::KW_CONSTRAINTS))
    {
        stmt = parseSetConstraints();              // Line 570
    }
    else if (check(TokenType::KW_SQL))
    {
        stmt = parseSetSqlDialect();               // Line 574
    }
    else if (check(TokenType::KW_NAMES))
    {
        stmt = parseSetNames();                    // Line 578
    }
    else if (check(TokenType::KW_LOCAL_TIMEOUT))
    {
        stmt = parseSetLocalTimeout();             // Line 582
    }
    else
    {
        stmt = parseSetTransaction();              // Line 586 (fallback)
    }
}
```

### RESET Command Dispatch

```cpp
else if (match(TokenType::KW_RESET))
{
    if (check(TokenType::KW_ROLE))
    {
        stmt = parseSetRole();                     // Line 593
    }
    else if (check(TokenType::KW_SESSION))
    {
        stmt = parseSetSessionAuth();              // Line 597
    }
    else
    {
        error("Expected ROLE or SESSION after RESET");  // Line 601
        synchronize();
    }
}
```

### Dispatch Order and Precedence

1. **Specific SET Commands First:** Check for specific keywords after SET
2. **Fallback to SET TRANSACTION:** If no specific keyword matches, assume SET TRANSACTION
3. **This means:**
   - SET ROLE, SET SESSION, SET CONSTRAINTS, SET SQL, SET NAMES, SET LOCAL_TIMEOUT are dispatched by keyword
   - SET TRANSACTION is the default/fallback
   - SET TRANSACTION does not require checking for KW_TRANSACTION in dispatch (but parser method does)

### RESET Constraints

- Only KW_ROLE and KW_SESSION are valid after RESET
- Any other keyword produces an error
- No fallback for RESET

---

## AST Nodes

All SET command AST nodes inherit from `Statement` base class.

### SetTransactionStmt

**File:** ast.h lines 3288-3332

**Fields:**
- `TransactionMode mode_` - READ_WRITE or READ_ONLY (default: READ_WRITE)
- `IsolationLevel isolation_` - READ_COMMITTED, SNAPSHOT, or SNAPSHOT_TABLE_STABILITY (default: READ_COMMITTED)
- `bool wait_` - Wait mode (default: true)
- `uint32_t lock_timeout_` - Lock timeout in seconds (default: 0)
- `std::vector<TableReservation> table_reservations_` - RESERVING clause tables (default: empty)

**Methods:**
- `TransactionMode mode() const`
- `IsolationLevel isolation() const`
- `bool wait() const`
- `uint32_t lockTimeout() const`
- `const std::vector<TableReservation>& tableReservations() const`
- `void accept(ASTVisitor* visitor) override`

### SetSqlDialectStmt

**File:** ast.h lines 3336-3350

**Fields:**
- `uint8_t dialect_` - SQL dialect (1, 2, or 3)

**Methods:**
- `uint8_t dialect() const`
- `void accept(ASTVisitor* visitor) override`

**Comment:** "Firebird ISQL compatibility" (line 3334)

### SetNamesStmt

**File:** ast.h lines 3353-3367

**Fields:**
- `StringPool::StringId charset_name_` - Character set name

**Methods:**
- `StringPool::StringId charsetName() const`
- `void accept(ASTVisitor* visitor) override`

**Comment:** "Character set for the connection" (line 3352)

### SetLocalTimeoutStmt

**File:** ast.h lines 3370-3384

**Fields:**
- `uint32_t timeout_seconds_` - Statement timeout in seconds

**Methods:**
- `uint32_t timeoutSeconds() const`
- `void accept(ASTVisitor* visitor) override`

**Comment:** "Statement timeout in seconds" (line 3369)

### SetRoleStmt

**File:** ast.h lines 4370-4390

**Fields:**
- `StringPool::StringId rolename_` - Role name (0 for RESET)
- `bool is_reset_` - True if RESET ROLE, false if SET ROLE

**Methods:**
- `StringPool::StringId rolename() const`
- `bool isReset() const`
- `void accept(ASTVisitor* visitor) override`

### SetSessionAuthStmt

**File:** ast.h lines 4393-4413

**Fields:**
- `StringPool::StringId username_` - Username (0 for RESET)
- `bool is_reset_` - True if RESET, false if SET

**Methods:**
- `StringPool::StringId username() const`
- `bool isReset() const`
- `void accept(ASTVisitor* visitor) override`

### SetConstraintsStmt

**File:** ast.h lines 4417-4441

**Fields:**
- `bool all_constraints_` - True if ALL was specified
- `std::vector<StringPool::StringId> constraint_names_` - Constraint names (empty if all_constraints_ is true)
- `bool deferred_` - True = DEFERRED, false = IMMEDIATE

**Methods:**
- `bool allConstraints() const`
- `const std::vector<StringPool::StringId>& constraintNames() const`
- `bool isDeferred() const`
- `void accept(ASTVisitor* visitor) override`

**Comment:** "P2-7: SET CONSTRAINTS statement" (line 4415)

---

## Supporting Types

### TransactionMode Enum

**File:** ast.h lines 2641-2645

```cpp
enum class TransactionMode : uint8_t
{
    READ_WRITE = 0,
    READ_ONLY = 1
};
```

### IsolationLevel Enum

**File:** ast.h lines 2647-2652

```cpp
enum class IsolationLevel : uint8_t
{
    READ_COMMITTED = 0,
    SNAPSHOT = 1,
    SNAPSHOT_TABLE_STABILITY = 2
};
```

### TableLockMode Enum

**File:** ast.h lines 2655-2659

```cpp
enum class TableLockMode : uint8_t
{
    SHARED = 0,    // SHARED READ - allows concurrent reads
    PROTECTED = 1, // PROTECTED READ/WRITE - exclusive table access
};
```

### TableReservation Struct

**File:** ast.h lines 2662-2672

```cpp
struct TableReservation
{
    StringPool::StringId table_name;
    TableLockMode lock_mode;
    bool for_write;

    TableReservation(StringPool::StringId name, TableLockMode mode, bool write)
        : table_name(name), lock_mode(mode), for_write(write)
    {
    }
};
```

**Fields:**
- `table_name` - Table identifier
- `lock_mode` - SHARED or PROTECTED
- `for_write` - true = WRITE access, false = READ access

---

## ASTKind Enumeration

**File:** ast.h lines 66, 90-95

The following ASTKind values are defined for SET commands:

```cpp
SET_TRANSACTION,   // Phase 3 Task 3.6 (line 66)
SET_ROLE,          // SET ROLE rolename / RESET ROLE (line 90)
SET_SESSION_AUTH,  // SET SESSION AUTHORIZATION username / RESET SESSION AUTHORIZATION (line 91)
SET_CONSTRAINTS,   // P2-7: SET CONSTRAINTS {ALL | constraint_name} {DEFERRED | IMMEDIATE} (line 92)
SET_SQL_DIALECT,   // SET SQL DIALECT N (Firebird ISQL compatibility) (line 93)
SET_NAMES,         // SET NAMES 'charset' (connection character set) (line 94)
SET_LOCAL_TIMEOUT, // SET LOCAL_TIMEOUT N (statement timeout in seconds) (line 95)
```

---

## Complete Token Reference

### SET Command Keywords

**File:** token.h

| Token Name | Line | Used In |
|------------|------|---------|
| KW_SET | (various) | All SET commands |
| KW_RESET | (various) | RESET ROLE, RESET SESSION AUTHORIZATION |
| KW_TRANSACTION | 300 | SET TRANSACTION |
| KW_READ | (various) | Transaction mode, isolation level |
| KW_WRITE | (various) | Transaction mode, access mode |
| KW_ONLY | (various) | READ ONLY |
| KW_NOT | 96 | NO WAIT (bug: should be KW_NO) |
| KW_NO | 346 | Used in other contexts (sequences) |
| KW_WAIT | 306 | NO WAIT / NOT WAIT |
| KW_ISOLATION | 307 | ISOLATION LEVEL |
| KW_LEVEL | 308 | ISOLATION LEVEL |
| KW_COMMITTED | 309 | READ COMMITTED |
| KW_SNAPSHOT | 310 | SNAPSHOT, SNAPSHOT TABLE STABILITY |
| KW_TABLE | (various) | SNAPSHOT TABLE STABILITY |
| KW_STABILITY | 311 | SNAPSHOT TABLE STABILITY |
| KW_LOCK | 318 | LOCK TIMEOUT |
| KW_TIMEOUT | 319 | LOCK TIMEOUT |
| KW_RESERVING | 312 | RESERVING clause |
| KW_SHARED | 313 | SHARED lock mode |
| KW_PROTECTED | 314 | PROTECTED lock mode |
| KW_FOR | (various) | RESERVING ... FOR ... |
| KW_SQL | (various) | SET SQL DIALECT |
| KW_DIALECT | 502 | SET SQL DIALECT |
| KW_NAMES | 511 | SET NAMES |
| KW_LOCAL_TIMEOUT | 512 | SET LOCAL_TIMEOUT |
| KW_ROLE | 430 | SET ROLE, RESET ROLE |
| KW_SESSION | 438 | SET SESSION AUTHORIZATION |
| KW_AUTHORIZATION | 439 | SET SESSION AUTHORIZATION |
| KW_CONSTRAINTS | 369 | SET CONSTRAINTS (plural) |
| KW_ALL | (various) | SET CONSTRAINTS ALL |
| KW_DEFERRED | 377 | SET CONSTRAINTS ... DEFERRED |
| KW_IMMEDIATE | 378 | SET CONSTRAINTS ... IMMEDIATE |

### Non-Keyword Tokens

| Token Name | Usage |
|------------|-------|
| INTEGER_LITERAL | Dialect number, timeout values |
| STRING_LITERAL | Character set name (alternative to identifier) |
| IDENTIFIER | Table names, constraint names, charset names, role names, usernames |
| COMMA | List separators |

---

## Implementation Issues and Notes

### Issue 1: NO WAIT vs NOT WAIT

**Location:** parser.cpp lines 4347, 4303

**Issue:** Comment says "NO WAIT" but implementation uses `KW_NOT` instead of `KW_NO`

**Impact:**
- Firebird syntax is "NO WAIT"
- ScratchBird currently parses "NOT WAIT"
- This breaks Firebird compatibility

**Same Issue In:**
- parseSetTransaction() line 4347
- parseStartTransaction() line 4142 (inferred from same pattern)

**Resolution Needed:**
- Either change to use KW_NO
- Or update comments to reflect actual syntax
- Or support both variants

### Issue 2: KW_NO vs KW_NOT Usage

**Observation:**
- KW_NO is defined (token.h line 346) for sequences (NO MINVALUE, NO MAXVALUE, NO CYCLE)
- KW_NOT is used for NO WAIT in transaction commands
- Inconsistent usage of negation keywords

**Locations:**
- KW_NO used: parseCreateSequence() (lines 5840, 5941), parseAlterTable() (line 9602)
- KW_NOT used: parseSetTransaction(), parseStartTransaction()

### Issue 3: READ COMMITTED Ambiguity

**Location:** parser.cpp lines 4334-4343

**Issue:** READ can start multiple constructs:
- READ WRITE (transaction mode)
- READ ONLY (transaction mode)
- READ COMMITTED (isolation level)
- READ (access mode in RESERVING)

**Current Handling:**
- Special case check for READ COMMITTED (line 4334)
- Sets isolation level directly without ISOLATION LEVEL keywords
- This allows both syntaxes:
  - `SET TRANSACTION READ COMMITTED`
  - `SET TRANSACTION ISOLATION LEVEL READ COMMITTED`

**Correctness:** Appears to work but is complex

### Issue 4: Lock Timeout Comment Inconsistency

**Location:** ast.h line 3330

**Comment:** "0 = no wait, UINT32_MAX = wait forever"

**Issue:**
- Comment suggests 0 means "no wait"
- But parser default is 0 with wait=true
- Semantics unclear - is 0 special or just a valid timeout?

**Impact:** Ambiguous specification, may confuse implementers

### Issue 5: WAIT as Standalone Keyword

**Observation:**
- Parser only supports "NOT WAIT" (or intended "NO WAIT")
- Does not support standalone "WAIT" keyword
- Cannot explicitly specify WAIT mode (only via default)

**Impact:**
- Cannot override a previous NO WAIT in same statement
- Less flexible than full Firebird syntax

**Firebird Behavior:** Supports both WAIT and NO WAIT as explicit options

---

## Test Coverage

### SET TRANSACTION Tests

**File:** test_transaction_advanced.cpp

- Line 199: Basic READ ONLY
- Line 211: LOCK TIMEOUT
- Line 222: RESERVING clause
- Line 239: All parameters combined
- Line 286: Bytecode generation

### SET SQL DIALECT Tests

**File:** test_show_set_commands.cpp

- Lines 453-455: Valid dialects (1, 2, 3)
- Line 460: AST structure
- Line 470: Bytecode generation
- Line 592-593: Error cases (missing dialect, invalid dialect)

### SET NAMES Tests

**File:** test_show_set_commands.cpp

- Lines 481-483: Valid charsets (UTF8, ISO8859_1, WIN1252)
- Line 488: AST structure
- Line 495: Bytecode generation
- Line 598: Error case (missing charset)

### SET LOCAL_TIMEOUT Tests

**File:** test_show_set_commands.cpp

- Lines 506-508: Valid timeouts (30, 0, 3600)
- Line 513: AST structure
- Line 523: Bytecode generation
- Lines 603-604: Error cases (missing timeout, invalid timeout)

### SET ROLE / SET SESSION AUTHORIZATION Tests

**File:** test_security_phase2.cpp

- Line 393: SET ROLE
- Line 394: RESET ROLE
- Line 395: SET SESSION AUTHORIZATION
- Line 396: RESET SESSION AUTHORIZATION

### SET CONSTRAINTS Tests

**File:** test_constraint_enforcement.cpp

- Line 548: Reference to SET CONSTRAINTS ALL DEFERRED (indirect test)

**Note:** Comprehensive parser tests for SET CONSTRAINTS appear to be missing.

---

## Comparison with Firebird

### Firebird SET TRANSACTION Syntax

```sql
SET TRANSACTION
    [NAME transaction_name]
    [READ WRITE | READ ONLY]
    [[ISOLATION LEVEL] {SNAPSHOT [TABLE STABILITY] | READ COMMITTED [[NO] RECORD_VERSION]}]
    [WAIT | NO WAIT]
    [LOCK TIMEOUT seconds]
    [NO AUTO UNDO]
    [IGNORE LIMBO]
    [RESERVING <tables> | USING <dbhandles>]
```

### ScratchBird Differences

**Not Implemented:**
1. NAME clause - Transaction naming
2. RECORD_VERSION options - Version control in READ COMMITTED
3. NO AUTO UNDO - Auto undo control
4. IGNORE LIMBO - Limbo transaction handling
5. USING clause - Multi-database transactions
6. Standalone WAIT keyword - Only NO WAIT supported (via NOT WAIT)

**Implemented:**
1. READ WRITE / READ ONLY - ✓
2. ISOLATION LEVEL - ✓ (READ COMMITTED, SNAPSHOT, SNAPSHOT TABLE STABILITY)
3. NO WAIT - ✓ (but as NOT WAIT, bug)
4. LOCK TIMEOUT - ✓
5. RESERVING - ✓ (with SHARED/PROTECTED and READ/WRITE)

**Compatibility Level:** ~60% - Core features implemented, advanced features missing

### Firebird SET SQL DIALECT

Fully compatible - supports dialects 1, 2, 3

### Firebird SET NAMES

Fully compatible - supports charset specification

### Other SET Commands

ScratchBird adds:
- SET LOCAL_TIMEOUT (not in Firebird)
- SET ROLE (standard SQL)
- SET SESSION AUTHORIZATION (standard SQL)
- SET CONSTRAINTS (standard SQL)

---

## Recommendations for Parser Rebuild

### 1. Fix NO WAIT vs NOT WAIT

**Action:** Change KW_NOT to KW_NO in parseSetTransaction() and parseStartTransaction()

**Rationale:** Firebird compatibility

### 2. Support WAIT Keyword

**Action:** Add support for explicit WAIT (not just NO WAIT as default)

**Rationale:** Full Firebird compatibility, explicitness

### 3. Clarify Lock Timeout Semantics

**Action:** Document what 0 means for lock_timeout (is it special or just 0 seconds?)

**Rationale:** Avoid implementation confusion

### 4. Add Missing Firebird Features (Optional)

Consider adding:
- NAME clause for transactions
- RECORD_VERSION options
- NO AUTO UNDO
- IGNORE LIMBO

**Rationale:** Enhanced Firebird compatibility

### 5. Improve READ Keyword Handling

**Action:** Consider refactoring READ COMMITTED detection to be more explicit

**Rationale:** Code clarity and maintainability

### 6. Add Comprehensive Tests

**Action:** Add parser tests for:
- SET CONSTRAINTS (all variants)
- RESET commands
- Error cases
- Complex combinations

**Rationale:** Ensure correctness and prevent regressions

### 7. Consider Order Dependency

**Action:** Document or enforce clause order in SET TRANSACTION

**Current:** Clauses can appear in any order
**Firebird:** Generally order-independent but with conventions

**Rationale:** Predictability and consistency

### 8. Validate Against SQL Standards

**Action:** Compare SET ROLE, SET SESSION AUTHORIZATION, SET CONSTRAINTS against SQL:2016 standard

**Rationale:** Standards compliance

---

## Summary Statistics

- **Total SET Commands:** 7
  1. SET TRANSACTION
  2. SET SQL DIALECT
  3. SET NAMES
  4. SET LOCAL_TIMEOUT
  5. SET ROLE
  6. SET SESSION AUTHORIZATION
  7. SET CONSTRAINTS

- **Total RESET Commands:** 2
  1. RESET ROLE
  2. RESET SESSION AUTHORIZATION

- **Total Parser Methods:** 7
  - parseSetTransaction()
  - parseSetSqlDialect()
  - parseSetNames()
  - parseSetLocalTimeout()
  - parseSetRole()
  - parseSetSessionAuth()
  - parseSetConstraints()

- **Total AST Nodes:** 7
  - SetTransactionStmt
  - SetSqlDialectStmt
  - SetNamesStmt
  - SetLocalTimeoutStmt
  - SetRoleStmt
  - SetSessionAuthStmt
  - SetConstraintsStmt

- **Total Keywords Used:** 26 (unique keywords across all SET commands)

- **Lines of Parser Code:**
  - parseSetTransaction(): 187 lines
  - parseSetSqlDialect(): 40 lines
  - parseSetNames(): 34 lines
  - parseSetLocalTimeout(): 33 lines
  - parseSetRole(): 29 lines
  - parseSetSessionAuth(): 35 lines
  - parseSetConstraints(): 54 lines
  - **Total:** 412 lines

---

## Appendix: Method Signatures

### Parser Methods (parser.h lines 144, 170-175)

```cpp
Statement *parseSetTransaction();        // Phase 3 Task 3.6
Statement *parseSetRole();               // SET ROLE or RESET ROLE
Statement *parseSetSessionAuth();        // SET SESSION AUTHORIZATION or RESET
Statement *parseSetConstraints();        // P2-7: SET CONSTRAINTS
Statement *parseSetSqlDialect();         // Firebird ISQL: SET SQL DIALECT N
Statement *parseSetNames();              // SET NAMES 'charset'
Statement *parseSetLocalTimeout();       // SET LOCAL_TIMEOUT N
```

### AST Constructors

```cpp
SetTransactionStmt(const SourceSpan &span,
                   TransactionMode mode = TransactionMode::READ_WRITE,
                   IsolationLevel isolation = IsolationLevel::READ_COMMITTED,
                   bool wait = true,
                   uint32_t lock_timeout = 0,
                   std::vector<TableReservation> reservations = {})

SetSqlDialectStmt(const SourceSpan &span, uint8_t dialect)

SetNamesStmt(const SourceSpan &span, StringPool::StringId charset_name)

SetLocalTimeoutStmt(const SourceSpan &span, uint32_t timeout_seconds)

SetRoleStmt(const SourceSpan& span,
           StringPool::StringId rolename,  // 0 for RESET
           bool is_reset)

SetSessionAuthStmt(const SourceSpan& span,
                  StringPool::StringId username,  // 0 for RESET
                  bool is_reset)

SetConstraintsStmt(const SourceSpan& span,
                  bool all_constraints,
                  const std::vector<StringPool::StringId>& constraint_names,
                  bool deferred)
```

---

**End of Audit Document**

**Auditor Notes:**
- All SET command implementations have been reviewed
- Source code examined: parser.cpp (4301-9360), ast.h (2641-4441), token.h
- Documentation complete and accurate as of 2025-12-06
- No summaries - all details included as requested
- Ready for comparative rebuild analysis
