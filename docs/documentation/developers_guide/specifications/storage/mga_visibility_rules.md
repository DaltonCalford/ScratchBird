# Specification: MGA Visibility Rules

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h:285`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:1700`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/heap_page.cpp:1206`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_transaction_manager.cpp:137`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_mga_debug.cpp`

## Synopsis

This specification defines the Firebird Multi-Generational Architecture (MGA) visibility rules used in ScratchBird. Unlike PostgreSQL's snapshot-based visibility, Firebird MGA uses TIP (Transaction Inventory Page) lookups to determine if a record version is visible to a transaction.

## Scope

### In Scope

- Firebird MGA visibility algorithm
- TIP-based visibility computation
- Hint bits optimization
- Version chain traversal visibility
- Special XID handling (FROZEN_XID, etc.)

### Out of Scope

- Snapshot isolation implementation (uses same MGA base)
- Lock-based isolation levels
- Serializable isolation (future work)

## Background

Firebird MGA visibility differs from PostgreSQL-style MVCC:

| Aspect | PostgreSQL | Firebird MGA |
|--------|------------|--------------|
| Visibility Check | Snapshot comparison | TIP state lookup |
| Snapshot Data | Active XID array | Not needed for basic visibility |
| Performance | O(log N) binary search | O(1) cache lookup |
| Memory | Snapshot per transaction | Shared TIP cache |

Key principle: A transaction can see a version only if:
1. The creating transaction is committed AND
2. The creating transaction is older than the reader AND
3. The deleting transaction (if any) is not committed OR is the reader itself

## Specification

### Data Structures

#### Tuple Header Visibility Fields

```cpp
// From include/scratchbird/core/heap_page.h:91
struct TupleHeader {
    uint64_t xmin;        // Creating transaction ID
    uint64_t xmax;        // Deleting/updating transaction ID (0 = not deleted)
    uint16_t infomask;    // Visibility hint bits
    
    // Hint bit flags
    static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0002;
    static constexpr uint16_t HEAP_XMIN_INVALID = 0x0004;
    static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0008;
    static constexpr uint16_t HEAP_XMAX_INVALID = 0x0010;
    static constexpr uint16_t HEAP_XMIN_FROZEN = 0x0100;
};
```

#### Special XID Constants

```cpp
// From include/scratchbird/core/transaction_manager.h:428
static constexpr uint64_t INVALID_XID = 0;     // Never valid in tuple headers
static constexpr uint64_t BOOTSTRAP_XID = 1;   // Bootstrap transaction
static constexpr uint64_t FROZEN_XID = 2;      // Frozen tuples (always visible)
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

**Preconditions:**
- `version_xid` is a valid XID (not INVALID_XID)
- `reader_xid` is valid and active

**Postconditions:**
- Returns true if version is visible to reader per MGA rules
- Returns false otherwise

**Algorithm:**
1. Own changes always visible: `version_xid == reader_xid → true`
2. Frozen tuples always visible: `version_xid <= FROZEN_XID → true`
3. Validate XID range
4. Look up `version_xid` state in TIP
5. Visible only if: `state == COMMITTED && version_xid < reader_xid`

#### Function: `findVisibleVersion()`

```cpp
// Source: src/core/heap_page.cpp:1206
Status HeapPage::findVisibleVersion(
    uint16_t item_id,           // Starting item ID (newest version)
    uint64_t current_xid,       // Reader's XID
    const uint8_t **data_out,   // Output: pointer to visible version
    uint32_t *size_out,         // Output: size of visible version
    ErrorContext *ctx
);
```

**Preconditions:**
- Page is pinned
- `item_id` is valid
- `current_xid` is active

**Postconditions:**
- Returns pointer to visible version data
- Follows version chain if needed
- May use cross-page buffer for off-page versions

### Algorithms

#### Algorithm: Firebird MGA Visibility Check

```
Input:  version_xid, reader_xid
Output: visible (boolean)

1. // Rule 1: Own changes always visible
2. IF version_xid == reader_xid:
3.     RETURN true
4.
5. // Rule 2: Frozen tuples always visible
6. IF version_xid <= FROZEN_XID:
7.     RETURN true
8.
9. // Rule 3: Validate XID is in valid range
10. IF NOT isXidInRange(version_xid):
11.    LOG warning (rate-limited)
12.    RETURN false
13.
14. // Rule 4: Look up transaction state in TIP
15. state = getTransactionState(version_xid)
16. IF lookup failed:
17.    LOG warning
18.    RETURN false
19.
20. // Rule 5: Only committed transactions older than reader are visible
21. IF state == COMMITTED AND version_xid < reader_xid:
22.    RETURN true
23.
24. // All other cases: not visible
25. RETURN false
```

