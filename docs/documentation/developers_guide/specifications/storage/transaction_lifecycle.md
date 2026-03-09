# Specification: Transaction Lifecycle

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h:36`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:307`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_transaction_manager.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_concurrent_transactions.cpp`

## Synopsis

This specification defines the transaction lifecycle in ScratchBird, including transaction states, state transitions, and the Transaction Inventory Page (TIP) format used to track transaction states for MVCC visibility.

## Scope

### In Scope

- Transaction states and state transitions
- Transaction Inventory Page (TIP) format
- Transaction begin, commit, rollback, and prepare operations
- Group commit optimization
- Transaction marker maintenance (OIT, OAT, OST)

### Out of Scope

- Lock management (see Lock Manager specifications)
- Distributed transactions (2PC coordinator logic)
- Savepoints and subtransactions (separate specification)

## Background

ScratchBird uses a Firebird-style Multi-Generational Architecture (MGA) where transaction state is stored in Transaction Inventory Pages (TIP). Each transaction has a state that determines visibility of its changes to other transactions.

Key transaction markers:
- **OIT (Oldest Interesting Transaction)**: Oldest transaction that may have uncommitted changes
- **OAT (Oldest Active Transaction)**: Oldest currently running transaction
- **OST (Oldest Snapshot Transaction)**: Oldest transaction using snapshot isolation

## Specification

### Data Structures

#### TransactionState Enum

```cpp
// From include/scratchbird/core/transaction_manager.h:36
enum class TransactionState : uint8_t {
    ACTIVE = 0,      // Transaction in progress
    COMMITTED = 1,   // Transaction committed
    ABORTED = 2,     // Transaction rolled back
    PREPARED = 3,    // Transaction prepared for 2PC
};
```

#### TIP Page Header

```cpp
// From include/scratchbird/core/transaction_manager.h:65
#pragma pack(push, 1)
struct TIPPageHeader {
    PageHeader page_header;    // Standard page header (80 bytes)
    uint64_t min_xid;          // Minimum XID in this page
    uint64_t max_xid;          // Maximum XID in this page
    uint32_t num_transactions; // Number of transactions in this page
    uint32_t next_tip_page;    // Next TIP page ID (0 if last)
    uint8_t reserved[20];      // Reserved for future use
};
#pragma pack(pop)
```

**Binary Layout:**

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | page_header | 80 bytes | Standard page header |
| 0x50 | min_xid | 8 bytes | Minimum XID stored |
| 0x58 | max_xid | 8 bytes | Maximum XID stored |
| 0x60 | num_transactions | 4 bytes | Count of entries |
| 0x64 | next_tip_page | 4 bytes | Linked list pointer |
| 0x68 | reserved | 20 bytes | Padding |

#### TIP Entry

```cpp
// From include/scratchbird/core/transaction_manager.h:76
#pragma pack(push, 1)
struct TIPEntry {
    uint64_t xid;         // Transaction ID
    uint8_t state;        // TransactionState
    uint8_t flags;        // Reserved flags
    uint16_t reserved;    // Alignment padding
    uint64_t commit_time; // Commit/abort timestamp (microseconds since epoch)
};
#pragma pack(pop)
```

**Binary Layout (24 bytes per entry):**

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | xid | 8 bytes | Transaction ID |
| 0x08 | state | 1 byte | State enum value |
| 0x09 | flags | 1 byte | Reserved flags |
| 0x0A | reserved | 2 bytes | Alignment padding |
| 0x0C | commit_time | 8 bytes | Timestamp (0 if ACTIVE) |

#### TransactionSnapshot (for Snapshot Isolation)

```cpp
// From include/scratchbird/core/transaction_manager.h:55
struct TransactionSnapshot {
    uint64_t snapshot_txid_high = 0;           // next_xid at snapshot time
    std::vector<uint64_t> active_txid_set;     // Sorted list of active XIDs
    uint64_t snapshot_commit_seqno_high = 0;   // For future use
};
```

### Interface Contracts

#### Function: `beginTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:307
Status TransactionManager::beginTransaction(
    uint32_t proc_id,     // Process/backend ID
    uint64_t &xid_out,    // Output: allocated XID
    ErrorContext *ctx
);
```

**Preconditions:**
- Backend is registered in ProcArray
- XID wraparound not imminent

**Postconditions:**
- New XID allocated atomically via `fetch_add`
- XID marked ACTIVE in TIP
- XID registered in ProcArray
- IN_PROGRESS status set in CLOG

**State Transition:**
```
[NO TRANSACTION] ──beginTransaction()──► [ACTIVE]
```

#### Function: `commitTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:407
Status TransactionManager::commitTransaction(
    uint32_t proc_id,     // Process/backend ID
    uint64_t xid,         // Transaction to commit
    ErrorContext *ctx
);
```

**Preconditions:**
- Transaction state is ACTIVE
- TIP entry exists for XID

**Postconditions:**
- CLOG status updated to COMMITTED
- TIP entry updated to COMMITTED (via group commit)
- ProcArray slot cleared
- Transaction markers may be updated

**State Transition:**
```
[ACTIVE] ──commitTransaction()──► [COMMITTED]
```

#### Function: `rollbackTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:962
Status TransactionManager::rollbackTransaction(
    uint32_t proc_id,     // Process/backend ID  
    uint64_t xid,         // Transaction to abort
    ErrorContext *ctx
);
```

**Preconditions:**
- Transaction state is ACTIVE

**Postconditions:**
- CLOG status updated to ABORTED
- TIP entry updated to ABORTED
- ProcArray slot cleared

**State Transition:**
```
[ACTIVE] ──rollbackTransaction()──► [ABORTED]
```

#### Function: `prepareTransaction()` (2PC)

```cpp
// Source: src/core/transaction_manager.cpp:588
Status TransactionManager::prepareTransaction(
    uint32_t proc_id,
    uint64_t xid,
    const std::string& gid,    // Global transaction ID
    const ID& owner_id,
    ErrorContext *ctx
);
```

**State Transition:**
```
[ACTIVE] ──prepareTransaction()──► [PREPARED]
```

### Algorithms

#### Algorithm: Transaction State Machine

```
States: ACTIVE, COMMITTED, ABORTED, PREPARED

