# Specification: HOT Updates

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

This specification defines Heap-Only Tuple (HOT) updates, an optimization that avoids index updates when indexed columns don't change. HOT updates keep both old and new versions on the same page with special chaining.

## Scope

### In Scope

- HOT update eligibility criteria
- HOT chain structure
- HOT pruning
- Index redirect pointers
- HOT update decision algorithm

### Out of Scope

- Regular UPDATE chains (see Update Chains)
- Index maintenance for non-HOT updates
- Cross-page HOT (not supported)

## Background

**Problem**: Regular UPDATE requires updating all indexes even when indexed columns don't change.

**HOT Solution**:
1. Keep both versions on same page
2. Chain them specially (HOT chain)
3. Don't update indexes (they still point to root tuple)
4. Prune chain when safe

**Requirements for HOT**:
- New version fits on same page
- No indexed columns modified
- Sufficient free space

## Specification

### Data Structures

#### HOT Chain Flags

```cpp
// From include/scratchbird/core/heap_page.h:134
static constexpr uint16_t HEAP_HOT_UPDATED = 0x0200;  // Part of HOT chain
static constexpr uint16_t HEAP_UPDATED = 0x0040;      // Regular update
```

#### HOT Chain Structure

```
HOT Chain (same page):

[Root Line Pointer]          [Redirect Line Pointer]
     │                              │
     ▼                              ▼
[Version 1 - Root] ──HOT──► [Version 2] ──HOT──► [Version 3 - Latest]
  xmin=50                     xmin=80               xmin=100
  xmax=80                     xmax=100              xmax=0
  infomask:                   infomask:             infomask:
    HOT_UPDATED                 HOT_UPDATED           (none)
  root tuple                  intermediate           current

Indexes point to Root LP (Line Pointer), 
which redirects to Latest via HOT chain.
```

### HOT Eligibility

```
Algorithm: canHOTUpdate(table, old_tuple, new_values)

1. // Check space
2. IF NOT hasFreeSpaceOnPage(page_id, new_tuple_size):
3.     RETURN false  // Must fit on same page
4.
5. // Check indexed columns
6. FOR each column IN table.indexed_columns:
7.     IF new_values[column] != old_tuple[column]:
8.         RETURN false  // Indexed column changed
9.
10. // Check other restrictions
11. IF table.has_exclusion_constraints:
12.     RETURN false
13.
14. IF old_tuple.is_compressed != new_values.is_compressed:
15.     RETURN false
16.
17. RETURN true
```

### HOT Update Algorithm

```
Algorithm: hotUpdate(item_id, new_data, xmax, new_xmin)

1. // Find end of HOT chain
2. root_item_id = item_id
3. current = getTuple(item_id)
4. WHILE current.infomask & HEAP_HOT_UPDATED:
5.     IF current.hasBackVersion():
6.         current = getBackVersion(current)
7.     ELSE:
8.         BREAK  // Shouldn't happen for valid chain

9. // current is now the latest version in chain
10. 
11. // Create new version (not back version - separate chain link)
12. new_offset = pageUpper - new_tuple_size
13. new_offset = ALIGN_8(new_offset)
14. 
15. // Get new item slot
16. new_item_id = allocateItemPointer()
17. items[new_item_id].offset = new_offset
18. items[new_item_id].length = new_tuple_size
19.
20. // Set up new version header
21. new_header = page_data + new_offset
22. new_header->xmin = new_xmin
23. new_header->xmax = 0
24. new_header->infomask = 0  // Current, no flags
25. new_header->row_uuid = current->row_uuid
26.
27. // Update previous latest version
28. current->xmax = xmax
29. current->infomask |= HEAP_HOT_UPDATED
30. // HOT versions don't use back_version pointers
31. // Instead, chain is implicit via same-page, sequential item IDs

32. // Root line pointer stays unchanged
33. // Index entries still valid

34. pageUpper = new_offset
35. RETURN new_item_id
```

### HOT Chain Traversal

