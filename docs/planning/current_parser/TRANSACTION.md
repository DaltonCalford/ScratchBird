# ScratchBird Parser - Transaction Control Commands

## Document Purpose

Complete audit of all transaction control commands implemented in the ScratchBird parser. This document is part of the comprehensive parser audit for rebuilding as a context-sensitive parser.

---

## 1. START TRANSACTION

### Location
- **Parser Method**: `parseStartTransaction()` (parser.cpp:4094-4299)
- **AST Node**: `StartTransactionStmt`

### BNF Syntax

```bnf
start_transaction_stmt ::=
    START TRANSACTION
    [ transaction_mode ]
    [ wait_mode ]
    [ isolation_clause ]
    [ lock_timeout_clause ]
    [ reserving_clause ]
    [ commit_outstanding_clause ]

transaction_mode ::=
    READ WRITE
    | READ ONLY
    | READ COMMITTED    -- Alternative form for isolation level

wait_mode ::=
    NO WAIT             -- Note: uses "NOT WAIT" in parser (KW_NOT + KW_WAIT)

isolation_clause ::=
    ISOLATION LEVEL isolation_level

isolation_level ::=
    READ COMMITTED
    | SNAPSHOT
    | SNAPSHOT TABLE STABILITY

lock_timeout_clause ::=
    LOCK TIMEOUT integer_literal

reserving_clause ::=
    RESERVING table_reservation_list

table_reservation_list ::=
    table_reservation [ , table_reservation ]*

table_reservation ::=
    table_name FOR lock_mode access_mode

lock_mode ::=
    SHARED
    | PROTECTED

access_mode ::=
    READ
    | WRITE

commit_outstanding_clause ::=
    WITH COMMIT OUTSTANDING
```

### Keywords Used

| Keyword | Token Type | Required |
|---------|------------|----------|
| `START` | `KW_START` | Yes |
| `TRANSACTION` | `KW_TRANSACTION` | Yes |
| `READ` | `KW_READ` | Optional |
| `WRITE` | `KW_WRITE` | Optional |
| `ONLY` | `KW_ONLY` | Optional |
| `NOT` | `KW_NOT` | Optional |
| `WAIT` | `KW_WAIT` | Optional |
| `ISOLATION` | `KW_ISOLATION` | Optional |
| `LEVEL` | `KW_LEVEL` | Optional |
| `COMMITTED` | `KW_COMMITTED` | Optional |
| `SNAPSHOT` | `KW_SNAPSHOT` | Optional |
| `TABLE` | `KW_TABLE` | Optional |
| `STABILITY` | `KW_STABILITY` | Optional |
| `LOCK` | `KW_LOCK` | Optional |
| `TIMEOUT` | `KW_TIMEOUT` | Optional |
| `RESERVING` | `KW_RESERVING` | Optional |
| `FOR` | `KW_FOR` | Optional |
| `SHARED` | `KW_SHARED` | Optional |
| `PROTECTED` | `KW_PROTECTED` | Optional |
| `WITH` | `KW_WITH` | Optional |
| `COMMIT` | `KW_COMMIT` | Optional |
| `OUTSTANDING` | `KW_OUTSTANDING` | Optional |

### AST Fields

```cpp
class StartTransactionStmt : public Statement {
    TransactionMode mode_;              // READ_WRITE or READ_ONLY
    IsolationLevel isolation_;          // READ_COMMITTED, SNAPSHOT, SNAPSHOT_TABLE_STABILITY
    bool wait_;                         // true = WAIT (default), false = NO WAIT
    bool commit_outstanding_;           // WITH COMMIT OUTSTANDING flag
    uint32_t lock_timeout_;             // LOCK TIMEOUT value (0 = not specified)
    std::vector<TableReservation> table_reservations_;  // RESERVING list
};

struct TableReservation {
    StringPool::StringId table_name;
    TableLockMode lock_mode;            // SHARED or PROTECTED
    bool for_write;                     // READ = false, WRITE = true
};
```

### Enums

```cpp
enum class TransactionMode {
    READ_WRITE,
    READ_ONLY
};

enum class IsolationLevel {
    READ_COMMITTED,
    SNAPSHOT,
    SNAPSHOT_TABLE_STABILITY
};

enum class TableLockMode {
    SHARED,
    PROTECTED
};
```