Transitions:
┌─────────┐  begin()   ┌─────────┐  commit()   ┌─────────┐
│  START  │ ─────────► │ ACTIVE  │ ──────────► │COMMITTED│
└─────────┘            └────┬────┘             └─────────┘
                            │ rollback()
                            ▼
                      ┌─────────┐  prepare()  ┌─────────┐
                      │ ABORTED │ ◄────────── │ PREPARED│
                      └─────────┘   rollback  └────┬────┘
                                                   │ commit
                                                   ▼
                                              ┌─────────┐
                                              │COMMITTED│
                                              └─────────┘
```

#### Algorithm: Group Commit

```
Input:  xid, state (COMMITTED or ABORTED)
Output: Status

1. waiter = CreateCommitWaiter(xid, state)
2. 
3. // Try to become group commit leader
4. ACQUIRE group_commit_mutex_
5. IF NOT group_commit_in_progress_:
6.     group_commit_in_progress_ = true
7.     is_leader = true
8. ELSE:
9.     commit_queue_.push_back(&waiter)
10.    is_leader = false
11. RELEASE group_commit_mutex_
12.
13. IF is_leader:
14.     PERFORM_GROUP_COMMIT(&waiter)
15. ELSE:
16.     WAIT on waiter.cv
17.     RETURN waiter.result

PERFORM_GROUP_COMMIT(leader_waiter):
1. batch = [leader_waiter]
2. deadline = now() + group_commit_timeout_us_
3. 
4. WHILE now() < deadline AND batch.size() < batch_size:
5.     ACQUIRE group_commit_mutex_
6.     WHILE NOT commit_queue_.empty():
7.         batch.push_back(commit_queue_.pop())
8.     RELEASE group_commit_mutex_
9.     SLEEP 1ms
10.
11. // Sort batch by XID for efficient TIP writes
12. SORT batch BY xid
13.
14. // Write all TIP entries
15. FOR EACH waiter IN batch:
16.     writeTipEntry(waiter->xid, waiter->state)
17.
18. // Single fsync for entire batch
19. db_->sync()
20.
21. // Wake all waiters
22. FOR EACH waiter IN batch:
23.     waiter->completed = true
24.     NOTIFY waiter->cv
25.
26. group_commit_in_progress_ = false
27. UPDATE statistics
```

### State Machines

```
┌─────────┐
│  START  │
└────┬────┘
     │ beginTransaction()
     ▼
