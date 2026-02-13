# ScratchBird Context Variables - Design Document

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: 2025-10-24 (Updated)
**Version**: 2.0
**Status**: DRAFT
**Dependencies**: ALPHA_ROW_IDENTITY_ENHANCED.md
**Estimated Effort**: 20-28 hours (was 16-22 hours)
**Priority**: HIGH (ALPHA requirement)

---

## Executive Summary

This document defines ScratchBird's context variables following Firebird's Chapter 12 pattern. Context variables provide access to connection state, transaction metadata, date/time functions, row identity, and PSQL execution context within SQL queries and PSQL procedures.

**Key Design Principles**:
1. Follow Firebird's naming conventions and semantics
2. Context variables are **constant within PSQL modules** (procedures, triggers)
3. String literals like `'NOW'` and `'TODAY'` provide **progressing values** in PSQL
4. All context variables are **read-only**
5. No function call syntax (e.g., `CURRENT_DATE`, not `CURRENT_DATE()`)
6. Support both **direct access** (simple) and **RDB$GET_CONTEXT()** (advanced)

---

## Table of Contents

1. [Context Variables Overview](#1-context-variables-overview)
2. [RDB$GET_CONTEXT and RDB$SET_CONTEXT Functions](#2-rdbget_context-and-rdbset_context-functions)
3. [Connection Context Variables](#3-connection-context-variables)
4. [Transaction Context Variables](#4-transaction-context-variables)
5. [Row Identity Context Variables](#5-row-identity-context-variables)
6. [Date/Time Context Variables](#6-datetime-context-variables)
7. [Security Context Variables](#7-security-context-variables)
8. [Trigger Context Variables](#8-trigger-context-variables)
9. [PSQL Execution Context Variables](#9-psql-execution-context-variables-new)
10. [Error Handling Context Variables](#10-error-handling-context-variables)
11. [Implementation Plan](#11-implementation-plan)
12. [Testing Strategy](#12-testing-strategy)

---

## 1. Context Variables Overview

### 1.1 Firebird Pattern

**Key Characteristics**:
- **No parentheses**: `CURRENT_DATE`, not `CURRENT_DATE()`
- **Optional precision**: `CURRENT_TIME(2)` for milliseconds
- **PSQL stability**: Value frozen for duration of outermost PSQL module
- **String literal mnemonics**: `'NOW'`, `'TODAY'`, `'YESTERDAY'`, `'TOMORROW'`
- **Type safety**: Context variables have fixed types (e.g., `BIGINT`, `VARCHAR(63)`)
- **Namespace-based**: `RDB$GET_CONTEXT('SYSTEM', 'variable')` for extensibility

### 1.2 Dual Access Pattern

ScratchBird supports **two ways** to access context variables:

#### 1.2.1 Direct Access (Simple, Common Cases)
```sql
-- Simple context variables
SELECT CURRENT_USER, CURRENT_TRANSACTION, CURRENT_DATE FROM rdb$database;

-- Row identity context variables
SELECT sdb$key, rdb$row_uuid FROM customers;
```

#### 1.2.2 RDB$GET_CONTEXT() Function (Advanced, Extensible)
```sql
-- System namespace (read-only)
SELECT RDB$GET_CONTEXT('SYSTEM', 'CURRENT_USER') FROM rdb$database;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CALLING_PROCEDURE') FROM rdb$database;

-- User session namespace (read-write)
SELECT RDB$SET_CONTEXT('USER_SESSION', 'MyVar', 'MyValue');
SELECT RDB$GET_CONTEXT('USER_SESSION', 'MyVar');

-- User transaction namespace (read-write)
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'SavepointUsed', 'Yes');
SELECT RDB$GET_CONTEXT('USER_TRANSACTION', 'SavepointUsed');
```

### 1.3 ScratchBird Extensions

**New Context Variables** (not in Firebird):
- `sdb$key`: Physical row identifier (equivalent to Firebird's `rdb$db_key`)
- `rdb$row_uuid`: Permanent UUID v7 row identifier
- `rdb$xact_id`: Last-modifying transaction ID visible to current transaction
- **PSQL Execution Context** (7 new variables):
  - `CALLING_PROCEDURE`: Name of calling procedure/function/trigger
  - `CALL_STACK`: Full call stack trace
  - `CALL_DEPTH`: Depth of PSQL call stack
  - `CURRENT_PROCEDURE`: Name of current PSQL object
  - `CURRENT_PROCEDURE_TYPE`: Type of current PSQL object
  - `CURRENT_TRIGGER_TABLE`: Table name for current trigger
  - `EXECUTION_SOURCE`: Origin of execution (SQL_PROMPT, PSQL, etc.)

**Compatibility**: All standard Firebird context variables are supported.

---

## 2. RDB$GET_CONTEXT and RDB$SET_CONTEXT Functions

### 2.1 `RDB$GET_CONTEXT()`

**Description**: Retrieves the value of a context variable from a namespace.

**Type**: `VARCHAR(255)`

**Syntax**:
```sql
RDB$GET_CONTEXT('<namespace>', '<varname>')
```

**Namespaces**:
- `SYSTEM` - Read-only system variables
- `USER_SESSION` - User-defined session variables (connection-scoped)
- `USER_TRANSACTION` - User-defined transaction variables (transaction-scoped)
- `DDL_TRIGGER` - DDL trigger context (only in DDL triggers, read-only)

**Semantics**:
- Returns `VARCHAR(255)` value or `NULL` if variable doesn't exist (in user namespaces)
- Raises error if namespace doesn't exist or variable doesn't exist in SYSTEM/DDL_TRIGGER namespace
- Case-sensitive variable names
- Maximum variable name length: 80 characters

**Examples**:
```sql
-- System variables
SELECT RDB$GET_CONTEXT('SYSTEM', 'DB_NAME') FROM rdb$database;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS') FROM rdb$database;
SELECT RDB$GET_CONTEXT('SYSTEM', 'CALLING_PROCEDURE') FROM rdb$database;

-- User session variables
SELECT RDB$GET_CONTEXT('USER_SESSION', 'DateBegin') FROM rdb$database;

-- User transaction variables
SELECT RDB$GET_CONTEXT('USER_TRANSACTION', 'RecordsProcessed') FROM rdb$database;
```

---

### 2.2 `RDB$SET_CONTEXT()`

**Description**: Creates, sets, or clears a variable in a user-writable namespace.

**Type**: `INTEGER` (returns 1 if variable existed, 0 if new)

**Syntax**:
```sql
RDB$SET_CONTEXT('<namespace>', '<varname>', <value> | NULL)
```

**Namespaces**:
- `USER_SESSION` - Session-scoped (cleared on connection close or ALTER SESSION RESET)
- `USER_TRANSACTION` - Transaction-scoped (cleared on transaction end)

**Semantics**:
- Set to `NULL` to remove variable
- Returns `1` if variable already existed, `0` if new
- Maximum 1000 variables per context
- Variables survive ROLLBACK RETAIN and ROLLBACK TO SAVEPOINT
- Can be called as void function in PSQL (without capturing return value)

**Examples**:
```sql
-- Set session variable
SELECT RDB$SET_CONTEXT('USER_SESSION', 'DateBegin', '2025-01-01') FROM rdb$database;

-- Set transaction variable
SELECT RDB$SET_CONTEXT('USER_TRANSACTION', 'RecordsProcessed', 0) FROM rdb$database;

-- Clear variable
SELECT RDB$SET_CONTEXT('USER_SESSION', 'DateBegin', NULL) FROM rdb$database;

-- Use in PSQL (void call)
CREATE PROCEDURE track_processing
AS BEGIN
    RDB$SET_CONTEXT('USER_TRANSACTION', 'StartTime', CAST('NOW' AS TIMESTAMP));
    -- ... do work ...
    RDB$SET_CONTEXT('USER_TRANSACTION', 'EndTime', CAST('NOW' AS TIMESTAMP));
END;
```

---

### 2.3 SYSTEM Namespace Variables

The `SYSTEM` namespace contains the following read-only variables accessible via `RDB$GET_CONTEXT('SYSTEM', 'variable')`:

| Variable Name | Type | Description |
|--------------|------|-------------|
| `CLIENT_ADDRESS` | VARCHAR | IP address (TCP) or process ID (XNET) |
| `CLIENT_HOST` | VARCHAR | Remote client hostname |
| `CLIENT_PID` | INTEGER | Remote client process ID |
| `CLIENT_PROCESS` | VARCHAR | Remote client process name |
| `CURRENT_ROLE` | VARCHAR | Same as CURRENT_ROLE context variable |
| `CURRENT_USER` | VARCHAR | Same as CURRENT_USER context variable |
| `DB_NAME` | VARCHAR | Database path or alias |
| `DB_GUID` | VARCHAR | Database GUID |
| `ENGINE_VERSION` | VARCHAR | ScratchBird version string |
| `ISOLATION_LEVEL` | VARCHAR | Transaction isolation level |
| `LOCK_TIMEOUT` | INTEGER | Transaction lock timeout (seconds) |
| `NETWORK_PROTOCOL` | VARCHAR | Network protocol (TCPv4, TCPv6, XNET, NULL) |
| `READ_ONLY` | VARCHAR | 'TRUE' if transaction is read-only |
| `SESSION_ID` | BIGINT | Same as CURRENT_CONNECTION |
| `SESSION_TIMEZONE` | VARCHAR | Session time zone |
| `SNAPSHOT_NUMBER` | BIGINT | Current snapshot number |
| `TRANSACTION_ID` | BIGINT | Same as CURRENT_TRANSACTION |
| **CALLING_PROCEDURE** (NEW) | VARCHAR | Name of calling procedure/function/trigger |
| **CALL_STACK** (NEW) | VARCHAR | Full call stack trace (multiline) |
| **CALL_DEPTH** (NEW) | INTEGER | Depth of PSQL call stack |
| **CURRENT_PROCEDURE** (NEW) | VARCHAR | Name of current PSQL object |
| **CURRENT_PROCEDURE_TYPE** (NEW) | VARCHAR | Type of current PSQL object |
| **CURRENT_TRIGGER_TABLE** (NEW) | VARCHAR | Table name for current trigger |
| **EXECUTION_SOURCE** (NEW) | VARCHAR | Origin of execution |

---

### 2.4 Implementation

```cpp
// In include/scratchbird/functions/context_functions.h

class RdbGetContextFunction : public ScalarFunction {
public:
    auto execute(const std::vector<Value>& args, ConnectionContext* ctx)
        -> Result<Value, Status> override;
};

class RdbSetContextFunction : public ScalarFunction {
public:
    auto execute(const std::vector<Value>& args, ConnectionContext* ctx)
        -> Result<Value, Status> override;
};

// In ConnectionContext.h
class ConnectionContext {
    // User-defined contexts
    std::unordered_map<std::string, std::string> user_session_context_;
    std::unordered_map<std::string, std::string> user_transaction_context_;

    static constexpr size_t MAX_CONTEXT_VARIABLES = 1000;

public:
    // RDB$GET_CONTEXT implementation
    [[nodiscard]] auto getContextVariable(
        std::string_view namespace_name,
        std::string_view var_name
    ) const -> Result<std::optional<std::string>, Status>;

    // RDB$SET_CONTEXT implementation
    [[nodiscard]] auto setContextVariable(
        std::string_view namespace_name,
        std::string_view var_name,
        std::optional<std::string> value
    ) -> Result<int, Status>;  // Returns 1 if existed, 0 if new

    // Clear transaction context on transaction end
    void clearTransactionContext() {
        user_transaction_context_.clear();
    }

    // Clear both contexts on session reset
    void clearAllContexts() {
        user_session_context_.clear();
        user_transaction_context_.clear();
    }
};
```

---

## 3. Connection Context Variables

### 2.1 `CURRENT_CONNECTION`

**Description**: Unique identifier of the current connection.

**Type**: `BIGINT`

**Syntax**:
```sql
CURRENT_CONNECTION
```

**Semantics**:
- Derived from a counter on the database header page
- Incremented for each new connection
- Reset to zero on database restore
- **Thread-safe**: Uses atomic increment

**Implementation**:
```cpp
// In Database.cpp
std::atomic<uint64_t> Database::connection_counter_{0};

auto Database::allocateConnectionId() -> uint64_t {
    return connection_counter_.fetch_add(1, std::memory_order_relaxed);
}

// In ConnectionContext.cpp
ConnectionContext::ConnectionContext(Database* db)
    : connection_id_(db->allocateConnectionId())
    , ...
{}
```

**Examples**:
```sql
SELECT CURRENT_CONNECTION FROM rdb$database;
-- returns e.g. 42

INSERT INTO audit_log (connection_id, action, timestamp)
VALUES (CURRENT_CONNECTION, 'login', CURRENT_TIMESTAMP);
```

---

### 2.2 `CURRENT_USER`

**Description**: Name of the user of the current connection.

**Type**: `VARCHAR(63)`

**Syntax**:
```sql
CURRENT_USER
USER  -- alias
```

**Semantics**:
- Returns the authenticated username
- Always non-NULL (defaults to 'SYSDBA' or equivalent)
- Stored in `ConnectionContext`

**Implementation**:
```cpp
// In ConnectionContext.h
class ConnectionContext {
    std::string username_;  // Set during authentication

public:
    [[nodiscard]] auto getUsername() const -> const std::string& {
        return username_;
    }
};

// In semantic_analyzer.cpp
case ExprType::CURRENT_USER:
    return ValueType::VARCHAR;  // max_length = 63
```

**Examples**:
```sql
SELECT CURRENT_USER FROM rdb$database;
-- returns e.g. 'admin'

CREATE TRIGGER bi_customers BEFORE INSERT ON customers
AS BEGIN
    NEW.added_by = CURRENT_USER;
END;
```

---

### 2.3 `CURRENT_ROLE`

**Description**: Current explicit role of the connection.

**Type**: `VARCHAR(63)`

**Syntax**:
```sql
CURRENT_ROLE
```

**Semantics**:
- Returns explicitly specified role, or `'NONE'` if no role set
- **Post-ALPHA**: Implement full role system
- **ALPHA**: Returns `'NONE'`

**Implementation** (stub for ALPHA):
```cpp
// In ConnectionContext.h
[[nodiscard]] auto getCurrentRole() const -> std::string {
    return role_.empty() ? "NONE" : role_;
}
```

---

## 3. Transaction Context Variables

### 3.1 `CURRENT_TRANSACTION`

**Description**: Unique identifier of the current transaction.

**Type**: `BIGINT`

**Syntax**:
```sql
CURRENT_TRANSACTION
```

**Semantics**:
- Returns the 64-bit transaction ID
- Derived from a counter on the database header page
- Reset to zero on database restore
- Equivalent to `txn_current()` function

**Implementation**:
```cpp
// In ConnectionContext.h
[[nodiscard]] auto getCurrentTransactionId() const -> uint64_t {
    if (current_transaction_) {
        return current_transaction_->getTransactionId();
    }
    return 0;  // No active transaction
}

// In executor.cpp (OP_PUSH_CONTEXT_VAR)
case ContextVar::CURRENT_TRANSACTION:
    stack_.push(Value::fromBigInt(ctx->getCurrentTransactionId()));
    break;
```

**Examples**:
```sql
SELECT CURRENT_TRANSACTION FROM rdb$database;
-- returns e.g. 12345

-- Log transaction ID in trigger
CREATE TRIGGER ai_orders AFTER INSERT ON orders
AS BEGIN
    INSERT INTO order_audit (order_id, txn_id, timestamp)
    VALUES (NEW.order_id, CURRENT_TRANSACTION, CURRENT_TIMESTAMP);
END;
```

---

### 3.2 `rdb$xact_id` (NEW - ScratchBird Extension)

**Description**: Transaction ID of the last-modifying transaction visible to the current transaction.

**Type**: `BIGINT`

**Syntax**:
```sql
SELECT rdb$xact_id FROM table_name;
```

**Semantics**:
- Returns the `xmin` value of the visible version of the row
- Always non-NULL (every row has a creator transaction)
- **MGA-aware**: Traverses back-version chain to find visible version
- **Read-only**: Cannot be set by user

**Implementation**:
```cpp
// In heap_page.cpp
auto HeapPage::getVisibleXactId(
    const TupleHeader* tuple,
    TransactionId current_xid,
    SnapshotPtr snapshot
) const -> uint64_t {
    // Traverse version chain to find visible version
    while (tuple) {
        if (isVisible(tuple, current_xid, snapshot)) {
            return tuple->xmin;  // Return creator transaction ID
        }
        // Follow back-version chain
        tuple = getBackVersion(tuple);
    }
    return 0;  // No visible version (should not happen)
}

// In semantic_analyzer.cpp
case SpecialColumn::RDB_XACT_ID:
    // System column, returns BIGINT
    return ValueType::BIGINT;
```

**Examples**:
```sql
-- Find all rows modified by transaction 12345
SELECT * FROM customers WHERE rdb$xact_id = 12345;

-- Check if row has been modified since snapshot
SELECT customer_id, rdb$xact_id
FROM customers
WHERE rdb$xact_id > :snapshot_txn_id;
```

---

## 4. Row Identity Context Variables

### 4.1 `sdb$key` (NEW - ScratchBird Extension)

**Description**: Physical row identifier (equivalent to Firebird's `rdb$db_key`).

**Type**: `VARCHAR(32)` (formatted as hex string)

**Syntax**:
```sql
SELECT sdb$key FROM table_name;
```

**Format**: `TTTT-PPPPPPPPPPPP-SSSS` (hex string, 22 characters + 2 hyphens)
- `TTTT`: 16-bit tablespace ID
- `PPPPPPPPPPPP`: 48-bit page number (12 hex digits)
- `SSSS`: 16-bit slot ID

**Semantics**:
- **Derived**: Computed from TID (GPID + slot_id), 0 bytes overhead
- **Hidden from `SELECT *`**: Only visible when explicitly addressed
- **Fast lookup**: Can be used in hash indexes for O(1) access
- **Unstable**: Changes if row moves (backup/restore, tablespace migration)

**Implementation**:
```cpp
// In types.h
struct SdbKey {
    uint16_t tablespace_id;
    uint64_t page_num;      // 48-bit
    uint16_t slot_id;

    [[nodiscard]] auto toString() const -> std::string {
        // Format: TTTT-PPPPPPPPPPPP-SSSS
        return fmt::format("{:04X}-{:012X}-{:04X}",
                          tablespace_id, page_num, slot_id);
    }

    [[nodiscard]] static auto fromString(std::string_view str) -> Result<SdbKey, Status>;
};

// In heap_page.cpp
auto HeapPage::getSdbKey(uint16_t slot_id) const -> SdbKey {
    return SdbKey{
        .tablespace_id = getTablespaceId(),
        .page_num = getPageNumber(),
        .slot_id = slot_id
    };
}
```

**Examples**:
```sql
-- Select with sdb$key (explicitly addressed)
SELECT customer_id, sdb$key FROM customers;
-- returns e.g. (1001, '0001-00000000ABCD-0042')

-- Fast positional UPDATE using sdb$key
UPDATE customers SET balance = balance + 100.00
WHERE sdb$key = '0001-00000000ABCD-0042';

-- Hash index on sdb$key for O(1) lookups
CREATE INDEX idx_customers_sdbkey ON customers USING HASH (sdb$key);
```

---

### 4.2 `rdb$row_uuid` (NEW - ScratchBird Extension)

**Description**: Permanent UUID v7 row identifier (survives backup/restore).

**Type**: `UUID` (displayed as `VARCHAR(36)`, stored as 16 bytes)

**Syntax**:
```sql
SELECT rdb$row_uuid FROM table_name;
```

**Format**: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx` (36-character UUID string)

**Semantics**:
- **Persistent**: Survives backup/restore, tablespace migration
- **Time-ordered**: UUID v7 (RFC 9562), sortable by creation time
- **16-byte overhead**: Added to `TupleHeader` (44 → 60 bytes)
- **Unique**: Globally unique across all tables
- **Immutable**: Set once on INSERT, never changes

**Implementation**:
```cpp
// In heap_page.h
struct TupleHeader {
    uint64_t xmin;
    uint64_t xmax;
    uint64_t back_version_gpid;
    uint16_t back_version_slot;
    uint16_t reserved1;
    GPID ctid_gpid;
    uint16_t ctid_slot;
    uint16_t infomask;
    uint16_t null_bitmap_offset;
    uint16_t padding;
    UuidV7Bytes row_uuid;  // NEW: +16 bytes
    // Total: 60 bytes
};

// In uuidv7.h
struct UuidV7Bytes {
    std::array<uint8_t, 16> bytes{};

    [[nodiscard]] auto toString() const -> std::string;
    [[nodiscard]] static auto fromString(std::string_view str) -> Result<UuidV7Bytes, Status>;
};
```

**Examples**:
```sql
-- Select with rdb$row_uuid
SELECT customer_id, rdb$row_uuid FROM customers;
-- returns e.g. (1001, '018c3c5e-7f8a-7000-9e5f-3b1d4e8a6f2c')

-- Find row by permanent UUID (survives migration)
SELECT * FROM customers
WHERE rdb$row_uuid = '018c3c5e-7f8a-7000-9e5f-3b1d4e8a6f2c';

-- Use as IDENTITY column (surfaced UUID)
CREATE TABLE orders (
    order_id UUID IDENTITY,  -- Surfaces rdb$row_uuid
    customer_id BIGINT,
    ...
);

-- Binary UUID input (16 bytes)
INSERT INTO customers (customer_id, rdb$row_uuid)
VALUES (1001, X'018C3C5E7F8A70009E5F3B1D4E8A6F2C');

-- String UUID input (36 characters)
INSERT INTO customers (customer_id, rdb$row_uuid)
VALUES (1001, '018c3c5e-7f8a-7000-9e5f-3b1d4e8a6f2c');
```

---

## 5. Date/Time Context Variables

### 5.1 `CURRENT_DATE`

**Description**: Current server date in the session time zone.

**Type**: `DATE`

**Syntax**:
```sql
CURRENT_DATE
```

**Semantics**:
- **PSQL stability**: Constant within outermost PSQL module
- **Progressing value**: Use `'TODAY'` for actual current date in PSQL
- Session time zone aware

**Implementation**:
```cpp
// In ConnectionContext.h
class ConnectionContext {
    mutable std::optional<Date> current_date_cache_;

public:
    [[nodiscard]] auto getCurrentDate() const -> Date {
        if (!current_date_cache_) {
            current_date_cache_ = Date::now(session_timezone_);
        }
        return *current_date_cache_;
    }

    // Reset cache on new outermost PSQL call
    void resetPsqlCache() {
        current_date_cache_.reset();
        current_time_cache_.reset();
        current_timestamp_cache_.reset();
    }
};
```

**Examples**:
```sql
SELECT CURRENT_DATE FROM rdb$database;
-- returns e.g. 2025-10-24

-- Use in WHERE clause
SELECT * FROM orders WHERE order_date = CURRENT_DATE;
```

---

### 5.2 `CURRENT_TIME`

**Description**: Current server time in the session time zone, with time zone information.

**Type**: `TIME WITH TIME ZONE`

**Syntax**:
```sql
CURRENT_TIME[(precision)]
```

**Precision**: 0-3 (default: 0 = seconds precision)

**Semantics**:
- **PSQL stability**: Constant within outermost PSQL module
- **Progressing value**: Use `'NOW'` for actual current time in PSQL
- Returns `TIME WITH TIME ZONE`
- Use `LOCALTIME` for `TIME WITHOUT TIME ZONE`

**Examples**:
```sql
SELECT CURRENT_TIME FROM rdb$database;
-- returns e.g. 14:20:19.0000+00:00

SELECT CURRENT_TIME(3) FROM rdb$database;
-- returns e.g. 14:20:23.120+00:00
```

---

### 5.3 `CURRENT_TIMESTAMP`

**Description**: Current server date and time in the session time zone, with time zone information.

**Type**: `TIMESTAMP WITH TIME ZONE`

**Syntax**:
```sql
CURRENT_TIMESTAMP[(precision)]
```

**Precision**: 0-3 (default: 3 = milliseconds precision)

**Semantics**:
- **PSQL stability**: Constant within outermost PSQL module
- **Progressing value**: Use `'NOW'` for actual current timestamp in PSQL

**Examples**:
```sql
SELECT CURRENT_TIMESTAMP FROM rdb$database;
-- returns e.g. 2025-10-24 14:20:19.617+00:00

-- Log with timestamp
INSERT INTO audit_log (action, timestamp)
VALUES ('user_login', CURRENT_TIMESTAMP);
```

---

### 5.4 `LOCALTIME` and `LOCALTIMESTAMP`

**Description**: Current time/timestamp WITHOUT time zone information.

**Type**: `TIME WITHOUT TIME ZONE` / `TIMESTAMP WITHOUT TIME ZONE`

**Syntax**:
```sql
LOCALTIME[(precision)]
LOCALTIMESTAMP[(precision)]
```

**Semantics**:
- Same as `CURRENT_TIME`/`CURRENT_TIMESTAMP` but without time zone
- Use when time zone information is not needed

**Examples**:
```sql
SELECT LOCALTIME FROM rdb$database;
-- returns e.g. 14:20:19.0000

SELECT LOCALTIMESTAMP FROM rdb$database;
-- returns e.g. 2025-10-24 14:20:19.617
```

---

### 5.5 String Literals: `'NOW'`, `'TODAY'`, `'YESTERDAY'`, `'TOMORROW'`

**Description**: Date/time mnemonics that provide **progressing values** in PSQL.

**Semantics**:
- **Not variables**: String literals with special CAST behavior
- **Always current**: Return actual current date/time (not cached in PSQL)
- **Case-insensitive**: `'NOW'`, `'now'`, `'Now'` all work
- **Leading/trailing spaces ignored**: `' NOW '` works

**Usage**:
```sql
-- 'NOW' - current timestamp
SELECT CAST('NOW' AS TIMESTAMP) FROM rdb$database;
-- returns e.g. 2025-10-24 14:20:23.120

-- 'TODAY' - current date
SELECT CAST('TODAY' AS DATE) FROM rdb$database;
-- returns e.g. 2025-10-24

-- 'YESTERDAY' - previous day
SELECT CAST('YESTERDAY' AS DATE) FROM rdb$database;
-- returns e.g. 2025-10-23

-- 'TOMORROW' - next day
SELECT CAST('TOMORROW' AS DATE) FROM rdb$database;
-- returns e.g. 2025-10-25

-- Use in PSQL for progressing values (time measurement)
CREATE PROCEDURE measure_time
AS
DECLARE start_time TIMESTAMP;
DECLARE end_time TIMESTAMP;
BEGIN
    start_time = CAST('NOW' AS TIMESTAMP);
    -- ... do work ...
    end_time = CAST('NOW' AS TIMESTAMP);
    -- start_time and end_time will be different
END;
```

---

## 6. Security Context Variables

### 6.1 `CURRENT_USER` / `USER`

See [Section 2.2](#22-current_user).

### 6.2 `CURRENT_ROLE`

See [Section 2.3](#23-current_role).

---

## 7. Trigger Context Variables

### 7.1 `INSERTING`, `UPDATING`, `DELETING`

**Description**: Boolean flags indicating which DML operation fired the trigger.

**Type**: `BOOLEAN`

**Available in**: PSQL triggers only

**Syntax**:
```sql
INSERTING
UPDATING
DELETING
```

**Semantics**:
- **Multi-action triggers**: Determine which operation fired the trigger
- One of the three is always `TRUE` in a DML trigger

**Examples**:
```sql
CREATE TRIGGER trg_audit FOR customers
AFTER INSERT OR UPDATE OR DELETE
AS BEGIN
    IF (INSERTING) THEN
        INSERT INTO audit_log VALUES ('INSERT', NEW.customer_id, CURRENT_TIMESTAMP);
    ELSE IF (UPDATING) THEN
        INSERT INTO audit_log VALUES ('UPDATE', NEW.customer_id, CURRENT_TIMESTAMP);
    ELSE IF (DELETING) THEN
        INSERT INTO audit_log VALUES ('DELETE', OLD.customer_id, CURRENT_TIMESTAMP);
    END IF;
END;
```

---

### 7.2 `NEW` and `OLD`

**Description**: Record values before/after DML operation.

**Type**: Record type

**Available in**: PSQL triggers only

**Syntax**:
```sql
NEW.column_name
OLD.column_name
```

**Semantics**:
- **NEW**: Contains new/updated values (INSERT, UPDATE)
- **OLD**: Contains existing values (UPDATE, DELETE)
- **NEW is read-only**: In AFTER triggers
- **OLD is always read-only**

**Examples**:
```sql
CREATE TRIGGER bi_customers BEFORE INSERT ON customers
AS BEGIN
    NEW.added_by = CURRENT_USER;
    NEW.created_at = CURRENT_TIMESTAMP;
END;

CREATE TRIGGER au_customers AFTER UPDATE ON customers
AS BEGIN
    IF (OLD.balance <> NEW.balance) THEN
        INSERT INTO balance_history (customer_id, old_balance, new_balance, changed_at)
        VALUES (NEW.customer_id, OLD.balance, NEW.balance, CURRENT_TIMESTAMP);
    END IF;
END;
```

---

## 8. Error Handling Context Variables

### 8.1 `GDSCODE`

**Description**: Firebird error code of the error in a `WHEN ... DO` block.

**Type**: `INTEGER`

**Available in**: PSQL error handlers

**Syntax**:
```sql
GDSCODE
```

**Examples**:
```sql
BEGIN
    -- ... code ...
EXCEPTION
    WHEN GDSCODE grant_obj_notfound DO
        INSERT INTO error_log VALUES (GDSCODE, CURRENT_TIMESTAMP);
END;
```

---

### 8.2 `SQLSTATE`

**Description**: SQL-compliant 5-character status code.

**Type**: `CHAR(5)`

**Available in**: PSQL error handlers

**Syntax**:
```sql
SQLSTATE
```

**Examples**:
```sql
BEGIN
    -- ... code ...
EXCEPTION
    WHEN ANY DO
        CASE SQLSTATE
            WHEN '22003' THEN -- Numeric overflow
                msg = 'Numeric value out of range';
            WHEN '23000' THEN -- Integrity constraint violation
                msg = 'Constraint violation';
            ELSE
                msg = 'Error: ' || SQLSTATE;
        END CASE;
END;
```

---

### 8.3 `ROW_COUNT`

**Description**: Number of affected rows of the last executed statement.

**Type**: `INTEGER`

**Available in**: PSQL

**Syntax**:
```sql
ROW_COUNT
```

**Semantics**:
- Contains count from most recent DML statement
- `SELECT`: 1 if row retrieved, 0 otherwise
- `FOR SELECT`: Incremented with each iteration
- `FETCH`: 1 if row fetched, 0 otherwise

**Examples**:
```sql
UPDATE customers SET balance = balance + 100 WHERE customer_id = :id;
IF (ROW_COUNT = 0) THEN
    INSERT INTO customers (customer_id, balance) VALUES (:id, 100);
END IF;
```

---

## 9. PSQL Execution Context Variables (NEW)

### Overview

PSQL Execution Context variables provide call stack tracking and execution source information for stored procedures, functions, and triggers. These variables are **ScratchBird extensions** not present in Firebird, designed to enable sophisticated debugging, auditing, and call frequency tracking.

**Access Method**: All PSQL execution context variables are accessed via `RDB$GET_CONTEXT('SYSTEM', 'variable_name')`.

---

### 9.1 `CALLING_PROCEDURE`

**Description**: Name of the procedure/function/trigger that called the current PSQL object.

**Type**: `VARCHAR(255)`

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'CALLING_PROCEDURE')
```

**Return Values**:
- `'SQL_PROMPT'` - Called directly from interactive SQL
- `'PROCEDURE:procedure_name'` - Called from stored procedure
- `'FUNCTION:function_name'` - Called from function
- `'TRIGGER:trigger_name ON table_name'` - Called from trigger
- `'EXECUTE_BLOCK'` - Called from anonymous block

**Examples**:
```sql
CREATE PROCEDURE log_transaction(amount DECIMAL(15,2))
AS
DECLARE caller VARCHAR(255);
BEGIN
    caller = RDB$GET_CONTEXT('SYSTEM', 'CALLING_PROCEDURE');

    INSERT INTO transaction_audit (
        caller_name,
        amount,
        logged_at
    ) VALUES (
        :caller,
        :amount,
        CURRENT_TIMESTAMP
    );

    -- Track call frequency
    IF (caller = 'SQL_PROMPT') THEN
        -- Direct user call
        INSERT INTO user_calls VALUES ('log_transaction', CURRENT_TIMESTAMP);
    ELSE
        -- Called from another PSQL object
        INSERT INTO psql_calls VALUES (:caller, 'log_transaction', CURRENT_TIMESTAMP);
    END IF;
END;
```

**Use Cases**:
- **Audit logging**: Track which procedures call each other
- **Call frequency analysis**: Identify hot code paths
- **Security**: Restrict procedure execution based on caller
- **Debugging**: Trace call origins

---

### 9.2 `CALL_STACK`

**Description**: Full call stack trace from SQL prompt to current PSQL object.

**Type**: `VARCHAR(8192)` (multiline string)

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'CALL_STACK')
```

**Format**: One line per stack frame (newest last):
```
SQL_PROMPT
  -> PROCEDURE:outer_proc (line 42)
  -> FUNCTION:inner_func (line 15)
  -> TRIGGER:audit_trigger ON customers (line 8)
```

**Examples**:
```sql
CREATE PROCEDURE handle_error
AS
DECLARE stack_trace VARCHAR(8192);
BEGIN
    stack_trace = RDB$GET_CONTEXT('SYSTEM', 'CALL_STACK');

    INSERT INTO error_log (
        error_code,
        error_message,
        call_stack,
        occurred_at
    ) VALUES (
        GDSCODE,
        SQLSTATE,
        :stack_trace,
        CURRENT_TIMESTAMP
    );
END;
```

**Use Cases**:
- **Error logging**: Capture full stack trace on exceptions
- **Performance profiling**: Identify deep call chains
- **Debugging**: Visual call path for troubleshooting

---

### 9.3 `CALL_DEPTH`

**Description**: Depth of the PSQL call stack (0 = SQL prompt, 1+ = nested depth).

**Type**: `INTEGER`

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'CALL_DEPTH')
```

**Values**:
- `0` - Executing at SQL prompt level
- `1` - One level deep (top-level procedure/function/trigger)
- `2+` - Nested calls

**Examples**:
```sql
CREATE PROCEDURE recursive_process(level INTEGER)
AS
DECLARE max_depth INTEGER = 10;
DECLARE current_depth INTEGER;
BEGIN
    current_depth = CAST(RDB$GET_CONTEXT('SYSTEM', 'CALL_DEPTH') AS INTEGER);

    IF (current_depth > max_depth) THEN
        EXCEPTION recursion_too_deep 'Maximum recursion depth exceeded: ' || current_depth;
    END IF;

    -- ... recursive logic ...
    IF (level > 0) THEN
        EXECUTE PROCEDURE recursive_process(:level - 1);
    END IF;
END;
```

**Use Cases**:
- **Recursion limiting**: Prevent stack overflow
- **Performance monitoring**: Detect overly deep call stacks
- **Conditional logic**: Behave differently at different depths

---

### 9.4 `CURRENT_PROCEDURE`

**Description**: Name of the currently executing procedure/function/trigger.

**Type**: `VARCHAR(63)`

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'CURRENT_PROCEDURE')
```

**Format**: `'TYPE:name'`
- `'PROCEDURE:name'`
- `'FUNCTION:name'`
- `'TRIGGER:name ON table'`
- `'EXECUTE_BLOCK'`
- `'SQL_PROMPT'`

**Examples**:
```sql
CREATE PROCEDURE shared_utility_function(param INT)
AS
DECLARE proc_name VARCHAR(63);
BEGIN
    proc_name = RDB$GET_CONTEXT('SYSTEM', 'CURRENT_PROCEDURE');

    -- Log which procedure is using this utility
    INSERT INTO utility_usage VALUES (:proc_name, CURRENT_TIMESTAMP);

    -- ... utility logic ...
END;
```

**Use Cases**:
- **Self-identification**: Shared code that needs to know its name
- **Logging**: Auto-include procedure name in logs
- **Metrics**: Track execution time per procedure

---

### 9.5 `CURRENT_PROCEDURE_TYPE`

**Description**: Type of the currently executing PSQL object.

**Type**: `VARCHAR(20)`

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'CURRENT_PROCEDURE_TYPE')
```

**Values**:
- `'PROCEDURE'`
- `'FUNCTION'`
- `'TRIGGER'`
- `'EXECUTE_BLOCK'`
- `'SQL_PROMPT'`

**Examples**:
```sql
CREATE PROCEDURE type_aware_logging
AS
DECLARE exec_type VARCHAR(20);
BEGIN
    exec_type = RDB$GET_CONTEXT('SYSTEM', 'CURRENT_PROCEDURE_TYPE');

    IF (exec_type = 'TRIGGER') THEN
        -- Trigger-specific logging
        INSERT INTO trigger_log VALUES (
            RDB$GET_CONTEXT('SYSTEM', 'CURRENT_TRIGGER_TABLE'),
            CURRENT_TIMESTAMP
        );
    ELSE IF (exec_type = 'PROCEDURE') THEN
        -- Procedure-specific logging
        INSERT INTO procedure_log VALUES (
            RDB$GET_CONTEXT('SYSTEM', 'CURRENT_PROCEDURE'),
            CURRENT_TIMESTAMP
        );
    END IF;
END;
```

**Use Cases**:
- **Conditional behavior**: Different logic for procedures vs triggers
- **Type-specific logging**: Separate logs for different PSQL types

---

### 9.6 `CURRENT_TRIGGER_TABLE`

**Description**: Table name for the currently executing trigger (NULL if not in trigger).

**Type**: `VARCHAR(63)` or NULL

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'CURRENT_TRIGGER_TABLE')
```

**Values**:
- Table name (e.g., `'customers'`) if in trigger
- `NULL` if not in trigger

**Examples**:
```sql
CREATE TRIGGER generic_audit_trigger FOR customers
AFTER INSERT OR UPDATE OR DELETE
AS
DECLARE table_name VARCHAR(63);
DECLARE operation VARCHAR(10);
BEGIN
    table_name = RDB$GET_CONTEXT('SYSTEM', 'CURRENT_TRIGGER_TABLE');

    IF (INSERTING) THEN operation = 'INSERT';
    ELSE IF (UPDATING) THEN operation = 'UPDATE';
    ELSE IF (DELETING) THEN operation = 'DELETE';
    END IF;

    INSERT INTO audit_log (table_name, operation, timestamp)
    VALUES (:table_name, :operation, CURRENT_TIMESTAMP);
END;
```

**Use Cases**:
- **Generic triggers**: Shared trigger code for multiple tables
- **Audit logging**: Know which table was modified
- **Table-specific logic**: Conditional behavior based on table

---

### 9.7 `EXECUTION_SOURCE`

**Description**: Origin of the current execution.

**Type**: `VARCHAR(20)`

**Access**:
```sql
RDB$GET_CONTEXT('SYSTEM', 'EXECUTION_SOURCE')
```

**Values**:
- `'SQL_PROMPT'` - Interactive SQL command
- `'PSQL'` - Called from stored procedure/function
- `'INTERNAL'` - System-initiated (e.g., constraint checks)
- `'AUTONOMOUS'` - Autonomous transaction block

**Examples**:
```sql
CREATE PROCEDURE security_check
AS
DECLARE source VARCHAR(20);
BEGIN
    source = RDB$GET_CONTEXT('SYSTEM', 'EXECUTION_SOURCE');

    IF (source <> 'SQL_PROMPT') THEN
        EXCEPTION unauthorized_call 'This procedure can only be called directly from SQL';
    END IF;

    -- ... privileged operation ...
END;
```

**Use Cases**:
- **Security**: Restrict execution to specific sources
- **Behavior modification**: Different logic for user vs system calls
- **Debugging**: Understand execution context

---

### 9.8 Implementation

**Data Structure** (ConnectionContext):
```cpp
class ConnectionContext {
    // PSQL call stack
    struct CallFrame {
        enum class Type { SQL_PROMPT, PROCEDURE, FUNCTION, TRIGGER, EXECUTE_BLOCK };

        Type type;
        std::string name;           // Procedure/function/trigger name
        std::string table_name;     // For triggers only
        uint32_t line_number;       // Current line in PSQL
        uint32_t call_site_line;    // Line where this was called from
    };

    std::vector<CallFrame> call_stack_;

public:
    // RAII helper for PSQL object entry/exit
    class PsqlScopeGuard {
        ConnectionContext* ctx_;
    public:
        PsqlScopeGuard(ConnectionContext* ctx, CallFrame::Type type,
                      std::string_view name, std::string_view table_name = "")
            : ctx_(ctx) {
            ctx_->enterPsqlObject(type, name, table_name);
        }

        ~PsqlScopeGuard() {
            ctx_->exitPsqlObject();
        }
    };

    void enterPsqlObject(CallFrame::Type type, std::string_view name,
                         std::string_view table_name = "") {
        call_stack_.push_back({
            type,
            std::string(name),
            std::string(table_name),
            0,
            0
        });

        // Reset date/time cache on outermost entry
        if (call_stack_.size() == 1) {
            resetPsqlCache();
        }
    }

    void exitPsqlObject() {
        if (!call_stack_.empty()) {
            call_stack_.pop_back();
        }
    }

    [[nodiscard]] auto getCallDepth() const -> uint32_t {
        return call_stack_.size();
    }

    [[nodiscard]] auto getCallingProcedure() const -> std::string {
        if (call_stack_.size() < 2) {
            return "SQL_PROMPT";
        }
        const auto& caller = call_stack_[call_stack_.size() - 2];
        return formatCallFrame(caller);
    }

    [[nodiscard]] auto getCurrentProcedure() const -> std::string {
        if (call_stack_.empty()) {
            return "SQL_PROMPT";
        }
        return formatCallFrame(call_stack_.back());
    }

    [[nodiscard]] auto getCurrentProcedureType() const -> std::string {
        if (call_stack_.empty()) {
            return "SQL_PROMPT";
        }

        switch (call_stack_.back().type) {
            case CallFrame::Type::PROCEDURE: return "PROCEDURE";
            case CallFrame::Type::FUNCTION: return "FUNCTION";
            case CallFrame::Type::TRIGGER: return "TRIGGER";
            case CallFrame::Type::EXECUTE_BLOCK: return "EXECUTE_BLOCK";
            default: return "SQL_PROMPT";
        }
    }

    [[nodiscard]] auto getCurrentTriggerTable() const -> std::optional<std::string> {
        if (call_stack_.empty() || call_stack_.back().type != CallFrame::Type::TRIGGER) {
            return std::nullopt;
        }
        return call_stack_.back().table_name;
    }

    [[nodiscard]] auto getCallStack() const -> std::string {
        std::string result = "SQL_PROMPT\n";
        for (const auto& frame : call_stack_) {
            result += "  -> " + formatCallFrame(frame);
            if (frame.line_number > 0) {
                result += " (line " + std::to_string(frame.line_number) + ")";
            }
            result += "\n";
        }
        return result;
    }

    [[nodiscard]] auto getExecutionSource() const -> std::string {
        if (call_stack_.empty()) {
            return "SQL_PROMPT";
        }
        // Could be extended with more sources
        return "PSQL";
    }

private:
    static auto formatCallFrame(const CallFrame& frame) -> std::string {
        switch (frame.type) {
            case CallFrame::Type::PROCEDURE:
                return "PROCEDURE:" + frame.name;
            case CallFrame::Type::FUNCTION:
                return "FUNCTION:" + frame.name;
            case CallFrame::Type::TRIGGER:
                return "TRIGGER:" + frame.name + " ON " + frame.table_name;
            case CallFrame::Type::EXECUTE_BLOCK:
                return "EXECUTE_BLOCK";
            default:
                return "SQL_PROMPT";
        }
    }
};
```

**Usage in PSQL Executor**:
```cpp
// In stored procedure executor
auto executeProcedure(const std::string& proc_name, const std::vector<Value>& args)
    -> Result<Value, Status> {

    // RAII: Automatically push/pop call stack
    ConnectionContext::PsqlScopeGuard scope(ctx_,
                                            ConnectionContext::CallFrame::Type::PROCEDURE,
                                            proc_name);

    // ... execute procedure body ...
}

// In trigger executor
auto executeTrigger(const std::string& trigger_name, const std::string& table_name)
    -> Result<void, Status> {

    // RAII: Automatically push/pop call stack
    ConnectionContext::PsqlScopeGuard scope(ctx_,
                                            ConnectionContext::CallFrame::Type::TRIGGER,
                                            trigger_name,
                                            table_name);

    // ... execute trigger body ...
}
```

---

## 10. Implementation Plan

### Phase 1: Core Infrastructure (4-6 hours)

**Task 9.1.1: Context Variable Enum** (1 hour)
```cpp
// In include/scratchbird/core/context_variables.h

enum class ContextVar : uint8_t {
    // Connection context
    CURRENT_CONNECTION = 0,
    CURRENT_USER = 1,
    CURRENT_ROLE = 2,

    // Transaction context
    CURRENT_TRANSACTION = 3,

    // Row identity (NEW)
    SDB_KEY = 4,
    RDB_ROW_UUID = 5,
    RDB_XACT_ID = 6,

    // Date/Time
    CURRENT_DATE = 7,
    CURRENT_TIME = 8,
    CURRENT_TIMESTAMP = 9,
    LOCALTIME = 10,
    LOCALTIMESTAMP = 11,

    // Trigger context
    INSERTING = 12,
    UPDATING = 13,
    DELETING = 14,

    // Error handling
    GDSCODE = 15,
    SQLSTATE = 16,
    ROW_COUNT = 17,
};

[[nodiscard]] auto contextVarToString(ContextVar var) -> std::string_view;
[[nodiscard]] auto contextVarType(ContextVar var) -> ValueType;
```

**Task 9.1.2: Lexer/Parser Integration** (2-3 hours)
```cpp
// In lexer.cpp - Add keywords
{"CURRENT_CONNECTION", TokenType::CURRENT_CONNECTION},
{"CURRENT_USER", TokenType::CURRENT_USER},
{"CURRENT_TRANSACTION", TokenType::CURRENT_TRANSACTION},
{"CURRENT_DATE", TokenType::CURRENT_DATE},
{"CURRENT_TIME", TokenType::CURRENT_TIME},
{"CURRENT_TIMESTAMP", TokenType::CURRENT_TIMESTAMP},
{"LOCALTIME", TokenType::LOCALTIME},
{"LOCALTIMESTAMP", TokenType::LOCALTIMESTAMP},
// ...

// In parser.cpp - Parse context variables
auto Parser::parseContextVariable() -> std::unique_ptr<ASTExpr> {
    ContextVar var;
    switch (currentToken().type) {
        case TokenType::CURRENT_CONNECTION:
            var = ContextVar::CURRENT_CONNECTION;
            break;
        // ...
    }
    advance();

    // Check for precision: CURRENT_TIME(3)
    std::optional<uint8_t> precision;
    if (match(TokenType::LPAREN)) {
        precision = parseIntLiteral();
        expect(TokenType::RPAREN);
    }

    return std::make_unique<ASTContextVarExpr>(var, precision);
}
```

**Task 9.1.3: Semantic Analysis** (1-2 hours)
```cpp
// In semantic_analyzer.cpp
auto SemanticAnalyzer::analyzeContextVar(const ASTContextVarExpr* expr) -> ValueType {
    ContextVar var = expr->getContextVar();

    // Type checking
    switch (var) {
        case ContextVar::CURRENT_CONNECTION:
        case ContextVar::CURRENT_TRANSACTION:
        case ContextVar::RDB_XACT_ID:
            return ValueType::BIGINT;

        case ContextVar::CURRENT_USER:
        case ContextVar::CURRENT_ROLE:
        case ContextVar::SQLSTATE:
            return ValueType::VARCHAR;

        case ContextVar::CURRENT_DATE:
            return ValueType::DATE;

        case ContextVar::CURRENT_TIME:
            return ValueType::TIME_WITH_TIMEZONE;

        case ContextVar::INSERTING:
        case ContextVar::UPDATING:
        case ContextVar::DELETING:
            return ValueType::BOOLEAN;

        // ...
    }

    // Availability checking
    if (var == ContextVar::INSERTING && !in_trigger_) {
        reportError("INSERTING context variable only available in triggers");
    }

    return contextVarType(var);
}
```

---

### Phase 2: Bytecode and Execution (3-5 hours)

**Task 9.2.1: Bytecode Generation** (1-2 hours)
```cpp
// In bytecode_generator.cpp
void BytecodeGenerator::generateContextVar(const ASTContextVarExpr* expr) {
    ContextVar var = expr->getContextVar();
    std::optional<uint8_t> precision = expr->getPrecision();

    emit(OpCode::OP_PUSH_CONTEXT_VAR);
    emit(static_cast<uint8_t>(var));
    emit(precision.value_or(0));  // 0 = no precision/default
}
```

**Task 9.2.2: Executor Implementation** (2-3 hours)
```cpp
// In executor.cpp
case OpCode::OP_PUSH_CONTEXT_VAR: {
    auto var = static_cast<ContextVar>(readByte());
    uint8_t precision = readByte();

    switch (var) {
        case ContextVar::CURRENT_CONNECTION:
            stack_.push(Value::fromBigInt(ctx_->getConnectionId()));
            break;

        case ContextVar::CURRENT_USER:
            stack_.push(Value::fromString(ctx_->getUsername()));
            break;

        case ContextVar::CURRENT_TRANSACTION:
            stack_.push(Value::fromBigInt(ctx_->getCurrentTransactionId()));
            break;

        case ContextVar::CURRENT_DATE:
            stack_.push(Value::fromDate(ctx_->getCurrentDate()));
            break;

        case ContextVar::CURRENT_TIMESTAMP:
            stack_.push(Value::fromTimestamp(
                ctx_->getCurrentTimestamp(precision)
            ));
            break;

        case ContextVar::SDB_KEY:
            // Requires current tuple context
            stack_.push(Value::fromString(ctx_->getCurrentSdbKey().toString()));
            break;

        case ContextVar::RDB_ROW_UUID:
            // Requires current tuple context
            stack_.push(Value::fromUuid(ctx_->getCurrentRowUuid()));
            break;

        case ContextVar::RDB_XACT_ID:
            // Requires current tuple context
            stack_.push(Value::fromBigInt(ctx_->getCurrentVisibleXactId()));
            break;

        // ...
    }
    break;
}
```

---

### Phase 3: Row Identity Context Variables (4-6 hours)

**Task 9.3.1: `sdb$key` Implementation** (2-3 hours)
- Add `SdbKey` struct to `types.h`
- Implement `toString()` and `fromString()`
- Add `getCurrentSdbKey()` to `ConnectionContext`
- Test hash index on `sdb$key`

**Task 9.3.2: `rdb$row_uuid` Implementation** (2-3 hours)
- Extend `TupleHeader` with `row_uuid` field (16 bytes)
- Generate UUID v7 on INSERT
- Add `getCurrentRowUuid()` to `ConnectionContext`
- Test UUID persistence across backup/restore

See **ALPHA_ROW_IDENTITY_ENHANCED.md** for detailed implementation.

---

### Phase 4: Date/Time Context Variables (3-4 hours)

**Task 9.4.1: PSQL Caching** (2 hours)
```cpp
// In ConnectionContext.h
class ConnectionContext {
    mutable std::optional<Date> current_date_cache_;
    mutable std::optional<TimeWithTZ> current_time_cache_;
    mutable std::optional<TimestampWithTZ> current_timestamp_cache_;
    uint32_t psql_depth_ = 0;  // Track PSQL call depth

public:
    void enterPsqlModule() {
        if (psql_depth_++ == 0) {
            // Reset cache on outermost module entry
            resetPsqlCache();
        }
    }

    void exitPsqlModule() {
        if (--psql_depth_ == 0) {
            // Keep cache until outermost module exits
        }
    }

    void resetPsqlCache() {
        current_date_cache_.reset();
        current_time_cache_.reset();
        current_timestamp_cache_.reset();
    }
};
```

**Task 9.4.2: String Literal Mnemonics** (1-2 hours)
```cpp
// In semantic_analyzer.cpp
auto SemanticAnalyzer::analyzeStringLiteral(const ASTStringLiteral* expr) -> ValueType {
    std::string_view str = expr->getValue();

    // Check for datetime mnemonics
    std::string upper = toUpper(trim(str));
    if (upper == "NOW" || upper == "TODAY" ||
        upper == "YESTERDAY" || upper == "TOMORROW") {
        // Mark as datetime mnemonic (special CAST behavior)
        expr->setDateTimeMnemonic(true);
    }

    return ValueType::VARCHAR;
}

// In executor.cpp (OP_CAST)
if (value.isString() && isCastToDateTimeType(target_type)) {
    std::string str = toUpper(trim(value.getString()));

    if (str == "NOW") {
        // Always get ACTUAL current timestamp (not cached)
        return Value::fromTimestamp(Clock::now());
    } else if (str == "TODAY") {
        return Value::fromDate(Clock::today());
    } else if (str == "YESTERDAY") {
        return Value::fromDate(Clock::today() - 1_day);
    } else if (str == "TOMORROW") {
        return Value::fromDate(Clock::today() + 1_day);
    }
}
```

---

### Phase 5: Trigger Context Variables (2-3 hours)

**Task 9.5.1: Trigger Context Tracking** (1-2 hours)
```cpp
// In ConnectionContext.h
class ConnectionContext {
    bool in_trigger_ = false;
    TriggerOperation trigger_op_ = TriggerOperation::NONE;

public:
    void enterTrigger(TriggerOperation op) {
        in_trigger_ = true;
        trigger_op_ = op;
    }

    void exitTrigger() {
        in_trigger_ = false;
        trigger_op_ = TriggerOperation::NONE;
    }

    [[nodiscard]] auto isInserting() const -> bool {
        return trigger_op_ == TriggerOperation::INSERT;
    }

    [[nodiscard]] auto isUpdating() const -> bool {
        return trigger_op_ == TriggerOperation::UPDATE;
    }

    [[nodiscard]] auto isDeleting() const -> bool {
        return trigger_op_ == TriggerOperation::DELETE;
    }
};
```

**Task 9.5.2: NEW/OLD Record Access** (1 hour)
```cpp
// In ConnectionContext.h
class ConnectionContext {
    const Tuple* new_tuple_ = nullptr;
    const Tuple* old_tuple_ = nullptr;

public:
    void setTriggerRecords(const Tuple* old_tuple, const Tuple* new_tuple) {
        old_tuple_ = old_tuple;
        new_tuple_ = new_tuple;
    }

    [[nodiscard]] auto getNewTuple() const -> const Tuple* {
        return new_tuple_;
    }

    [[nodiscard]] auto getOldTuple() const -> const Tuple* {
        return old_tuple_;
    }
};
```

---

### Phase 6: Error Handling Context Variables (2-3 hours)

**Task 9.6.1: Error Context Tracking** (1-2 hours)
```cpp
// In ConnectionContext.h
class ConnectionContext {
    int32_t gdscode_ = 0;
    std::string sqlstate_ = "00000";  // '00000' = success
    int32_t row_count_ = 0;

public:
    void setErrorContext(int32_t gdscode, std::string_view sqlstate) {
        gdscode_ = gdscode;
        sqlstate_ = sqlstate;
    }

    void clearErrorContext() {
        gdscode_ = 0;
        sqlstate_ = "00000";
    }

    void setRowCount(int32_t count) {
        row_count_ = count;
    }

    [[nodiscard]] auto getGdsCode() const -> int32_t { return gdscode_; }
    [[nodiscard]] auto getSqlState() const -> std::string_view { return sqlstate_; }
    [[nodiscard]] auto getRowCount() const -> int32_t { return row_count_; }
};
```

**Task 9.6.2: Exception Handler Integration** (1 hour)
- Update exception handlers to set GDSCODE/SQLSTATE
- Track ROW_COUNT in executor after each DML statement

---

## 10. Testing Strategy

### Unit Tests (35 tests)

**Connection Context** (5 tests):
```cpp
TEST(ContextVariablesTest, CurrentConnection) {
    auto db = Database::create(":memory:");
    auto ctx1 = db->createConnection();
    auto ctx2 = db->createConnection();

    EXPECT_NE(ctx1->getConnectionId(), ctx2->getConnectionId());
    EXPECT_GT(ctx2->getConnectionId(), ctx1->getConnectionId());
}

TEST(ContextVariablesTest, CurrentUser) {
    auto ctx = createContext("admin");
    EXPECT_EQ(ctx->getUsername(), "admin");
}
```

**Transaction Context** (5 tests):
```cpp
TEST(ContextVariablesTest, CurrentTransaction) {
    auto ctx = createContext();
    ctx->beginTransaction();

    uint64_t txn_id = ctx->getCurrentTransactionId();
    EXPECT_GT(txn_id, 0);
}

TEST(ContextVariablesTest, RdbXactId) {
    // Insert row in transaction 10
    ctx->beginTransaction();  // txn 10
    ctx->execute("INSERT INTO customers VALUES (1, 'Alice')");
    ctx->commit();

    // Query in transaction 11
    ctx->beginTransaction();  // txn 11
    auto result = ctx->execute("SELECT rdb$xact_id FROM customers WHERE customer_id = 1");
    EXPECT_EQ(result[0]["rdb$xact_id"], 10);  // Created by txn 10
}
```

**Row Identity** (8 tests):
```cpp
TEST(ContextVariablesTest, SdbKey) {
    ctx->execute("INSERT INTO customers VALUES (1, 'Alice')");
    auto result = ctx->execute("SELECT sdb$key FROM customers");

    std::string sdb_key = result[0]["sdb$key"];
    EXPECT_MATCH(sdb_key, R"([0-9A-F]{4}-[0-9A-F]{12}-[0-9A-F]{4})");
}

TEST(ContextVariablesTest, RdbRowUuid) {
    ctx->execute("INSERT INTO customers VALUES (1, 'Alice')");
    auto result = ctx->execute("SELECT rdb$row_uuid FROM customers");

    std::string uuid = result[0]["rdb$row_uuid"];
    EXPECT_TRUE(isValidUuidV7(uuid));
}

TEST(ContextVariablesTest, SdbKeyHashIndex) {
    ctx->execute("CREATE INDEX idx ON customers USING HASH (sdb$key)");
    ctx->execute("INSERT INTO customers VALUES (1, 'Alice')");

    auto result = ctx->execute("SELECT sdb$key FROM customers WHERE customer_id = 1");
    std::string sdb_key = result[0]["sdb$key"];

    // Fast O(1) lookup by sdb$key
    auto result2 = ctx->execute("SELECT customer_id FROM customers WHERE sdb$key = ?", sdb_key);
    EXPECT_EQ(result2[0]["customer_id"], 1);
}

TEST(ContextVariablesTest, RdbRowUuidPersistence) {
    // Insert row
    ctx->execute("INSERT INTO customers VALUES (1, 'Alice')");
    auto result1 = ctx->execute("SELECT rdb$row_uuid FROM customers WHERE customer_id = 1");
    std::string uuid1 = result1[0]["rdb$row_uuid"];

    // Backup and restore
    db->backup("backup.sbdb");
    auto db2 = Database::restore("backup.sbdb");
    auto ctx2 = db2->createConnection();

    // UUID should be preserved
    auto result2 = ctx2->execute("SELECT rdb$row_uuid FROM customers WHERE customer_id = 1");
    std::string uuid2 = result2[0]["rdb$row_uuid"];
    EXPECT_EQ(uuid1, uuid2);
}
```

**Date/Time** (10 tests):
```cpp
TEST(ContextVariablesTest, CurrentDate) {
    auto result = ctx->execute("SELECT CURRENT_DATE FROM rdb$database");
    Date date = result[0]["CURRENT_DATE"];
    EXPECT_EQ(date, Date::today());
}

TEST(ContextVariablesTest, CurrentTimePrecision) {
    auto result1 = ctx->execute("SELECT CURRENT_TIME FROM rdb$database");
    auto result2 = ctx->execute("SELECT CURRENT_TIME(3) FROM rdb$database");

    // Default precision = 0 (seconds)
    EXPECT_EQ(result1[0]["CURRENT_TIME"].getPrecision(), 0);

    // Explicit precision = 3 (milliseconds)
    EXPECT_EQ(result2[0]["CURRENT_TIME"].getPrecision(), 3);
}

TEST(ContextVariablesTest, PsqlStability) {
    ctx->execute(R"(
        CREATE PROCEDURE test_stability
        RETURNS (t1 TIMESTAMP, t2 TIMESTAMP)
        AS BEGIN
            t1 = CURRENT_TIMESTAMP;
            -- Sleep for 100ms
            t2 = CURRENT_TIMESTAMP;
            SUSPEND;
        END
    )");

    auto result = ctx->execute("SELECT * FROM test_stability");
    EXPECT_EQ(result[0]["t1"], result[0]["t2"]);  // Same value in PSQL
}

TEST(ContextVariablesTest, NowProgressing) {
    ctx->execute(R"(
        CREATE PROCEDURE test_now_progressing
        RETURNS (t1 TIMESTAMP, t2 TIMESTAMP)
        AS BEGIN
            t1 = CAST('NOW' AS TIMESTAMP);
            -- Sleep for 100ms
            t2 = CAST('NOW' AS TIMESTAMP);
            SUSPEND;
        END
    )");

    auto result = ctx->execute("SELECT * FROM test_now_progressing");
    EXPECT_GT(result[0]["t2"], result[0]["t1"]);  // 'NOW' progresses
}

TEST(ContextVariablesTest, TodayYesterdayTomorrow) {
    auto today = ctx->execute("SELECT CAST('TODAY' AS DATE)")[0][0];
    auto yesterday = ctx->execute("SELECT CAST('YESTERDAY' AS DATE)")[0][0];
    auto tomorrow = ctx->execute("SELECT CAST('TOMORROW' AS DATE)")[0][0];

    EXPECT_EQ(yesterday, today - 1_day);
    EXPECT_EQ(tomorrow, today + 1_day);
}
```

**Trigger Context** (5 tests):
```cpp
TEST(ContextVariablesTest, InsertingUpdatingDeleting) {
    ctx->execute(R"(
        CREATE TRIGGER trg_audit FOR customers
        AFTER INSERT OR UPDATE OR DELETE
        AS BEGIN
            IF (INSERTING) THEN
                INSERT INTO audit_log VALUES ('INSERT');
            ELSE IF (UPDATING) THEN
                INSERT INTO audit_log VALUES ('UPDATE');
            ELSE IF (DELETING) THEN
                INSERT INTO audit_log VALUES ('DELETE');
            END IF;
        END
    )");

    ctx->execute("INSERT INTO customers VALUES (1, 'Alice')");
    EXPECT_EQ(ctx->execute("SELECT * FROM audit_log")[0]["action"], "INSERT");

    ctx->execute("UPDATE customers SET name = 'Bob' WHERE customer_id = 1");
    EXPECT_EQ(ctx->execute("SELECT * FROM audit_log")[1]["action"], "UPDATE");

    ctx->execute("DELETE FROM customers WHERE customer_id = 1");
    EXPECT_EQ(ctx->execute("SELECT * FROM audit_log")[2]["action"], "DELETE");
}

TEST(ContextVariablesTest, NewOldRecords) {
    ctx->execute(R"(
        CREATE TRIGGER trg_balance_history FOR customers
        AFTER UPDATE
        AS BEGIN
            INSERT INTO balance_history VALUES (OLD.balance, NEW.balance);
        END
    )");

    ctx->execute("INSERT INTO customers VALUES (1, 100.00)");
    ctx->execute("UPDATE customers SET balance = 200.00 WHERE customer_id = 1");

    auto result = ctx->execute("SELECT * FROM balance_history");
    EXPECT_EQ(result[0]["old_balance"], 100.00);
    EXPECT_EQ(result[0]["new_balance"], 200.00);
}
```

**Error Handling** (2 tests):
```cpp
TEST(ContextVariablesTest, RowCount) {
    ctx->execute(R"(
        CREATE PROCEDURE upsert(id BIGINT, balance DECIMAL)
        AS BEGIN
            UPDATE customers SET balance = :balance WHERE customer_id = :id;
            IF (ROW_COUNT = 0) THEN
                INSERT INTO customers VALUES (:id, :balance);
            END IF;
        END
    )");

    ctx->execute("EXECUTE PROCEDURE upsert(1, 100.00)");
    EXPECT_EQ(ctx->execute("SELECT balance FROM customers WHERE customer_id = 1")[0]["balance"], 100.00);

    ctx->execute("EXECUTE PROCEDURE upsert(1, 200.00)");
    EXPECT_EQ(ctx->execute("SELECT balance FROM customers WHERE customer_id = 1")[0]["balance"], 200.00);
}

TEST(ContextVariablesTest, SqlState) {
    ctx->execute(R"(
        CREATE PROCEDURE test_sqlstate
        RETURNS (state CHAR(5))
        AS BEGIN
            BEGIN
                -- Trigger division by zero
                state = 1 / 0;
            EXCEPTION
                WHEN ANY DO
                    state = SQLSTATE;
            END
            SUSPEND;
        END
    )");

    auto result = ctx->execute("SELECT * FROM test_sqlstate");
    EXPECT_EQ(result[0]["state"], "22012");  // Division by zero
}
```

---

## 11. Documentation Requirements

### User Documentation

**SQL Reference: Context Variables** (new section):
- List all context variables with descriptions
- Examples for each variable
- PSQL stability vs progressing values
- String literal mnemonics (`'NOW'`, `'TODAY'`, etc.)

**Migration Guide: Firebird to ScratchBird**:
- Context variable compatibility matrix
- Differences from Firebird (if any)
- New ScratchBird-specific variables (`sdb$key`, `rdb$row_uuid`, `rdb$xact_id`)

### Developer Documentation

**Context Variables Implementation**:
- Architecture diagram (lexer → parser → semantic → bytecode → executor)
- ConnectionContext state management
- PSQL caching mechanism
- Trigger context tracking

---

## 12. Success Criteria

- [ ] All 24 Firebird context variables implemented
- [ ] 3 new ScratchBird context variables implemented (`sdb$key`, `rdb$row_uuid`, `rdb$xact_id`)
- [ ] PSQL stability for date/time variables
- [ ] String literal mnemonics (`'NOW'`, `'TODAY'`, etc.) work correctly
- [ ] Trigger context variables (`INSERTING`, `UPDATING`, `DELETING`, `NEW`, `OLD`) functional
- [ ] Error handling variables (`GDSCODE`, `SQLSTATE`, `ROW_COUNT`) work in PSQL
- [ ] 35 unit tests passing
- [ ] Performance: < 1% overhead for context variable access
- [ ] Documentation complete

---

## 13. Risks and Mitigations

### Risk: PSQL Caching Complexity

**Risk**: Tracking PSQL depth and managing cache lifetimes is error-prone.

**Mitigation**:
- Use RAII pattern for PSQL module entry/exit
- Extensive testing of nested PSQL calls
- Clear documentation of caching semantics

### Risk: Context Variable Availability

**Risk**: Users may try to use trigger-only variables outside triggers.

**Mitigation**:
- Semantic analyzer checks availability
- Clear error messages (e.g., "INSERTING only available in triggers")
- Comprehensive documentation

---

## 14. Future Enhancements (Post-ALPHA)

### Additional Context Variables

- `CURRENT_SCHEMA`: Current schema name
- `SESSION_ID`: Unique session identifier
- `SERVER_VERSION`: ScratchBird version string
- `EFFECTIVE_USER`: User after SET ROLE
- `SYSTEM_USER`: Operating system user

### Extended Precision

- Microsecond precision (6 decimals) for `CURRENT_TIME`/`CURRENT_TIMESTAMP`
- Nanosecond precision (9 decimals) for high-resolution timers

---

**Document Version**: 1.0
**Last Updated**: 2025-10-24
**Status**: DRAFT
**Prerequisites**: ALPHA_ROW_IDENTITY_ENHANCED.md

**Next Steps**:
1. Review design with stakeholders
2. Begin implementation of Phase 1 (Core Infrastructure)
3. Create unit test stubs
4. Update PROJECT_CONTEXT.md with new feature
