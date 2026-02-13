# ScratchBird Transaction Management Design

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** October 7, 2025
**Status:** Design Specification
**Version:** 1.0

---

## Executive Summary

ScratchBird implements a **always-in-transaction** model where every connection is continuously within a transaction from establishment to close. This design ensures all database operations occur within the Multi-Generational Architecture (MGA) transaction system, providing consistent MVCC semantics for all operations including DDL.

---

## Core Design Principle

### Always-In-Transaction Model

**Fundamental Rule:** A connection NEVER exists outside of a transaction.

```
Connection Lifecycle:
┌─────────────────┐
│ Connection Open │ ──> Immediately start Transaction T1
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Work in T1      │ ──> All operations use current XID
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ COMMIT T1       │ ──> Atomically: commit T1, start T2
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Work in T2      │ ──> Continues with new XID
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ ROLLBACK T2     │ ──> Atomically: rollback T2, start T3
└─────────────────┘
         │
         ▼
┌─────────────────┐
│ Connection Close│ ──> Rollback T3, then close
└─────────────────┘
```

**Key Properties:**
- No gap between transactions
- Transaction ID is never NULL/invalid
- All work occurs under MVCC visibility rules
- Consistent snapshot semantics for all operations

---

## Rationale

### Why Always-In-Transaction?

1. **Consistent MVCC Semantics:**
   - Every read has a consistent snapshot
   - No special cases for "outside transaction" reads
   - Simplified visibility logic

2. **DDL Within Transactions:**
   - DDL operations are transactional (can be rolled back)
   - Schema changes visible only after commit
   - Concurrent DDL properly isolated

3. **Simplified Implementation:**
   - No autocommit mode to handle
   - No dual code paths (in-transaction vs out-of-transaction)
   - Single visibility check implementation

4. **PostgreSQL-Compatible Semantics:**
   - Matches PostgreSQL behavior
   - Predictable for users familiar with PostgreSQL
   - Well-understood model with proven correctness

---

## Transaction Lifecycle

### Connection Establishment

```cpp
Status Database::connect(const std::string& db_name, ConnectionContext** ctx_out) {
    // 1. Allocate proc_id from ProcArray
    int32_t proc_id = proc_array_->allocateProcId();

    // 2. Create ConnectionContext
    auto* ctx = new ConnectionContext(proc_id);

    // 3. IMMEDIATELY start initial transaction
    TransactionId initial_xid = transaction_manager_->begin(proc_id);
    ctx->setCurrentTransaction(initial_xid, XACT_READ_WRITE);

    // 4. Return connection with active transaction
    *ctx_out = ctx;
    return Status::OK();
}
```

**Invariant:** Connection returned with valid XID in active transaction.

---

### COMMIT Operation

```cpp
Status ConnectionContext::commit(ErrorContext* err_ctx) {
    TransactionId old_xid = current_xid_;

    // 1. Commit current transaction
    Status s = transaction_manager_->commit(old_xid, err_ctx);
    if (!s.ok()) {
        // Commit failed - transaction remains active
        return s;
    }

    // 2. ATOMICALLY start new transaction (no gap)
    TransactionId new_xid = transaction_manager_->begin(proc_id_);

    // 3. Update context with new transaction
    current_xid_ = new_xid;
    xact_start_time_ = getCurrentTimestamp();
    // Keep same transaction settings (read_only, etc.)

    return Status::OK();
}
```

**Critical:** Steps 1-3 must be atomic. No other operation can observe a state where `current_xid_` is invalid.

---

### ROLLBACK Operation

```cpp
Status ConnectionContext::rollback(ErrorContext* err_ctx) {
    TransactionId old_xid = current_xid_;

    // 1. Rollback current transaction
    Status s = transaction_manager_->rollback(old_xid, err_ctx);
    // Note: Rollback always succeeds (best effort)

    // 2. ATOMICALLY start new transaction (no gap)
    TransactionId new_xid = transaction_manager_->begin(proc_id_);

    // 3. Update context with new transaction
    current_xid_ = new_xid;
    xact_start_time_ = getCurrentTimestamp();
    // Keep same transaction settings (read_only, etc.)

    return Status::OK();
}
```

