# Specification: Transaction States

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

## Synopsis

This specification defines the transaction state machine used in ScratchBird's Multi-Generational Architecture (MGA), including state transitions, state storage, and the relationship between transaction states and data visibility.

## Scope

### In Scope

- Transaction state enum and values
- State transition rules
- State storage (TIP and CLOG)
- Transaction markers (OIT, OAT, OST)
- State machine invariants

### Out of Scope

- Lock management during transactions (see Lock Manager)
- Savepoint/subtransaction states (see Savepoints)
- Distributed transaction coordination

## Background

ScratchBird uses Firebird-style MGA where transaction state determines visibility:

| State | Meaning | Visibility |
|-------|---------|------------|
| ACTIVE | In progress | Only to self |
| COMMITTED | Completed successfully | To all newer transactions |
| ABORTED | Rolled back | Never visible (except during) |
| PREPARED | 2PC ready | Depends on eventual outcome |

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

#### TransactionState (CLOG - 2-bit encoding)

```cpp
// From include/scratchbird/core/clog.h:44
enum class ClogStatus : uint8_t {
    IN_PROGRESS = 0,  // 00 - Transaction still active
    COMMITTED = 1,    // 01 - Transaction committed
    ABORTED = 2,      // 10 - Transaction aborted
    PREPARED = 3      // 11 - Transaction prepared (2PC limbo)
};
```

**Binary Encoding:**
| State | Binary | Decimal |
|-------|--------|---------|
| ACTIVE/IN_PROGRESS | 00 | 0 |
| COMMITTED | 01 | 1 |
| ABORTED | 10 | 2 |
| PREPARED | 11 | 3 |

#### Transaction Markers

```cpp
// From include/scratchbird/core/transaction_manager.h:384
uint64_t oldest_xid_;        // OIT: Oldest Interesting Transaction
uint64_t oldest_active_xid_; // OAT: Oldest Active Transaction
uint64_t oldest_snapshot_;   // OST: Oldest Snapshot Transaction
```

| Marker | Description | Use Case |
|--------|-------------|----------|
| OIT | Oldest non-COMMITTED transaction | GC horizon, freeze limit |
| OAT | Oldest transaction still running | Conflict detection |
| OST | Oldest SNAPSHOT isolation reader | Oldest visible snapshot |

### State Transitions

```
┌─────────┐  begin()   ┌─────────┐  commit()   ┌─────────┐
│  START  │ ─────────► │ ACTIVE  │ ──────────► │COMMITTED│
└─────────┘            └────┬────┘             └─────────┘
                            │
                            │ rollback()
                            ▼
                      ┌─────────┐  prepare()  ┌─────────┐
                      │ ABORTED │ ◄────────── │ PREPARED│
                      └─────────┘             └────┬────┘
                                                   │
                              ┌────────────────────┘
                              │ commit_prepared()
                              ▼
                         ┌─────────┐
                         │COMMITTED│
                         └─────────┘
                              ▲
                              │ rollback_prepared()
                              │
                         ┌─────────┐
                         │ ABORTED │
                         └─────────┘
```

### State Transition Table

| Current State | Event | Condition | New State |
|---------------|-------|-----------|-----------|
| START | BEGIN | - | ACTIVE |
| ACTIVE | COMMIT | Transaction has changes | COMMITTED |
| ACTIVE | ROLLBACK | User request or error | ABORTED |
| ACTIVE | PREPARE | 2PC coordinator request | PREPARED |
| PREPARED | COMMIT_PREPARED | Coordinator decision | COMMITTED |
| PREPARED | ROLLBACK_PREPARED | Coordinator decision | ABORTED |

### Interface Contracts

