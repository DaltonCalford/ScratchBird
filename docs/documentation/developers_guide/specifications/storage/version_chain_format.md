# Specification: Version Chain Format

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/heap_page.h:91`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/heap_page.cpp:750`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_mga_back_versioning.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_version_chain_cycle.cpp`

## Synopsis

This specification defines the Firebird-style Multi-Generational Architecture (MGA) version chain format used in ScratchBird. Version chains enable MVCC by maintaining multiple versions of a record, with each update creating a back version that preserves the previous state while the primary location holds the newest version.

## Scope

### In Scope

- Tuple header structure and version chain fields
- Back version pointer format (GPID-based TID)
- Version chain traversal algorithm
- Same-page vs cross-page back version storage
- Version chain invariants and validation

### Out of Scope

- Transaction visibility computation (see `mga_visibility_rules.md`)
- Garbage collection of version chains (see `gc_sweep_algorithm.md`)
- TOAST handling for large tuples (see TOAST specifications)

## Background

ScratchBird implements Firebird MGA semantics where:

1. **Stable TIDs**: Item pointers never change during updates (indexes remain valid)
2. **Back Versioning**: Updates create back versions, new data overwrites primary location
3. **Newest-to-Oldest Chains**: Version chains point backward from newest to oldest
4. **In-Place Updates**: Primary location is overwritten in-place with new tuple data

This provides 80% reduction in write amplification compared to PostgreSQL-style MVCC.

## Specification

### Data Structures

#### Tuple Header (Version Chain Fields)

```cpp
// From include/scratchbird/core/heap_page.h:91
#pragma pack(push, 1)
struct TupleHeader {
    // Transaction info (16 bytes)
    uint64_t xmin; // Transaction ID that inserted this tuple
    uint64_t xmax; // Transaction ID that deleted/updated this tuple (or 0)

    // Version chain (12 bytes) - Firebird MGA back versioning
    uint64_t back_version_gpid;  // GPID of BACK version (INVALID_GPID if original)
    uint16_t back_version_slot;  // Slot of BACK version
    uint16_t reserved1;          // Alignment padding

    // Tuple metadata (12 bytes)
    GPID ctid_gpid;      // Current tuple ID: GPID (tablespace + page number)
    uint16_t ctid_slot;  // Current tuple ID: slot number
    uint16_t infomask;   // Tuple state flags

    // ... additional fields (48 bytes total)
};
#pragma pack(pop)
```

**Binary Layout (80 bytes total):**

| Offset | Field | Size | Description |
|--------|-------|------|-------------|
| 0x00 | xmin | 8 bytes | Creating transaction ID |
| 0x08 | xmax | 8 bytes | Deleting/updating transaction ID (0 = not deleted) |
| 0x10 | back_version_gpid | 8 bytes | GPID pointing to back version |
| 0x18 | back_version_slot | 2 bytes | Slot number of back version |
| 0x1A | reserved1 | 2 bytes | Padding |
| 0x1C | ctid_gpid | 8 bytes | GPID of this tuple's location |
| 0x24 | ctid_slot | 2 bytes | Slot number of this tuple |
| 0x26 | infomask | 2 bytes | State flags |
| 0x28 | null_bitmap_offset | 2 bytes | Offset to null bitmap |
| 0x2A | padding | 2 bytes | Padding |
| 0x2C | session_id | 16 bytes | Session scope for temp tables |
| 0x3C | row_uuid | 16 bytes | Stable row UUID across versions |
| 0x4C | record_flags | 4 bytes | RHD_* flags |
| 0x50 | record_format | 4 bytes | Format version |
| 0x54 | payload_len | 4 bytes | Data length after header |

#### Item Pointer (Line Pointer)

```cpp
// From include/scratchbird/core/heap_page.h:37
#pragma pack(push, 1)
struct ItemPointer {
    uint32_t offset;      // Offset from start of page
    uint32_t length : 31; // Length of tuple
    uint32_t flags : 1;   // 0 = normal, 1 = deleted/unused
};
#pragma pack(pop)
```

### Interface Contracts

#### Function: `HeapPage::updateTuple()`

```cpp
// Source: src/core/heap_page.cpp:750
Status HeapPage::updateTuple(
    uint16_t old_item_id,      // Item ID of tuple to update
    const uint8_t *new_tuple_data,  // New tuple data
    uint32_t new_tuple_size,   // Size of new tuple
    uint64_t xmax,             // XID performing update (sets xmax on old version)
    uint64_t new_xmin,         // XID for new version
    uint16_t *new_item_id_out, // Returns same item_id (stable pointer)
    ErrorContext *ctx
);
```

**Preconditions:**
- Page is pinned and writable
- `old_item_id` is valid and not deleted
- Sufficient space exists for back version + new tuple

**Postconditions:**
- Old tuple data copied to back version location
- Old tuple header updated with `xmax` and `HEAP_UPDATED` flag
- Primary location overwritten with new tuple data
- New tuple header contains `xmin=new_xmin`, `xmax=0`
- Back version pointer set to point to old tuple location
- `*new_item_id_out` equals `old_item_id` (stable TID)

**Thread Safety:**
- Caller must hold page-level lock
- Not thread-safe without external locking

### Algorithms

#### Algorithm: Firebird MGA Update

```
Input:  old_item_id, new_tuple_data, new_tuple_size, xmax, new_xmin
Output: new_item_id (same as old_item_id - stable)

