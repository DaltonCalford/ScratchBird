# Specification: Visibility Computation

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h:301`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/heap_page.cpp:1206`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_mga_visibility.cpp`

## Synopsis

This specification defines the Firebird MGA (Multi-Generational Architecture) visibility computation rules used in ScratchBird. Unlike PostgreSQL's snapshot-based visibility, Firebird MGA uses TIP state lookups to determine tuple visibility.

## Scope

### In Scope

- Firebird MGA visibility rules
- Hint bit optimization
- Version chain traversal visibility
- Snapshot isolation visibility
- Comparison with PostgreSQL MVCC

### Out of Scope

- Serializable isolation (SSI) - see Predicate Locks
- Lock-based visibility (FOR UPDATE)
- Distributed transaction visibility

## Background

| Aspect | PostgreSQL | Firebird MGA |
|--------|------------|--------------|
| Visibility Check | Snapshot comparison | TIP state lookup |
| Snapshot Data | Active XID array | Not needed for basic visibility |
| Performance | O(log N) binary search | O(1) cache lookup |
| Memory | Snapshot per transaction | Shared TIP cache |

**Core Principle**: A transaction can see a version only if:
1. The creating transaction is committed AND
2. The creating transaction is older than the reader AND
3. The deleting transaction (if any) is not committed OR is the reader itself

## Specification

### Data Structures

#### TupleHeader Visibility Fields

```cpp
// From include/scratchbird/core/heap_page.h:91
struct TupleHeader {
    uint64_t xmin;        // Creating transaction
    uint64_t xmax;        // Deleting transaction (0 = not deleted)
    uint16_t infomask;    // Visibility hint bits
};

// Hint bit flags
static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0002;
static constexpr uint16_t HEAP_XMIN_INVALID = 0x0004;
static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0008;
static constexpr uint16_t HEAP_XMAX_INVALID = 0x0010;
static constexpr uint16_t HEAP_XMIN_FROZEN = 0x0100;
```

#### TransactionSnapshot (for Snapshot Isolation)

```cpp
// From include/scratchbird/core/transaction_manager.h:55
struct TransactionSnapshot {
    uint64_t snapshot_txid_high = 0;           // next_xid at snapshot time
    std::vector<uint64_t> active_txid_set;     // Sorted active XIDs
    uint64_t snapshot_commit_seqno_high = 0;   // For future use
};
```

### Visibility Rules

#### Rule 1: Own Changes Always Visible

```cpp
if (version_xmin == reader_xid) {
    return true;  // My insert/update
}
```

#### Rule 2: Frozen Tuples Always Visible

```cpp
if (version_xmin <= FROZEN_XID) {
    return true;  // Survived VACUUM freeze
}
```

#### Rule 3: Validate XID Range

```cpp
if (!isXidInRange(version_xmin)) {
    return false;  // Invalid/corrupted XID
}
```

#### Rule 4: Check Creating Transaction State

```cpp
state = getTransactionState(version_xmin);
if (state != COMMITTED) {
    return false;  // Not committed, not visible
}
```

#### Rule 5: Time Ordering

```cpp
if (version_xmin >= reader_xid) {
    return false;  // Created after reader started
}
```

#### Rule 6: Check Deleting Transaction

```cpp
if (version_xmax == 0) {
    return true;  // Not deleted
}

if (version_xmax == reader_xid) {
    return true;  // Deleted by me, but I can still see it
}

xmax_state = getTransactionState(version_xmax);
if (xmax_state != COMMITTED) {
    return true;  // Delete not committed
}

if (version_xmax >= reader_xid) {
    return true;  // Deleted after I started
}

return false;  // Deleted by committed transaction before I started
```

### Interface Contracts

#### Function: `isVersionVisible()` (Firebird MGA Core)

```cpp
// Source: src/core/transaction_manager.cpp:1700
bool TransactionManager::isVersionVisible(
    uint64_t version_xid,   // XID that created the version
    uint64_t reader_xid     // XID of the reading transaction
);
```

**Algorithm:**
```
1. // Rule 1: Own changes
2. IF version_xid == reader_xid: RETURN true

3. // Rule 2: Frozen tuples
4. IF version_xid <= FROZEN_XID: RETURN true

5. // Rule 3: Valid XID
6. IF NOT isValidXid(version_xid): 
7.     LOG warning
8.     RETURN false

9. IF NOT isXidInRange(version_xid):
10.    LOG warning  
11.    RETURN false

12. // Rule 4: Committed?
13. state = getTransactionState(version_xid)
14. IF state != COMMITTED: RETURN false

15. // Rule 5: Time ordering
16. IF version_xid >= reader_xid: RETURN false

17. // Visible!
18. RETURN true
```

#### Function: `isTupleVisible()` (Full Tuple Check)

```cpp
// Source: src/core/heap_page.cpp
bool isTupleVisible(
    const TupleHeader &header,
    uint64_t reader_xid,
    TransactionManager *tm
);
```

**Algorithm:**
```
1. // Fast path: Check hint bits first
2. IF header.infomask & HEAP_XMIN_COMMITTED:
3.     // xmin is committed, now check xmax
4.     IF header.infomask & HEAP_XMAX_INVALID:
5.         RETURN true  // Not deleted
6.     IF header.xmax == 0:
7.         RETURN true
8.     IF header.xmax == reader_xid:
9.         RETURN true  // Deleted by self
10.    IF header.infomask & HEAP_XMAX_COMMITTED:
11.        IF header.xmax >= reader_xid:
12.            RETURN true  // Deleted after start
13.        ELSE:
14.            RETURN false  // Deleted before start
15.    IF header.infomask & HEAP_XMAX_INVALID:
16.        RETURN true  // Delete aborted
17.    // Need xmax lookup
18.    xmax_state = tm->getTransactionState(header.xmax)
19.    RETURN (xmax_state != COMMITTED) || (header.xmax >= reader_xid)

