# Specification: Update Chains

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines UPDATE chains in ScratchBird's Firebird MGA implementation, where updates create back versions forming a linked list from newest to oldest. The primary tuple location is overwritten in-place while preserving older versions via back pointers.

## Scope

### In Scope

- Back version creation during UPDATE
- Chain traversal for visibility
- Cross-page back versions
- Chain pruning and garbage collection
- TID stability guarantees

### Out of Scope

- TOAST chains (see TOAST Storage)
- Index chain maintenance (see Index specs)
- HOT updates (see HOT Updates spec)

## Background

UPDATE Chain Characteristics:
- **Stable TID**: Item pointer never moves (index entries remain valid)
- **Newest-to-Oldest**: Chain points backward from current to past
- **In-Place Update**: Primary location overwritten
- **Back Version Storage**: Old data copied to new location (often same page)

## Specification

### Data Structures

#### Back Version Pointer

```cpp
// Within TupleHeader
struct TupleHeader {
    uint64_t back_version_gpid;  // GPID of previous version
    uint16_t back_version_slot;  // Slot/offset of previous version
    uint16_t reserved1;
};

// Helper methods
bool hasBackVersion() const {
    return back_version_gpid != INVALID_GPID;
}

TID getBackVersionTID() const {
    return TID(back_version_gpid, back_version_slot);
}

void setBackVersionTID(GPID gpid, uint16_t slot) {
    back_version_gpid = gpid;
    back_version_slot = slot;
}
```

#### Chain Node (Conceptual)

```
Chain Node:
┌─────────────────────────────────────────────────────────────┐
│ Tuple Header                                                │
│ ├─ xmin: Transaction that created this version             │
│ ├─ xmax: Transaction that updated/deleted (0 if current)   │
│ ├─ back_version_gpid/slot: Pointer to older version        │
│ └─ infomask: HEAP_UPDATED if this is an old version        │
├─────────────────────────────────────────────────────────────┤
│ Tuple Data                                                  │
│ └─ Column values as of this version                        │
└─────────────────────────────────────────────────────────────┘
```

### Chain Structure

```
UPDATE Chain Example (3 versions):

TID (1, 5) - Item pointer 5 on page 1
     │
     ├─ [Version 3] (current, in primary location)
     │   xmin = 100 (committed)
     │   xmax = 0
     │   back_version ──────┐
     │   data = "current"   │
     │                      │
     │   [ItemPointer[5]    │
     │    points here]      │
     │                      │
     ▼                      │
   [Version 2] (back version at offset 6500)
     xmin = 80 (committed)
     xmax = 100 (committed update)
     back_version ──────────┤
     data = "previous"      │
     infomask |= UPDATED    │
                            │
                            ▼
                          [Version 1] (back version at offset 7200)
                            xmin = 50 (committed)
                            xmax = 80 (committed update)
                            back_version = INVALID
                            data = "original"
                            infomask |= UPDATED
```

### Interface Contracts

#### Function: `updateTuple()` (Chain Creation)

```cpp
// Source: src/core/heap_page.cpp:750
Status HeapPage::updateTuple(
    uint16_t old_item_id,      // Primary location to update
    const uint8_t *new_tuple_data,
    uint32_t new_tuple_size,
    uint64_t xmax,             // This transaction updates
    uint64_t new_xmin,         // New version's creator
    uint16_t *new_item_id_out,
    ErrorContext *ctx
);
```

**Preconditions:**
- `old_item_id` is valid and not deleted
- Sufficient space for back version + new tuple

**Postconditions:**
- Old tuple copied to back version location
- Old tuple header updated with xmax and HEAP_UPDATED
- Primary location overwritten with new tuple
- New tuple has back_version pointing to old location
- `*new_item_id_out == old_item_id` (TID stable)

**Algorithm:**
```
1. // Validate
2. old_item = getItemPointer(old_item_id)
3. IF old_item.isDeleted(): RETURN INVALID_STATE

4. // Calculate space
5. old_tuple = page_data + old_item.offset
6. old_size = old_item.length
7. needed = old_size + new_tuple_size + sizeof(ItemPointer)
8. IF NOT hasFreeSpace(needed): RETURN PAGE_FULL

9. // Phase 1: Create back version
10. back_offset = pageUpper - old_size
11. back_offset = ALIGN_8(back_offset)
12. COPY old_tuple TO page_data[back_offset]
13. 
14. back_header = page_data + back_offset
15. back_header->xmax = xmax
16. back_header->infomask |= HEAP_UPDATED | HEAP_XMAX_COMMITTED

11. // Phase 2: Overwrite primary location
12. IF new_tuple_size <= old_size:
13.     // Fits in place
14.     new_offset = old_item.offset
15.     COPY new_tuple_data TO page_data[new_offset]
16.     old_item.length = new_tuple_size
17. ELSE:
18.     // Need new location
19.     new_offset = back_offset - new_tuple_size
20.     new_offset = ALIGN_8(new_offset)
21.     COPY new_tuple_data TO page_data[new_offset]
22.     old_item.offset = new_offset
23.     old_item.length = new_tuple_size
24. 
25. pageUpper = MIN(back_offset, new_offset)

26. // Phase 3: Set up new version header
27. new_header = page_data + new_offset
28. new_header->xmin = new_xmin
29. new_header->xmax = 0
30. new_header->back_version_gpid = MAKE_GPID(tablespace_id, page_id)
31. new_header->back_version_slot = back_offset  // Store offset as slot
32. new_header->row_uuid = back_header->row_uuid  // Preserve identity
33. new_header->infomask = 0

34. *new_item_id_out = old_item_id
35. RETURN OK
```

