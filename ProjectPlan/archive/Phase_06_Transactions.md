# Phase 6: Transaction Management

## Objective
Implement transaction support with ACID properties.

## Prerequisites
- Phase 5 complete (space allocation)

## Tasks

### 6.1 Transaction ID Management
- 64-bit transaction IDs
- Monotonic allocation
- Wraparound handling

### 6.2 TIP (Transaction Inventory Page)
Track transaction states:
```cpp
enum TxnState {
    Active = 1,
    Committed = 2,
    Aborted = 3
};
```

### 6.3 Transaction Operations
```cpp
Transaction begin_transaction(Session*);
Status commit(Transaction*);
Status rollback(Transaction*);
uint64_t get_current_xid(Transaction*);
```

### 6.4 Tuple Visibility
- Add `created_xid` and `deleted_xid` to tuples
- Implement visibility checks
- Support read-uncommitted initially

### 6.5 Write-Ahead Logging (Basic)
- Log transaction begin/commit/abort
- No recovery in this phase

## Files to Create/Modify
- `include/scratchbird/engine/txn.h`
- `src/engine/txn.cpp`

## Validation Tests
```cpp
// Transaction lifecycle
auto txn = begin_transaction(session);
execute(txn, "INSERT INTO test VALUES (1)");
commit(txn);

// Rollback test
auto txn2 = begin_transaction(session);
execute(txn2, "INSERT INTO test VALUES (2)");
rollback(txn2);
// Value 2 should not be visible

// Concurrent transactions
auto txn3 = begin_transaction(session1);
auto txn4 = begin_transaction(session2);
assert(get_current_xid(txn4) > get_current_xid(txn3));
```

## Exit Criteria
- Transactions begin, commit, rollback correctly
- Changes visible after commit
- Changes invisible after rollback