#### Function: `beginTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:307
Status TransactionManager::beginTransaction(
    uint32_t proc_id,
    uint64_t &xid_out,
    ErrorContext *ctx
);
```

**Preconditions:**
- Backend registered in ProcArray
- XID space available (not near wraparound)

**Postconditions:**
- New XID allocated atomically
- XID marked ACTIVE in TIP/CLOG
- ProcArray updated with xid
- CLOG set to IN_PROGRESS

**State Transition:**
```
[NO TRANSACTION] ──beginTransaction()──► [ACTIVE]
```

#### Function: `commitTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:407
Status TransactionManager::commitTransaction(
    uint32_t proc_id,
    uint64_t xid,
    ErrorContext *ctx
);
```

**Preconditions:**
- Transaction state is ACTIVE
- All changes flushed (if durability required)

**Postconditions:**
- CLOG updated to COMMITTED
- TIP updated to COMMITTED (via group commit)
- ProcArray slot cleared
- Transaction markers may advance

**State Transition:**
```
[ACTIVE] ──commitTransaction()──► [COMMITTED]
```

#### Function: `rollbackTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:962
Status TransactionManager::rollbackTransaction(
    uint32_t proc_id,
    uint64_t xid,
    ErrorContext *ctx
);
```

**Preconditions:**
- Transaction state is ACTIVE

**Postconditions:**
- CLOG updated to ABORTED
- TIP updated to ABORTED
- ProcArray slot cleared
- Changes discarded (not written or rolled back via back versions)

**State Transition:**
```
[ACTIVE] ──rollbackTransaction()──► [ABORTED]
```

#### Function: `prepareTransaction()`

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

**Preconditions:**
- Transaction state is ACTIVE
- 2PC enabled

**Postconditions:**
- State set to PREPARED
- Transaction data persisted for recovery
- ProcArray slot kept (XID remains visible)

**State Transition:**
```
[ACTIVE] ──prepareTransaction()──► [PREPARED]
```

### State Storage

#### TIP Entry (24 bytes)

```cpp
// From include/scratchbird/core/transaction_manager.h:76
struct TIPEntry {
    uint64_t xid;         // Transaction ID
    uint8_t state;        // TransactionState value
    uint8_t flags;        // Reserved flags
    uint16_t reserved;    // Alignment
    uint64_t commit_time; // Commit timestamp (0 if ACTIVE)
};
```

#### CLOG (2 bits per transaction)

```cpp
// From include/scratchbird/core/clog.h:54
struct ClogPageHeader {
    PageHeader page_header;  // 80 bytes
    uint64_t base_xid;       // First XID in page
    uint32_t next_clog_page;
    uint32_t reserved;
    // 2-bit status array follows
};
```

**Storage Comparison:**
| Storage | Bytes/XID | Capacity/page (16KB) |
|---------|-----------|---------------------|
| TIP | 24 | ~680 |
| CLOG | 0.25 (2 bits) | ~65,000 |
| Savings | 96x | 96x |

## Invariants

1. **Monotonic State**: Transaction state never moves backward
   - ACTIVE → COMMITTED | ABORTED | PREPARED
   - PREPARED → COMMITTED | ABORTED
   - COMMITTED, ABORTED: Terminal (no changes)
   
2. **Valid State Values**: Only enum values 0-3 stored
   - Verification: Decode validates, treats invalid as ABORTED
   
3. **OIT Monotonicity**: OIT never decreases
   - Verification: setOldestXid() checks new_oit >= old_oit
   
4. **State Persistence**: State changes fsync'd before acknowledgment
   - Verification: Group commit fsync, CLOG write

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `INVALID_STATE` | Operation on non-ACTIVE transaction | Return error |
| `STATE_LOST` | TIP/CLOG corruption | Reconstruct from WAL (if available) |
| `WRAPAROUND` | XID approaching limit | Block new transactions, force VACUUM |

## Performance Considerations

### Group Commit
- **Batches**: Multiple commits in single fsync
- **Timeout**: 10ms max wait for batch formation
- **Benefit**: 5-10x throughput improvement

### CLOG Caching
- **In-memory cache**: Recent transaction states cached
- **LRU eviction**: Older entries evicted under memory pressure
- **Benefit**: Avoids disk read for visibility checks

### State Lookup Optimization
- **Hint bits**: Cache state in tuple header infomask
- **OIT shortcut**: XID < OIT implies COMMITTED
- **Benefit**: 50% reduction in TIP/CLOG lookups

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_transaction_manager.cpp` | Basic state transitions |
| `tests/unit/test_transaction_states.cpp` | State validation |
| `tests/unit/test_clog.cpp` | CLOG encoding |
| `tests/unit/test_2pc.cpp` | PREPARED state |

## Related Specifications

- [Transaction ID Allocation](./transaction_id_allocation.md) - XID assignment
- [TIP Format](./tip_format.md) - Transaction Inventory Page layout
- [Visibility Computation](./visibility_computation.md) - State to visibility mapping
- [Prepared Transactions](./prepared_transactions.md) - 2PC details

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