20. IF header.infomask & HEAP_XMIN_INVALID:
21.     RETURN false  // xmin aborted

22. IF header.infomask & HEAP_XMIN_FROZEN:
23.     RETURN true  // Frozen tuple

24. // Slow path: Need xmin lookup
25. IF NOT tm->isVersionVisible(header.xmin, reader_xid):
26.     // Set hint bit for future
27.     IF header.xmin < reader_xid:
28.         xmin_state = tm->getTransactionState(header.xmin)
29.         IF xmin_state == ABORTED:
30.             header.infomask |= HEAP_XMIN_INVALID
31.     RETURN false

32. // xmin is visible, now check xmax
33. IF header.xmax == 0:
34.     RETURN true

35. // Similar xmax logic as above...
```

### Hint Bit Optimization

```
Hint Bit Setting:

After TIP lookup confirms state:
├─ IF xid <= reader_xid:
│   ├─ state == COMMITTED: Set HEAP_X*_COMMITTED
│   ├─ state == ABORTED: Set HEAP_X*_INVALID
│   └─ state == ACTIVE: Don't set (may change)
│
└─ IF xid > reader_xid:
    └─ Don't set (future transaction, reader can't cache)

Benefit: Next visibility check avoids TIP lookup
```

### Snapshot Isolation Visibility

For SNAPSHOT isolation, use snapshot instead of current XID:

```cpp
bool isVisibleInSnapshot(
    uint64_t create_xid,
    uint64_t delete_xid,
    const TransactionSnapshot &snapshot
) {
    // Creation must be committed and not active in snapshot
    if (create_xid >= snapshot.snapshot_txid_high) {
        return false;  // Created after snapshot
    }
    
    if (snapshotHasActiveXid(snapshot, create_xid)) {
        return false;  // Creator was active in snapshot
    }
    
    // Deletion check
    if (delete_xid == 0) {
        return true;  // Not deleted
    }
    
    if (delete_xid >= snapshot.snapshot_txid_high) {
        return true;  // Deleted after snapshot
    }
    
    if (snapshotHasActiveXid(snapshot, delete_xid)) {
        return true;  // Deleter was active
    }
    
    return false;  // Deleted by committed transaction in snapshot
}
```

## Decision Trees

```
Is tuple visible to reader XID?
│
├── xmin == reader_xid ────────────────────────────► YES (own changes)
│
├── xmin <= FROZEN_XID ────────────────────────────► YES (frozen)
│
├── NOT isValidXid(xmin) ──────────────────────────► NO (corrupted)
│
├── getTransactionState(xmin) == ABORTED ──────────► NO (aborted insert)
│
├── getTransactionState(xmin) == ACTIVE ───────────► NO (still running)
│
└── getTransactionState(xmin) == COMMITTED
    │
    ├── xmin >= reader_xid ────────────────────────► NO (future)
    │
    └── xmin < reader_xid
        │
        ├── xmax == 0 ─────────────────────────────► YES (not deleted)
        │
        ├── xmax == reader_xid ────────────────────► YES (deleted by self)
        │
        └── xmax != reader_xid
            │
            ├── getTransactionState(xmax) != COMMITTED ─► YES
            │
            └── getTransactionState(xmax) == COMMITTED
                │
                ├── xmax >= reader_xid ────────────► YES
                │
                └── xmax < reader_xid ─────────────► NO
```

## Invariants

1. **Own Visibility**: Transaction always sees own uncommitted changes
   - Verification: `xmin == reader_xid` check first
   
2. **Committed Only**: Only committed transactions' changes visible
   - Verification: `state == COMMITTED` required
   
3. **Time Ordering**: Only older transactions visible
   - Verification: `version_xid < reader_xid` required
   
4. **Hint Bit Consistency**: Hint bits never contradict actual state
   - Verification: Only set after TIP lookup confirms

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_VISIBLE` | Normal visibility result | Caller handles (skip tuple) |
| `PAGE_CORRUPT` | Invalid XID in tuple | Log error, skip to back version |

## Performance Considerations

### Hint Bit Hit Rate
- **Target**: 50% of checks avoid TIP lookup
- **Mechanism**: Cache committed/aborted states in tuple header
- **Benefit**: Eliminates cache miss for frequently-checked XIDs

### TIP Cache
- **Size**: 10,000 entries default
- **Policy**: LRU eviction
- **Hit rate**: >95% for active transactions

### Comparison with PostgreSQL

| Metric | PostgreSQL | Firebird MGA |
|--------|------------|--------------|
| Visibility check | O(log N) | O(1) |
| Snapshot memory | O(active txns) | O(0) for basic, O(active) for snapshot |
| Cache pressure | Higher | Lower |
| Subtransaction handling | Complex (subxip) | Simpler |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_mga_visibility.cpp` | Basic visibility |
| `tests/unit/test_visibility_rules.cpp` | Edge cases |
| `tests/unit/test_hint_bits.cpp` | Optimization |
| `tests/unit/test_snapshot_isolation.cpp` | Snapshot visibility |

## Related Specifications

- [MGA Visibility Rules](./mga_visibility_rules.md) - Original specification
- [Transaction States](./transaction_states.md) - State definitions
- [Version Chain Format](./version_chain_format.md) - Chain traversal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