```
Algorithm: findHOTVisibleVersion(root_item_id, reader_xid)

1. // Start at root
2. current_item_id = root_item_id
3. 
4. WHILE true:
5.     tuple = getTuple(current_item_id)
6.     
7.     IF isVisible(tuple, reader_xid):
8.         RETURN tuple
9.     
10.    // Not visible, check if HOT chain continues
11.    IF NOT (tuple.infomask & HEAP_HOT_UPDATED):
12.        // End of HOT chain, check regular back version
13.        IF tuple.hasBackVersion():
14.            RETURN findVisibleVersion(root_item_id, reader_xid)
15.        RETURN NOT_FOUND
16.    
17.    // Follow HOT chain (next item ID on same page)
18.    current_item_id++
19.    IF current_item_id >= page.item_count:
20.        RETURN NOT_FOUND  // Chain broken
```

### HOT Pruning

When root version is dead (xmax committed and old enough):

```
Algorithm: pruneHOTChain(page_id, root_item_id, oit)

1. chain = collectHOTChain(root_item_id)
2. 
3. // Find first visible version
4. visible_version = nullptr
5. FOR tuple IN chain (newest to oldest):
6.     IF isVisible(tuple, oit):  // Using OIT as conservative reader
7.         visible_version = tuple
8.         BREAK
9.
10. IF visible_version == nullptr:
11.    // No visible versions - entire chain dead
12.    FOR item_id IN chain.item_ids:
13.        markItemUnused(item_id)
14.    RETURN ALL_DEAD
15.
16. IF visible_version == chain.back():
17.    // Oldest version visible - move to root position
18.    moveTupleToRoot(visible_version, root_item_id)
19.    FOR item_id IN chain.item_ids (except root):
20.        markItemUnused(item_id)
21.    RETURN PRUNED
22.
23. // Intermediate version visible - can't prune yet
24. RETURN CANNOT_PRUNE
```

## Invariants

1. **Same Page**: All HOT chain members on same page
   - Verification: Assert during update
   
2. **No Index Changes**: HOT updates never modify indexes
   - Verification: Skip index updates for HOT
   
3. **Chain Continuity**: Item IDs in HOT chain are sequential
   - Verification: Linear scan finds all members
   
4. **Root Stability**: Root item pointer never moves
   - Verification: Index entries remain valid

## Performance Benefits

### Update Performance
| Scenario | Regular UPDATE | HOT UPDATE | Improvement |
|----------|---------------|------------|-------------|
| 1 index | 2 writes | 1 write | 50% |
| 5 indexes | 6 writes | 1 write | 83% |
| 10 indexes | 11 writes | 1 write | 91% |

### Space Efficiency
- **Regular update**: Back version + new version + index entries
- **HOT update**: Chain versions on same page, no index changes
- **Benefit**: Less bloat, fewer pages dirtied

### Read Performance
- **Chain length**: Limited by page size (typically 2-5 versions)
- **Traversal**: O(chain_length) within page
- **Pruning**: Keeps chains short

## Decision Trees

```
UPDATE Decision:
│
├─ Indexed columns changed? ────────────────────► Regular UPDATE
│
├─ New version fits on page? ───────────────────► Regular UPDATE
│
├─ Exclusion constraints? ──────────────────────► Regular UPDATE
│
└─ ELSE ────────────────────────────────────────► HOT UPDATE
```

## Limitations

1. **Same Page Only**: Cannot HOT update if page is full
2. **No Indexed Column Changes**: Any indexed column change forces regular UPDATE
3. **No TOAST**: TOASTed columns disqualify HOT
4. **Exclusion Constraints**: Tables with exclusion constraints can't use HOT

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_hot_updates.cpp` | Basic HOT |
| `tests/unit/test_hot_pruning.cpp` | Pruning |
| `tests/unit/test_hot_eligibility.cpp` | Eligibility checks |
| `tests/unit/test_hot_chain_limit.cpp` | Page size limits |

## Related Specifications

- [Update Chains](./update_chains.md) - Regular update chaining
- [Version Chain Format](./version_chain_format.md) - Physical structure
- [GC Sweep](./gc_sweep.md) - HOT chain pruning

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