1. Validate old_item_id exists and is not deleted
2. Calculate space needed = new_tuple_size + old_tuple_size
3. IF NOT hasFreeSpace(space_needed):
       RETURN PAGE_FULL (cross-page handling by StorageEngine)
4. 
5. // PHASE 1: Create back version (preserve old state)
6. back_version_offset = pageUpper - old_tuple_size
7. back_version_offset = ALIGN_8(back_version_offset)
8. COPY old_tuple_data TO page_data[back_version_offset]
9. 
10. // Update back version header
11. back_version_hdr.xmax = xmax
12. back_version_hdr.infomask |= HEAP_CHAIN | HEAP_UPDATED
13. pageUpper = back_version_offset
14.
15. // PHASE 2: Overwrite primary location in-place
16. IF new_tuple_size <= old_tuple_size:
17.     COPY new_tuple_data TO old_tuple_offset
18.     item_ptr[old_item_id].length = new_tuple_size
19. ELSE:
20.     new_offset = pageUpper - new_tuple_size
21.     new_offset = ALIGN_8(new_offset)
22.     COPY new_tuple_data TO page_data[new_offset]
23.     item_ptr[old_item_id].offset = new_offset
24.     item_ptr[old_item_id].length = new_tuple_size
25.     pageUpper = new_offset
26.
27. // PHASE 3: Set up new tuple header
28. new_hdr.xmin = new_xmin
29. new_hdr.xmax = 0
30. new_hdr.back_version_gpid = MAKE_GPID(page_id)
31. new_hdr.back_version_slot = back_version_offset (Alpha: uses offset as slot)
32. new_hdr.ctid_gpid = MAKE_GPID(page_id)
33. new_hdr.ctid_slot = old_item_id
34. new_hdr.infomask = 0 (cleared for new version)
35.
36. RETURN old_item_id
```

#### Algorithm: Version Chain Traversal

```
Input:  item_id, reader_xid
Output: visible_version_data, visible_version_size