#### Algorithm: Hint Bits Optimization

```
Input:  tuple_hdr, reader_xid
Output: visible (boolean), hint_bits_set (boolean)

// Fast path: Check hint bits first
1. IF tuple_hdr.infomask & HEAP_XMIN_COMMITTED:
2.     // xmin definitely committed
3.     IF tuple_hdr.infomask & HEAP_XMAX_INVALID:
4.         // Not deleted - definitely visible
5.         RETURN true, true
6.     ELSE IF tuple_hdr.xmax != 0 AND 
7.             (tuple_hdr.infomask & HEAP_XMAX_COMMITTED):
8.         // Deleted by committed transaction
9.         IF tuple_hdr.xmax <= reader_xid:
10.            RETURN false, true
11.        ELSE:
12.            RETURN true, true
13.    ELSE IF tuple_hdr.xmax == 0:
14.        // Not deleted - visible
15.        RETURN true, true
16.
17. ELSE IF tuple_hdr.infomask & HEAP_XMIN_INVALID:
18.    // xmin definitely invalid - not visible
19.    RETURN false, true
20.
21. // Slow path: Need TIP lookup
22. visible = isVersionVisible(tuple_hdr.xmin, reader_xid)
23.
24. // Set hint bits for future calls
25. IF db_ != nullptr AND db_->transaction_manager() != nullptr:
26.    txn_mgr = db_->transaction_manager()
27.    
28.    // Set xmin hint bits
29.    IF tuple_hdr.xmin <= reader_xid:
30.        xmin_state = txn_mgr->getTransactionState(tuple_hdr.xmin)
31.        IF xmin_state == COMMITTED:
32.            tuple_hdr.infomask |= HEAP_XMIN_COMMITTED
33.        ELSE IF xmin_state == ABORTED:
34.            tuple_hdr.infomask |= HEAP_XMIN_INVALID
35.
36.    // Set xmax hint bits
37.    IF tuple_hdr.xmax != 0 AND tuple_hdr.xmax <= reader_xid:
38.        xmax_state = txn_mgr->getTransactionState(tuple_hdr.xmax)
39.        IF xmax_state == COMMITTED:
40.            tuple_hdr.infomask |= HEAP_XMAX_COMMITTED
41.        ELSE IF xmax_state == ABORTED:
42.            tuple_hdr.infomask |= HEAP_XMAX_INVALID
43.
44. RETURN visible, false
```

#### Algorithm: Version Chain Visibility Traversal

```
Input:  item_id, reader_xid
Output: visible_version_data, visible_version_size

1. current_item_id = item_id
2. is_back_version = false
3. chain_length = 0
4. visited = empty_set()
5.
6. WHILE chain_length < MAX_CHAIN_LENGTH:
7.     // Cycle detection
8.     location_key = MAKE_KEY(page_id, current_item_id, is_back_version)
9.     IF location_key IN visited:
10.        RETURN PAGE_CORRUPT (cycle detected)
11.    ADD location_key TO visited
12.
13.    // Get tuple header
14.    IF is_back_version:
15.        tuple_hdr = page_data + current_offset
16.    ELSE:
17.        tuple_hdr = page_data + items[current_item_id].offset
18.
19.    // Validate XIDs
20.    IF NOT isValidXid(tuple_hdr.xmin):
21.        IF tuple_hdr.hasBackVersion():
22.            FOLLOW_BACK_VERSION()
23.            CONTINUE
24.        ELSE:
25.            RETURN PAGE_CORRUPT
26.
27.    effective_xmax = isValidXid(tuple_hdr.xmax) ? tuple_hdr.xmax : 0
28.
29.    // Check visibility using hint bits + MGA rules
30.    IF isVisible(tuple_hdr, reader_xid):
31.        RETURN tuple_data, tuple_size
32.
33.    // Not visible - follow back version
34.    IF NOT tuple_hdr.hasBackVersion():
35.        RETURN NOT_FOUND
36.
37.    back_tid = tuple_hdr.getBackVersionTID()
38.    IF back_tid.page_id != current_page_id:
39.        SWITCH_TO_PAGE(back_tid.page_id)
40.    current_offset = back_tid.slot
41.    is_back_version = true
42.    chain_length++
43.
44. RETURN PAGE_CORRUPT (chain too long)
```