### Default Values
- `mode`: `READ_WRITE`
- `isolation`: `READ_COMMITTED`
- `wait`: `true` (WAIT)
- `commit_outstanding`: `false`
- `lock_timeout`: `0` (no timeout)
- `table_reservations`: empty

### Examples

```sql
-- Minimal
START TRANSACTION;

-- Read-only
START TRANSACTION READ ONLY;

-- With isolation level
START TRANSACTION ISOLATION LEVEL SNAPSHOT;

-- Full Firebird-style with reservations
START TRANSACTION READ WRITE
    NO WAIT
    ISOLATION LEVEL SNAPSHOT TABLE STABILITY
    LOCK TIMEOUT 30
    RESERVING orders FOR PROTECTED WRITE,
              customers FOR SHARED READ
    WITH COMMIT OUTSTANDING;
```

### Implementation Notes
- "NO WAIT" is parsed as `NOT WAIT` using `KW_NOT` then `KW_WAIT`
- `READ COMMITTED` can appear directly after `READ` (ambiguous with `READ WRITE`)
- Multiple table reservations are comma-separated

---

## 2. SET TRANSACTION

### Location
- **Parser Method**: `parseSetTransaction()` (parser.cpp:4301-4488)
- **AST Node**: `SetTransactionStmt`

### BNF Syntax

```bnf
set_transaction_stmt ::=
    SET TRANSACTION
    [ transaction_mode ]
    [ wait_mode ]
    [ isolation_clause ]
    [ lock_timeout_clause ]
    [ reserving_clause ]
```

### Description

Identical syntax to `START TRANSACTION` but uses `SET TRANSACTION` prefix. Does not support `WITH COMMIT OUTSTANDING`.

### Keywords Used
Same as START TRANSACTION, replacing `START` with `SET`.

### Examples

```sql
SET TRANSACTION READ ONLY;
SET TRANSACTION ISOLATION LEVEL SNAPSHOT;
SET TRANSACTION READ WRITE LOCK TIMEOUT 60;
```

---

## 3. COMMIT

### Location
- **Parser Method**: `parseCommit()` (parser.cpp:4599-4606)
- **AST Node**: `CommitStmt`

### BNF Syntax

```bnf
commit_stmt ::= COMMIT
```

### Keywords Used

| Keyword | Token Type | Required |
|---------|------------|----------|
| `COMMIT` | `KW_COMMIT` | Yes |

### AST Fields

```cpp
class CommitStmt : public Statement {
    // No additional fields - simple statement
};
```

### Implementation Notes
- Very simple implementation - just recognizes `COMMIT` keyword
- Does not currently support `COMMIT WORK` or `COMMIT RETAIN` variants
- No optional clauses implemented

### Examples

```sql
COMMIT;
```

---

## 4. ROLLBACK

### Location
- **Parser Method**: `parseRollback()` (parser.cpp:4608-4634)
- **AST Node**: `RollbackStmt` or `RollbackToSavepointStmt`

### BNF Syntax

```bnf
rollback_stmt ::=
    ROLLBACK
    | ROLLBACK TO [ SAVEPOINT ] savepoint_name

savepoint_name ::= identifier
```

### Keywords Used

| Keyword | Token Type | Required |
|---------|------------|----------|
| `ROLLBACK` | `KW_ROLLBACK` | Yes |
| `TO` | `KW_TO` | Optional |
| `SAVEPOINT` | `KW_SAVEPOINT` | Optional |

### AST Fields

```cpp
class RollbackStmt : public Statement {
    // No additional fields - full rollback
};

class RollbackToSavepointStmt : public Statement {
    StringPool::StringId savepoint_name_;
};
```

### Implementation Notes
- Two different AST nodes depending on whether `TO` clause is present
- `SAVEPOINT` keyword after `TO` is optional
- Uses `isIdentifierOrUnreservedKeyword()` for savepoint name flexibility
- Does not support `ROLLBACK WORK` or `ROLLBACK RETAIN` variants

### Examples

```sql
-- Full rollback
ROLLBACK;

-- Rollback to savepoint
ROLLBACK TO my_savepoint;

-- With optional SAVEPOINT keyword
ROLLBACK TO SAVEPOINT my_savepoint;
```

---

## 5. SAVEPOINT

### Location
- **Parser Method**: `parseSavepoint()` (parser.cpp:4636-4652)
- **AST Node**: `SavepointStmt`

### BNF Syntax