#### Function: `findVisibleVersion()` (Chain Traversal)

```cpp
// Source: src/core/heap_page.cpp:1206
Status HeapPage::findVisibleVersion(
    uint16_t item_id,          // Starting item ID (newest version)
    uint64_t current_xid,      // Reader's XID
    const uint8_t **data_out,
    uint32_t *size_out,
    ErrorContext *ctx
);
```

**Algorithm:**
```
1. current_item_id = item_id
2. is_back_version = false
3. current_offset = 0
4. chain_length = 0
5. visited = empty_set()

6. WHILE chain_length < MAX_CHAIN_LENGTH (1000):
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
17.        item_ptr = getItemArray()[current_item_id]
18.        tuple_hdr = page_data + item_ptr.offset
19.    
20.    // Check visibility
21.    IF isTupleVisible(tuple_hdr, current_xid):
22.        *data_out = tuple_data
23.        *size_out = tuple_size
24.        RETURN OK
25.    
26.    // Not visible - follow back version
27.    IF NOT tuple_hdr->hasBackVersion():
28.        RETURN NOT_FOUND  // No visible version
29.    
30.    back_tid = tuple_hdr->getBackVersionTID()
31.    IF back_tid.page_id != current_page_id:
32.        // Cross-page chain - switch to other page
33.        RETURN handleCrossPageVersion(back_tid, current_xid, data_out, size_out)
34.    
35.    current_offset = back_tid.slot
36.    is_back_version = true
37.    chain_length++

38. RETURN PAGE_CORRUPT (chain too long)
```

### Cross-Page Back Versions

When back version is on different page:

```
Algorithm: handleCrossPageVersion(back_tid, current_xid)

1. Pin back_tid.page_id
2. back_page = getPage(back_tid.page_id)
3. 
4. // Read version from offset (back_tid.slot)
5. tuple_hdr = back_page.data + back_tid.slot
6. 
7. IF isTupleVisible(tuple_hdr, current_xid):
8.     // Copy to cross-page buffer before unpinning
9.     cross_page_buffer_.resize(tuple_size)
10.    COPY tuple_data TO cross_page_buffer_
11.    Unpin back_tid.page_id
12.    *data_out = cross_page_buffer_.data()
13.    RETURN OK
14.
15. // Not visible on other page - continue chain
16. IF tuple_hdr->hasBackVersion():
17.    next_tid = tuple_hdr->getBackVersionTID()
18.    Unpin back_tid.page_id
19.    RETURN handleCrossPageVersion(next_tid, current_xid, ...)
20.
21. Unpin back_tid.page_id
22. RETURN NOT_FOUND
```

## Invariants

1. **TID Stability**: Primary item pointer never changes during updates
   - Verification: updateTuple returns same item_id
   
2. **Chain Acyclicity**: No cycles in version chain
   - Verification: Visited set during traversal
   
3. **Version Ordering**: xmin strictly decreases along chain
   - Verification: Assert back_version.xmin < current.xmin
   
4. **Chain Termination**: Every chain ends with !hasBackVersion()
   - Verification: MAX_CHAIN_LENGTH limit

## Performance Considerations

### Chain Length Distribution
- **Typical**: 1-3 versions (most rows updated few times)
- **Heavy updates**: 10-20 versions (hot rows)
- **Limit**: 1000 versions (cycle detection threshold)

### Same-Page vs Cross-Page
- **Same-page**: ~100 ns (cache hit)
- **Cross-page**: ~10 µs (buffer pool lookup)
- **Optimization**: Prefer same-page for HOT updates

### Pruning Benefit
- **Before pruning**: Chain traversal O(chain_length)
- **After pruning**: Often O(1) for newest version
- **Frequency**: Vacuum prunes dead versions

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_update_chains.cpp` | Chain creation |
| `tests/unit/test_chain_traversal.cpp` | Visibility traversal |
| `tests/unit/test_cross_page_chains.cpp` | Cross-page handling |
| `tests/unit/test_chain_cycles.cpp` | Cycle detection |

## Related Specifications

- [Version Chain Format](./version_chain_format.md) - Physical structure
- [Tuple Versions](./tuple_versions.md) - Version semantics
- [HOT Updates](./hot_updates.md) - Same-page optimization
- [GC Sweep](./gc_sweep.md) - Dead version pruning

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