**Note:** ROLLBACK cannot fail - it's always safe to start a new transaction after rollback.

---

### Connection Close

```cpp
Status Database::disconnect(ConnectionContext* ctx) {
    // 1. Rollback any outstanding transaction
    // (Silent rollback - connection is closing)
    transaction_manager_->rollback(ctx->getCurrentTransactionId(), nullptr);

    // 2. Release proc_id back to ProcArray
    proc_array_->releaseProcId(ctx->getProcId());

    // 3. Destroy ConnectionContext
    delete ctx;

    return Status::OK();
}
```

**Behavior:** Outstanding transaction is implicitly rolled back on disconnect.

---

## START TRANSACTION Command

### Purpose

`START TRANSACTION` changes transaction **settings**, it does NOT start the first transaction (connection already has one).

### Syntax

```sql
START TRANSACTION [transaction_mode [, ...]] [COMMIT OUTSTANDING]

where transaction_mode is:
    READ ONLY
  | READ WRITE
```

### Semantics

#### Without COMMIT OUTSTANDING

```sql
START TRANSACTION READ ONLY;
```

**Behavior:**
1. Current transaction remains active
2. Transaction settings change takes effect for **next** transaction
3. When current transaction commits/rolls back, new transaction starts with new settings

**Example:**
```sql
-- Connection established (in read-write transaction T1)
INSERT INTO foo VALUES (1);  -- Works (in T1, read-write)

START TRANSACTION READ ONLY;  -- Settings staged for next transaction
INSERT INTO foo VALUES (2);   -- Still works (T1 still read-write)

COMMIT;  -- T1 commits, T2 starts as READ ONLY

INSERT INTO foo VALUES (3);  -- ERROR: Transaction is read-only (in T2)
```

---

#### With COMMIT OUTSTANDING

```sql
START TRANSACTION READ ONLY COMMIT OUTSTANDING;
```

**Behavior:**
1. Attempts to commit current transaction
2. If commit succeeds, new transaction starts immediately with new settings
3. If commit fails, command fails and transaction remains active with old settings

**Example Success:**
```sql
-- In transaction T1 (read-write)
INSERT INTO foo VALUES (1);

START TRANSACTION READ ONLY COMMIT OUTSTANDING;
-- T1 commits successfully
-- T2 starts immediately as READ ONLY

SELECT * FROM foo;  -- Works (read operation in read-only T2)
INSERT INTO foo VALUES (2);  -- ERROR: Transaction is read-only
```

**Example Failure:**
```sql
-- In transaction T1 (read-write)
INSERT INTO foo VALUES (1);
-- Assume constraint violation makes commit fail

START TRANSACTION READ ONLY COMMIT OUTSTANDING;
-- T1 commit fails
-- Command returns error
-- STILL IN T1, still read-write

INSERT INTO foo VALUES (2);  -- Still works (T1 still active and read-write)
```

---

### Implementation

```cpp
Status ConnectionContext::startTransaction(bool read_only, bool commit_outstanding,
                                           ErrorContext* err_ctx) {
    if (commit_outstanding) {
        // Attempt to commit current transaction
        Status s = commit(err_ctx);
        if (!s.ok()) {
            // Commit failed - do not change settings
            SET_ERROR_CONTEXT(err_ctx, "Cannot start new transaction: commit failed");
            return s;
        }
        // New transaction already started by commit()
        // Now apply new settings
        is_read_only_ = read_only;
    } else {
        // Stage settings for next transaction
        next_is_read_only_ = read_only;
        settings_changed_ = true;
    }

    return Status::OK();
}
```

---

## DDL Within Transactions

### All DDL is Transactional

