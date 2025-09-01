# Phase 6: MGA-Based Transactions (PRIMARY ACID)

## Objective
Implement Multi-Generational Architecture for transactions - the PRIMARY mechanism for ACID (minus durability).

## Prerequisites
- Phase 5 complete (space allocation)

## Architecture Note
**MGA is PRIMARY**: This phase implements Firebird-style MGA which provides Atomicity, Consistency, and Isolation WITHOUT needing WAL. WAL will be added later ONLY for durability.

## Technical Specifications
- **MGA Core**: See `/references/technical_specifications/TRANSACTION_MGA_CORE.md`
- **Lock Manager**: See `/references/technical_specifications/TRANSACTION_LOCK_MANAGER.md`
- **Main Integration**: See `/references/technical_specifications/TRANSACTION_MAIN.md`

## Tasks

### 6.1 Transaction Inventory Pages (TIP)
```cpp
// TIP stores transaction states persistently
enum TxnState {
    Active = 1,
    Committed = 2,
    Aborted = 3,
    Limbo = 4  // For two-phase commit
};

// One TIP entry per transaction
struct TIPEntry {
    TxnState state;
    uint64_t snapshot_number;  // For sub-transactions
};
```

### 6.2 Version-Aware Tuple Format
```cpp
struct MGATupleHeader {
    uint64_t created_xid;    // Creating transaction
    uint64_t deleted_xid;    // Deleting transaction (0 if live)
    uint64_t backptr_rid;    // Previous version RowID
    uint16_t flags;          // Version flags
    // Followed by actual tuple data
};
```

### 6.3 Transaction Operations (No WAL Required)
```cpp
Transaction begin_transaction(Session*) {
    // Allocate XID
    // Mark Active in TIP
    // Create snapshot
    // NO WAL record needed
}

Status commit(Transaction* txn) {
    // Update TIP: Active -> Committed
    // NO WAL flush required for correctness
    // Commit is visible immediately via TIP
}

Status rollback(Transaction* txn) {
    // Update TIP: Active -> Aborted
    // NO undo needed - old versions still exist
}
```

### 6.4 Version Chain Management
- INSERT: Create new version with created_xid
- UPDATE: Create new version, mark old with deleted_xid
- DELETE: Mark with deleted_xid only
- No physical removal during transaction

### 6.5 Snapshot Creation
```cpp
Snapshot create_snapshot() {
    // Scan TIP for active transactions
    // Record XID boundaries
    // NO locks needed
}
```

## Files to Create/Modify
- `include/scratchbird/engine/mga.h`
- `src/engine/mga_transaction.cpp`
- `src/engine/tip_manager.cpp`

## Validation Tests
```cpp
// Test MGA without WAL
disable_wal();  // MGA must work without WAL

// Atomicity via MGA
auto txn1 = begin_transaction(session);
execute(txn1, "INSERT INTO test VALUES (1, 'data')");
// Crash before commit
simulate_crash();
restart_database();
auto result = execute("SELECT * FROM test");
assert(result.rows.size() == 0);  // Atomicity without WAL

// Isolation via MGA
auto txn2 = begin_transaction(session1);
auto txn3 = begin_transaction(session2);
execute(txn2, "UPDATE test SET val = 2");
auto result = execute(txn3, "SELECT val FROM test");
assert(result == old_value);  // Isolation via versioning

// Rollback is free
auto txn4 = begin_transaction(session);
execute(txn4, "UPDATE test SET val = 99");
rollback(txn4);  // Just marks TIP entry as Aborted
// Old version still visible, no undo needed
```

## Exit Criteria
- Transactions work WITHOUT WAL
- ACID (except Durability) provided by MGA alone
- Version chains properly maintained
- TIP correctly tracks transaction states
- No read locks ever taken