┌─────────┐     commit()      ┌─────────┐
│  ACTIVE │ ─────────────────► │COMMITTED│
└────┬────┘                    └─────────┘
     │
     │ rollback()
     ▼
┌─────────┐
│ ABORTED │
└─────────┘
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| START | BEGIN | Allocate XID, mark ACTIVE | ACTIVE |
| ACTIVE | COMMIT | Write COMMITTED to TIP, fsync | COMMITTED |
| ACTIVE | ROLLBACK | Write ABORTED to TIP, fsync | ABORTED |
| ACTIVE | PREPARE | Write PREPARED to TIP, fsync | PREPARED |
| PREPARED | COMMIT_PREPARED | Update to COMMITTED | COMMITTED |
| PREPARED | ROLLBACK_PREPARED | Update to ABORTED | ABORTED |

### Decision Trees

```
Transaction completion path:
├── Is group commit enabled?
│   ├── No → Individual TIP write + fsync
│   └── Yes
│       ├── Can become leader? → Perform group commit as leader
│       └── Must wait? → Join queue, wait for leader
│
└── Is durability required?
    ├── Yes → Ensure fsync before returning
    └── No (async) → Queue for background flush
```

## Invariants

1. **Monotonic XID Allocation**: XIDs are allocated in strictly increasing order
   - Verification: `next_xid_` only increases via atomic `fetch_add`
   
2. **TIP Entry Persistence**: All transaction state changes are persisted to TIP before returning
   - Verification: `fsync()` called after TIP write (or via group commit)
   
3. **State Consistency**: CLOG and TIP states must agree for all transactions
   - Verification: Both updated atomically within critical section
   
4. **Valid State Values**: Only valid TransactionState values written to TIP
   - Verification: State cast from enum, validated on read

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_FULL` | XID age approaching 2 billion | Trigger emergency sweep, block new transactions |
| `PAGE_CORRUPT` | XID age >= 2 billion | Block all new transactions until VACUUM |
| `INVALID_ARGUMENT` | Transaction not in ACTIVE state | Return error to caller |
| `IO_ERROR` | Failed to write TIP page | Rollback cache changes, return error |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_transaction_manager.cpp` | Basic transaction lifecycle |
| `tests/unit/test_concurrent_transactions.cpp` | Concurrent transaction handling |
| `tests/unit/test_group_commit.cpp` | Group commit functionality |
| `tests/unit/test_xid_validation_fix.cpp` | XID validation and wraparound |
| `tests/conformance/transactions/test_transaction_truth_native.cpp` | Transaction truth contract |

## Migration Notes

N/A - Initial transaction lifecycle specification for ScratchBird Alpha.

## Related Specifications

- [MGA Visibility Rules](./mga_visibility_rules.md) - How transaction states affect visibility
- [GC Sweep Algorithm](./gc_sweep_algorithm.md) - How OIT is advanced
- [Version Chain Format](./version_chain_format.md) - Record versions tied to transactions

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| TIP | Transaction Inventory Page - stores transaction states |
| XID | Transaction ID - unique identifier for a transaction |
| OIT | Oldest Interesting Transaction - oldest non-committed transaction |
| OAT | Oldest Active Transaction - oldest currently running transaction |
| OST | Oldest Snapshot Transaction - oldest snapshot isolation reader |
| 2PC | Two-Phase Commit - distributed transaction protocol |
| Group Commit | Batch multiple commits for single fsync |
| CLOG | Commit Log - auxiliary transaction status storage |

### References

- Firebird MGA documentation
- `MGA_RULES.md` - Internal MGA implementation rules

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