```sql
START TRANSACTION;

CREATE TABLE foo (id INT, name VARCHAR(100));
INSERT INTO foo VALUES (1, 'test');

-- Other connections cannot see 'foo' table yet

ROLLBACK;  -- Table creation is rolled back

-- Table 'foo' never existed for other connections
```

### DDL Requiring Exclusive Transaction

Some DDL operations may require being in their own transaction (no other work before or after):

```sql
-- Example: Operations that need exclusive access
ALTER TABLE foo ADD COLUMN bar INT;

-- Implementation may enforce:
-- - Must be first statement in transaction, OR
-- - Must be followed immediately by COMMIT/ROLLBACK
```

**Note:** Even these operations occur **within** a transaction, just with stricter rules about what else can be in that transaction.

---

## ConnectionContext Structure

### Required Fields

```cpp
class ConnectionContext {
public:
    // Process identity
    int32_t proc_id_;              // Unique process ID from ProcArray

    // Current transaction (ALWAYS valid)
    TransactionId current_xid_;    // Current transaction ID (never InvalidTransactionId)
    Timestamp xact_start_time_;    // Transaction start time

    // Transaction settings
    bool is_read_only_;            // Current transaction is read-only

    // Staged settings for next transaction (from START TRANSACTION without COMMIT OUTSTANDING)
    bool settings_changed_;        // Settings staged for next transaction
    bool next_is_read_only_;       // Read-only setting for next transaction

    // Snapshot
    Snapshot snapshot_;            // Current transaction's snapshot

    // Connection state
    bool connected_;               // Connection is active

    // Thread-local storage for current context
    static thread_local ConnectionContext* current_;

public:
    static ConnectionContext* getCurrent() { return current_; }
    static int32_t getCurrentProcId() { return current_ ? current_->proc_id_ : -1; }

    TransactionId getCurrentTransactionId() const { return current_xid_; }
    bool isReadOnly() const { return is_read_only_; }

    Status commit(ErrorContext* err_ctx);
    Status rollback(ErrorContext* err_ctx);
    Status startTransaction(bool read_only, bool commit_outstanding, ErrorContext* err_ctx);

private:
    void applyNextSettings() {
        if (settings_changed_) {
            is_read_only_ = next_is_read_only_;
            settings_changed_ = false;
        }
    }
};
```

---

## Interaction with Other Subsystems

### MVCC Visibility

All visibility checks use `ConnectionContext::getCurrentTransactionId()`:

```cpp
bool HeapPage::isVisible(const ItemPointer& item, Snapshot* snapshot) {
    TransactionId tuple_xmin = getTupleXmin(item);
    TransactionId tuple_xmax = getTupleXmax(item);
    TransactionId my_xid = ConnectionContext::getCurrent()->getCurrentTransactionId();

    // Standard MVCC visibility check
    // my_xid is ALWAYS valid
    return isXactVisible(tuple_xmin, snapshot) &&
           !isXactVisible(tuple_xmax, snapshot);
}
```

### Locking

All lock acquisitions use `ConnectionContext::getCurrentProcId()`:

```cpp
Status BufferPool::getPageForWrite(const PageId& page_id, Page** page_out,
                                   ErrorContext* err_ctx) {
    int32_t proc_id = ConnectionContext::getCurrentProcId();
    // proc_id is ALWAYS valid (never -1)

    Status s = lock_manager_->acquireLock(page_id, proc_id, LOCK_EXCLUSIVE, err_ctx);
    if (!s.ok()) return s;

    // ... proceed with page access
}
```

### Catalog Operations

All catalog operations execute within transactions:

```cpp
Status CatalogManager::createTable(const TableInfo& table_info, ErrorContext* err_ctx) {
    // This executes in current transaction
    TransactionId xid = ConnectionContext::getCurrent()->getCurrentTransactionId();

    // Set tuple xmin to current XID
    catalog_tuple.xmin = xid;
    catalog_tuple.xmax = InvalidTransactionId;

    // Insert into catalog
    // Change is only visible after transaction commits
    return insertCatalogTuple(catalog_tuple, err_ctx);
}
```