### Decision Trees

```
Is tuple visible to reader XID?
│
├── xmin == reader_xid ────────────────────────────► YES (own changes)
│
├── xmin <= FROZEN_XID ────────────────────────────► YES (frozen)
│
├── NOT isValidXid(xmin) ──────────────────────────► NO (corrupted)
│
├── xmin > reader_xid ─────────────────────────────► NO (future txn)
│
└── xmin < reader_xid
    │
    ├── getTransactionState(xmin) == ABORTED ──────► NO (aborted)
    │
    ├── getTransactionState(xmin) == ACTIVE ───────► NO (still running)
    │
    └── getTransactionState(xmin) == COMMITTED
        │
        ├── xmax == 0 ─────────────────────────────► YES (not deleted)
        │
        ├── xmax == reader_xid ────────────────────► YES (deleted by self)
        │
        └── xmax != reader_xid
            │
            ├── getTransactionState(xmax) == ABORTED ─► YES (delete aborted)
            │
            ├── getTransactionState(xmax) == ACTIVE ───► YES (delete in progress)
            │
            └── getTransactionState(xmax) == COMMITTED
                │
                ├── xmax > reader_xid ─────────────► YES (deleted after start)
                │
                └── xmax < reader_xid ─────────────► NO (deleted before start)
```

## Invariants

1. **Own Visibility**: A transaction always sees its own uncommitted changes
   - Verification: `xmin == reader_xid` returns true before any state checks
   
2. **Frozen Visibility**: Frozen tuples (xmin <= FROZEN_XID) are always visible
   - Verification: Short-circuit return before TIP lookup
   
3. **Committed Only**: Only committed transactions' changes are visible
   - Verification: `state == COMMITTED` required for visibility
   
4. **Time Ordering**: Only transactions older than reader are visible
   - Verification: `version_xid < reader_xid` required
   
5. **Hint Bit Consistency**: Hint bits must accurately reflect transaction state
   - Verification: Set only after TIP lookup confirms state

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_CORRUPT` | Invalid XID in tuple header | Log warning, skip to back version |
| `PAGE_CORRUPT` | Cycle in version chain | Abort traversal, return error |
| `NOT_FOUND` | No visible version in chain | Return "row not found" |
| `OOM` | Out of memory in cycle detection | Abort traversal for safety |

## Performance Considerations

### Hint Bits Target
- **Goal**: 50% reduction in TIP lookups
- **Mechanism**: Cache transaction state in tuple header infomask
- **Persistence**: Opportunistic (not required for correctness)

### TIP Cache
- In-memory LRU cache of recent transaction states
- Default size: 10,000 entries
- Reduces disk reads for frequently-checked transactions

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_transaction_manager.cpp:137` | Basic visibility checks |
| `tests/unit/test_mga_debug.cpp` | MGA-specific debugging |
| `tests/unit/test_hint_bits.cpp` | Hint bit optimization |
| `tests/unit/test_heap_page.cpp` | Version chain visibility |
| `tests/unit/test_garbage_collector.cpp` | GC visibility horizon |

## Migration Notes

N/A - Initial MGA visibility specification for ScratchBird Alpha.

## Related Specifications

- [Version Chain Format](./version_chain_format.md) - Physical version chain structure
- [Transaction Lifecycle](./transaction_lifecycle.md) - Transaction states
- [GC Sweep Algorithm](./gc_sweep_algorithm.md) - When versions become reclaimable

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| MGA | Multi-Generational Architecture - MVCC using TIP-based visibility |
| TIP | Transaction Inventory Page - stores transaction commit states |
| Hint Bits | Cached transaction state stored in tuple header |
| Frozen XID | Special XID (2) indicating tuple survived VACUUM |
| xmin | Transaction ID that created a tuple version |
| xmax | Transaction ID that deleted/updated a tuple |

### References

- Firebird MGA documentation
- `MGA_RULES.md` - Internal MGA implementation rules

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