1. current_page_data = page_data
2. current_item_id = item_id
3. is_back_version = false
4. chain_length = 0
5. visited_locations = empty_set
6.
7. WHILE chain_length < MAX_CHAIN_LENGTH:
8.     // Cycle detection
9.     location_key = MAKE_KEY(current_page_id, current_item_id/is_back_version)
10.    IF location_key IN visited_locations:
11.        RETURN PAGE_CORRUPT (cycle detected)
12.    ADD location_key TO visited_locations
13.
14.    // Get tuple header based on access type
15.    IF is_back_version:
16.        tuple_hdr = page_data + current_offset
17.    ELSE:
18.        item_ptr = getItemArray()[current_item_id]
19.        tuple_hdr = page_data + item_ptr.offset
20.
21.    // Check visibility using Firebird MGA rules
22.    IF isVersionVisible(tuple_hdr, reader_xid):
23.        RETURN tuple_data, tuple_size
24.
25.    // Not visible - follow back version chain
26.    IF tuple_hdr.hasBackVersion():
27.        back_tid = tuple_hdr.getBackVersionTID()
28.        IF back_tid.page_id != current_page_id:
29.            SWITCH_TO_PAGE(back_tid.page_id)
30.        current_offset = back_tid.slot
31.        is_back_version = true
32.        chain_length++
33.    ELSE:
34.        RETURN NOT_FOUND (no visible version)
35.
36. RETURN PAGE_CORRUPT (chain too long)
```

**Complexity:**
- Time: O(chain_length) - typically 1-3 hops
- Space: O(1) - fixed-size visited set

### State Machines

```
┌─────────────┐    UPDATE    ┌─────────────┐
│   PRIMARY   │─────────────►│   PRIMARY   │
│  (version   │              │  (version   │
│     N)      │              │    N+1)     │
└─────────────┘              └──────┬──────┘
       ▲                            │
       │         back_version       │
       └────────────────────────────┘
              (points to N)
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| Primary (version N) | INSERT | Create initial version | Primary (version N) |
| Primary (version N) | UPDATE | Copy to back version, overwrite primary | Primary (version N+1) with back pointer |
| Primary (version N) | DELETE | Set xmax, mark deleted | Deleted with back pointer |
| Back version | GC PRUNE | Remove if all transactions can see newer | Removed |

### Decision Trees

```
Is version visible to reader?
├── xmin == reader_xid → YES (own changes)
├── xmin <= FROZEN_XID → YES (frozen tuple)
├── xmin > reader_xid → NO (future transaction)
└── xmin < reader_xid
    ├── TransactionState(xmin) != COMMITTED → NO
    └── TransactionState(xmin) == COMMITTED
        ├── xmax == 0 → YES (not deleted)
        ├── xmax == reader_xid → YES (deleted by self, still visible)
        └── xmax != reader_xid
            ├── TransactionState(xmax) != COMMITTED → YES (delete not committed)
            └── TransactionState(xmax) == COMMITTED → NO (deleted by other)
```

## Invariants

1. **Stable Item Pointer**: Item pointer location never changes during updates
   - Verification: `new_item_id_out == old_item_id` after update
   
2. **Back Version Validity**: Back version pointers must point to valid tuple data
   - Verification: Offset must be within page bounds, tuple header must be valid
   
3. **Cycle Freedom**: Version chains must not contain cycles
   - Verification: Track visited locations during traversal
   
4. **Chain Termination**: All version chains must terminate (no infinite loops)
   - Verification: Enforce `MAX_CHAIN_LENGTH` limit (default: 1000)

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_FULL` | Not enough space for back version | StorageEngine handles cross-page allocation |
| `PAGE_CORRUPT` | Cycle detected in version chain | Abort query, log corruption error |
| `NOT_FOUND` | No visible version in chain | Return "row not found" to caller |
| `INVALID_ARGUMENT` | Invalid item_id or null data | Return error to caller |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_mga_back_versioning.cpp` | Back version creation and traversal |
| `tests/unit/test_version_chain_cycle.cpp` | Cycle detection in version chains |
| `tests/unit/test_heap_page.cpp` | Basic tuple operations and version chain integrity |
| `tests/unit/test_storage_engine_mga_crosspage.cpp` | Cross-page version chain handling |

## Migration Notes

N/A - This is the initial version chain format for ScratchBird Alpha.

## Related Specifications

- [Transaction Lifecycle](./transaction_lifecycle.md) - Transaction states affecting version visibility
- [MGA Visibility Rules](./mga_visibility_rules.md) - How visibility is computed
- [GC Sweep Algorithm](./gc_sweep_algorithm.md) - When versions are reclaimed
- [Page Layout](./page_layout.md) - Physical page structure

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| MGA | Multi-Generational Architecture - MVCC approach maintaining multiple record versions |
| Back Version | Previous state of a record preserved during update |
| TID | Tuple Identifier - unique address of a tuple (GPID + slot) |
| GPID | Global Page ID - tablespace + page number |
| Stable TID | TID that never changes during updates (enables index stability) |
| Version Chain | Linked list of record versions from newest to oldest |

### References

- Firebird MGA documentation
- `MGA_RULES.md` - Internal MGA implementation rules
- `ON_DISK_FORMAT.md` - Overall on-disk format specification

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