---

## Error Handling

### Transaction Errors

When transaction cannot commit:
```cpp
Status s = ctx->commit(&err);
if (!s.ok()) {
    // Transaction remains active
    // Can continue work or rollback
    if (error_is_fatal) {
        ctx->rollback(nullptr);  // Start fresh transaction
    }
}
```

### Connection Errors

When connection fails:
```cpp
// Automatic cleanup ensures transaction is rolled back
Status s = db->executeQuery(sql, &err);
if (!s.ok() && is_connection_error(s)) {
    // Connection cleanup:
    // 1. Rollback outstanding transaction
    // 2. Release locks
    // 3. Free resources
    db->disconnect(ctx);
}
```

---

## Testing Requirements

### Unit Tests

1. **Connection Lifecycle:**
   - `test_connection_starts_with_transaction()` - Verify initial XID is valid
   - `test_connection_close_rolls_back()` - Verify rollback on disconnect

2. **Transaction Transitions:**
   - `test_commit_starts_new_transaction()` - Verify new XID after commit
   - `test_rollback_starts_new_transaction()` - Verify new XID after rollback
   - `test_no_gap_between_transactions()` - Verify XID never invalid

3. **START TRANSACTION:**
   - `test_start_transaction_without_commit_outstanding()` - Staged settings
   - `test_start_transaction_with_commit_outstanding_success()` - Immediate settings
   - `test_start_transaction_with_commit_outstanding_failure()` - Keep old settings

4. **Read-Only Transactions:**
   - `test_read_only_prevents_writes()` - Verify writes fail
   - `test_read_only_allows_reads()` - Verify reads succeed
   - `test_switch_to_read_write()` - Verify can switch back

### Integration Tests

1. **Multi-Connection Scenarios:**
   - `test_concurrent_transactions()` - Multiple connections with independent transactions
   - `test_isolation_between_connections()` - Verify snapshot isolation

2. **DDL Transactions:**
   - `test_ddl_rollback()` - Create table, rollback, verify not visible
   - `test_ddl_commit()` - Create table, commit, verify visible to others
   - `test_ddl_isolation()` - DDL in T1 not visible to T2 until commit

---

## Required State (V3)

ScratchBird V3 MUST operate with:
- A **ConnectionContext** per session, stored in thread-local context.
- Locking enabled in all code paths using the current process id from ConnectionContext.
- TransactionManager fully integrated with session lifecycle (open, commit, rollback, close).
- Parser support for START TRANSACTION and transaction settings.
- Read-only enforcement at executor level.

### Required Integration Steps (Authoritative)

1. **ConnectionContext**
   - Provide thread-local accessors.
   - Bind session id, current transaction id, and current user id.
2. **Call site integration**
   - All lock calls MUST use `getCurrentProcId()` from ConnectionContext.
   - All data operations MUST resolve current XID from ConnectionContext.
3. **Lifecycle**
   - Connection open starts an implicit transaction unless explicit BEGIN/START TRANSACTION is issued.
   - COMMIT/ROLLBACK transitions are deterministic and flush lock state.
4. **Parser/executor**
   - START TRANSACTION is parsed and emitted to SBLR.
   - READ ONLY/READ WRITE is enforced with `ERR_READ_ONLY_VIOLATION`.
5. **Testing**
   - Unit tests and integration tests MUST cover concurrent sessions and DDL isolation.

---

## References

- **Alpha 1.2 Requirements:** `/docs/specifications/parser/v3/issues/ALPHA_1_2_REQUIREMENTS.md`
- **ConnectionContext requirements:** `TRANSACTION_MGA_CORE.md` and `TRANSACTION_LOCK_MANAGER.md`
- **Code Audit:** `/docs/specifications/parser/v3/audits/audit_2025_10_06.md` - Current state analysis
- **PostgreSQL Documentation:** Transaction isolation and MVCC semantics

---

**Document Version:** 1.0
**Date:** October 7, 2025
**Status:** Authoritative (V3)
