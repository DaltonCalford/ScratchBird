# Specification: Transaction ID Allocation

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h:384`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/config.h`

## Synopsis

This specification defines Transaction ID (XID) allocation, the monotonic counter that uniquely identifies transactions. XIDs are 64-bit values that increase indefinitely (no wraparound in practice), with special reserved values for system operations.

## Scope

### In Scope

- XID allocation algorithm
- Special XID values
- XID wraparound prevention
- XID validation
- 64-bit XID space management

### Out of Scope

- 32-bit XID wraparound (not applicable with 64-bit)
- XID epoch mechanisms
- Distributed transaction XID coordination

## Background

Unlike PostgreSQL's 32-bit XIDs that wrap around, ScratchBird uses 64-bit XIDs:
- **No wraparound**: 2^64 XIDs exceeds practical database lifetime
- **No epoch tracking**: Simpler implementation
- **Freeze still needed**: For hint bit optimization

## Specification

### Data Structures

#### XID Constants

```cpp
// From include/scratchbird/core/transaction_manager.h:428
static constexpr uint64_t INVALID_XID = 0;      // Never valid
static constexpr uint64_t BOOTSTRAP_XID = 1;    // Bootstrap transaction
static constexpr uint64_t FROZEN_XID = 2;       // Frozen tuples

// From include/scratchbird/core/config.h
static constexpr uint64_t DEFAULT_INITIAL_XID = 3;  // First user XID
```

#### XID Allocation State

```cpp
// From include/scratchbird/core/transaction_manager.h:384
class TransactionManager {
    std::atomic<uint64_t> next_xid_{config::DEFAULT_INITIAL_XID};
    uint64_t oldest_xid_ = FROZEN_XID + 1;
    
    // Wraparound protection (mostly theoretical with 64-bit)
    static constexpr uint64_t XID_WRAPAROUND_THRESHOLD = 1000000;
    static constexpr uint64_t MAX_SAFE_XID = UINT64_MAX - XID_WRAPAROUND_THRESHOLD;
};
```

### Special XID Values

| XID | Name | Meaning |
|-----|------|---------|
| 0 | INVALID_XID | Invalid/uninitialized |
| 1 | BOOTSTRAP_XID | Catalog creation, initdb |
| 2 | FROZEN_XID | Tuple frozen (always visible) |
| 3+ | User XIDs | Regular transactions |

### XID Allocation Algorithm

```cpp
// Source: src/core/transaction_manager.cpp
Status TransactionManager::beginTransaction(
    uint32_t proc_id, 
    uint64_t &xid_out,
    ErrorContext *ctx
) {
    // 1. Allocate XID atomically
    uint64_t new_xid = next_xid_.fetch_add(1, std::memory_order_acq_rel);
    
    // 2. Check for theoretical wraparound
    if (new_xid > MAX_SAFE_XID) {
        // This should never happen in practice
        return Status::PAGE_CORRUPT;  // Critical error
    }
    
    // 3. Register in ProcArray
    Status status = ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);
    if (status != Status::OK) {
        return status;
    }
    
    // 4. Initialize CLOG state
    clog_->setStatus(new_xid, ClogStatus::IN_PROGRESS, ctx);
    
    xid_out = new_xid;
    stats_.transactions_started++;
    
    return Status::OK;
}
```

### XID Validation

#### Function: `isValidXid()`

```cpp
// From include/scratchbird/core/transaction_manager.h:209
static bool isValidXid(uint64_t xid) {
    return xid != INVALID_XID;
}
```

#### Function: `isXidInRange()`

```cpp
// Source: src/core/transaction_manager.cpp
bool TransactionManager::isXidInRange(uint64_t xid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // XID must be >= oldest_xid_ (OIT) and < next_xid_
    uint64_t next = next_xid_.load(std::memory_order_acquire);
    
    if (xid < oldest_xid_ || xid >= next) {
        return false;
    }
    
    return true;
}
```

### XID Range Checking