```bnf
savepoint_stmt ::= SAVEPOINT savepoint_name

savepoint_name ::= identifier
```

### Keywords Used

| Keyword | Token Type | Required |
|---------|------------|----------|
| `SAVEPOINT` | `KW_SAVEPOINT` | Yes |

### AST Fields

```cpp
class SavepointStmt : public Statement {
    StringPool::StringId savepoint_name_;
};
```

### Implementation Notes
- Uses `isIdentifierOrUnreservedKeyword()` for savepoint name
- Savepoint name is required (not optional)

### Examples

```sql
SAVEPOINT before_updates;
SAVEPOINT sp1;
```

---

## 6. RELEASE SAVEPOINT

### Location
- **Parser Method**: `parseReleaseSavepoint()` (parser.cpp:4654-4676)
- **AST Node**: `ReleaseSavepointStmt`

### BNF Syntax

```bnf
release_savepoint_stmt ::= RELEASE SAVEPOINT savepoint_name

savepoint_name ::= identifier
```

### Keywords Used

| Keyword | Token Type | Required |
|---------|------------|----------|
| `RELEASE` | `KW_RELEASE` | Yes |
| `SAVEPOINT` | `KW_SAVEPOINT` | Yes |

### AST Fields

```cpp
class ReleaseSavepointStmt : public Statement {
    StringPool::StringId savepoint_name_;
};
```

### Implementation Notes
- `SAVEPOINT` keyword is required (unlike `ROLLBACK TO`)
- Uses `isIdentifierOrUnreservedKeyword()` for savepoint name

### Examples

```sql
RELEASE SAVEPOINT before_updates;
RELEASE SAVEPOINT sp1;
```

---

## 7. SWEEP DATABASE

### Location
- **Parser Method**: `parseSweep()` (parser.cpp:4678-4691)
- **AST Node**: `SweepStmt`

### BNF Syntax

```bnf
sweep_stmt ::= SWEEP DATABASE
```

### Keywords Used

| Keyword | Token Type | Required |
|---------|------------|----------|
| `SWEEP` | `KW_SWEEP` | Yes |
| `DATABASE` | `KW_DATABASE` | Yes |

### AST Fields

```cpp
class SweepStmt : public Statement {
    // No additional fields
};
```

### Description

Firebird-specific command to trigger garbage collection sweep of the database. This is part of Firebird's Multi-Generational Architecture (MGA) for cleaning up old record versions.

### Implementation Notes
- Simple two-keyword command
- No options or parameters
- Firebird-specific feature

### Examples

```sql
SWEEP DATABASE;
```

---

## Summary Table

| Command | Keywords | Options | AST Node |
|---------|----------|---------|----------|
| `START TRANSACTION` | 23 | Mode, Isolation, Wait, Timeout, Reserving, Outstanding | `StartTransactionStmt` |
| `SET TRANSACTION` | 22 | Mode, Isolation, Wait, Timeout, Reserving | `SetTransactionStmt` |
| `COMMIT` | 1 | None | `CommitStmt` |
| `ROLLBACK` | 3 | TO SAVEPOINT | `RollbackStmt` / `RollbackToSavepointStmt` |
| `SAVEPOINT` | 1 | None | `SavepointStmt` |
| `RELEASE SAVEPOINT` | 2 | None | `ReleaseSavepointStmt` |
| `SWEEP DATABASE` | 2 | None | `SweepStmt` |

## Firebird MGA Notes

The transaction commands implement Firebird's Multi-Generational Architecture features:

1. **Isolation Levels**:
   - `READ COMMITTED` - Default, sees committed changes from other transactions
   - `SNAPSHOT` - Transaction sees a consistent snapshot from start
   - `SNAPSHOT TABLE STABILITY` - Like SNAPSHOT but with table-level locking

2. **Table Reservations**:
   - `SHARED READ` - Multiple transactions can read
   - `SHARED WRITE` - Multiple transactions can read/write
   - `PROTECTED READ` - Exclusive read access
   - `PROTECTED WRITE` - Exclusive read/write access

3. **Lock Timeout**:
   - Specifies how long to wait for locked resources
   - 0 = wait indefinitely (with `WAIT` mode)

4. **NO WAIT**:
   - Immediately fail if resource is locked
   - Alternative to using lock timeout

5. **WITH COMMIT OUTSTANDING**:
   - Firebird-specific for deferred constraint checking