```
Valid XID Range:
├─ oldest_xid_ (OIT)
│  ├─ XIDs in this range are "interesting"
│  │  (may have uncommitted changes affecting visibility)
│  │
│  ├─ XIDs between OIT and next_xid are valid
│  │
│  └─ XIDs >= next_xid are "future" transactions
│
└─ next_xid (NEXT)
   └─ Next XID to be allocated

XID < OIT: Either frozen (<= 2) or committed long ago
OIT <= XID < NEXT: Valid transaction range
XID >= NEXT: Future transaction (not yet started)
```

### Wraparound Prevention (Theoretical)

With 64-bit XIDs, wraparound is practically impossible:
- **Allocation rate**: 1 million XIDs/second
- **Time to wrap**: 2^64 / 1,000,000 / 86400 / 365 = 584 million years

However, the code still includes checks:

```cpp
// Source: src/core/transaction_manager.cpp
Status TransactionManager::checkXIDWraparound(ErrorContext *ctx) {
    uint64_t next = next_xid_.load(std::memory_order_acquire);
    
    if (next > MAX_SAFE_XID) {
        // Critical: Approaching 2^64
        // Block new transactions until emergency measures
        return Status::PAGE_CORRUPT;
    }
    
    if (next > MAX_SAFE_XID - XID_WRAPAROUND_THRESHOLD) {
        // Warning: Getting close (purely theoretical)
        Logger::warn("XID space exhaustion imminent");
    }
    
    return Status::OK;
}
```

## Invariants

1. **Monotonic Allocation**: `next_xid_` only increases
   - Verification: Atomic fetch_add guarantees monotonicity
   
2. **Unique XIDs**: Each allocation returns distinct value
   - Verification: Atomic increment ensures uniqueness
   
3. **Valid Range**: `oldest_xid_ <= xid < next_xid_` for all valid XIDs
   - Verification: isXidInRange() checks
   
4. **Special XID Constants**: 0, 1, 2 never allocated to user transactions
   - Verification: DEFAULT_INITIAL_XID starts at 3

## Algorithms

### Algorithm: XID to String (for debugging)

```
xidToString(xid):
    SWITCH xid:
        CASE 0: RETURN "INVALID"
        CASE 1: RETURN "BOOTSTRAP"
        CASE 2: RETURN "FROZEN"
        DEFAULT: RETURN to_string(xid)
```

### Algorithm: XID Comparison

```
compareXids(xid1, xid2):
    // Simple numeric comparison with 64-bit
    // No wraparound logic needed!
    
    IF xid1 < xid2: RETURN -1
    IF xid1 > xid2: RETURN 1
    RETURN 0
```

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_CORRUPT` | XID >= MAX_SAFE_XID | Block all transactions, alert admin |
| `INVALID_ARGUMENT` | XID = 0 passed to operation | Return error |
| `OUT_OF_RANGE` | XID outside valid range | Return error, suggest corruption |

## Performance Considerations

### Atomic Allocation
- **fetch_add**: Single atomic operation
- **Lock-free**: No mutex for allocation
- **Scalability**: No contention between allocators

### Cache Line Optimization
- **next_xid_**: Separate cache line (atomic)
- **No false sharing**: Each backend updates own ProcArray entry

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_xid_allocation.cpp` | Allocation sequence |
| `tests/unit/test_xid_validation.cpp` | Validation checks |
| `tests/unit/test_xid_64bit.cpp` | 64-bit overflow edge cases |

## Comparison: 32-bit vs 64-bit XIDs

| Aspect | PostgreSQL (32-bit) | ScratchBird (64-bit) |
|--------|---------------------|----------------------|
| Wraparound | Every 4 billion XIDs | Never (practically) |
| Epoch tracking | Required | Not needed |
| Freeze urgency | High | Low |
| Code complexity | Higher | Lower |
| Storage | 4 bytes | 8 bytes |

## Related Specifications

- [Transaction States](./transaction_states.md) - State machine
- [TIP Format](./tip_format.md) - XID storage
- [CLOG](./clog.md) - XID status storage
- [Freeze Operations](./vacuum.md) - XID freezing

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
