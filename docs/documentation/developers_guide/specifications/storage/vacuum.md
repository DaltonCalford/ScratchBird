# Specification: VACUUM

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

This specification defines VACUUM operations for space reclamation, tuple freezing, and database maintenance. VACUUM removes dead tuples, compacts pages, and prevents transaction ID wraparound.

## Scope

### In Scope

- VACUUM phases (scan, prune, freeze, cleanup)
- Dead tuple detection
- Page freezing for XID wraparound
- Visibility map maintenance
- Cost-based delay

### Out of Scope

- AUTOVACUUM scheduling policy
- ANALYZE statistics collection
- Full VACUUM (table rewrite)
- Parallel VACUUM

## Background

VACUUM performs three main functions:
1. **Garbage Collection**: Remove dead tuple versions
2. **Freezing**: Mark old tuples as permanently visible
3. **Statistics**: Update visibility maps for optimization

## Specification

### Data Structures

#### VacuumStats

```cpp
struct VacuumStats {
    uint64_t pages_scanned;
    uint64_t pages_vacuumed;
    uint64_t tuples_deleted;
    uint64_t tuples_frozen;
    uint64_t dead_tuples;
    uint64_t space_reclaimed;
    uint64_t duration_ms;
};
```

#### VisibilityMap (per page)

```cpp
// 2 bits per page
enum class PageVisibility : uint8_t {
    NOT_VISIBLE = 0,        // Has dead tuples, needs vacuum
    PARTIALLY_VISIBLE = 1,  // All tuples visible to all, some frozen
    FULLY_VISIBLE = 2,      // All frozen
    UNUSED = 3              // Page empty
};

// In-memory bitmap, flushed to disk
class VisibilityMap {
    std::vector<uint8_t> bitmap;  // 2 bits per heap page
};
```

### VACUUM Phases

```
VACUUM Algorithm:

Phase 1: Heap Scan
├─ Scan all heap pages
├─ Identify dead tuples (xmax committed and < OIT)
├─ Collect dead tuple TIDs
└─ Build dead tuple list

Phase 2: Index Cleanup
├─ For each index on table
├─ Remove entries pointing to dead tuples
└─ Mark index pages

Phase 3: Heap Pruning
├─ Revisit pages with dead tuples
├─ Remove dead tuples
├─ Compact page (defragment)
└─ Update line pointers

Phase 4: Freezing
├─ Scan pages again
├─ Mark old tuples frozen (xmin = FROZEN_XID)
├─ Update visibility map
└─ Record frozen pages

Phase 5: Truncation (optional)
├─ Find trailing empty pages
├─ Truncate relation
└─ Return pages to FSM
```

### Dead Tuple Detection

```
Algorithm: isTupleDead(tuple, oit)

1. IF tuple.xmax == 0:
2.     RETURN false  // Not deleted

3. xmax_state = getTransactionState(tuple.xmax)
4. IF xmax_state != COMMITTED:
5.     RETURN false  // Delete not committed

6. IF tuple.xmax >= oit:
7.     RETURN false  // Delete not visible to all

8. // xmax is committed and old enough
9. IF tuple.infomask & HEAP_UPDATED:
10.    // Check if there's a newer version
11.    IF NOT tuple.hasBackVersion():
12.        // This is the latest, can't remove yet
13.        // (need to check if newer is visible)
14.        RETURN false
15.
16. RETURN true  // Dead and removable
```

### Freezing

```
Algorithm: freezeTuples(page, freeze_limit)

1. frozen_count = 0
2. 
3. FOR each item IN page:
4.     IF item.isDeleted() OR item.isUnused():
5.         CONTINUE
6.     
7.     tuple = page.getTuple(item)
8.     
9.     // Check if freezable
10.    IF tuple.xmin >= freeze_limit:
11.        CONTINUE  // Too recent
12.    
13.    xmin_state = getTransactionState(tuple.xmin)
14.    IF xmin_state != COMMITTED:
15.        CONTINUE  // Not committed, don't freeze
16.    
17.    IF tuple.infomask & HEAP_XMIN_FROZEN:
18.        CONTINUE  // Already frozen
19.    
20.    // Freeze the tuple
21.    tuple.xmin = FROZEN_XID
22.    tuple.infomask |= HEAP_XMIN_FROZEN
23.    tuple.infomask &= ~HEAP_XMIN_COMMITTED  // Not needed anymore
24.    
25.    frozen_count++

26. IF frozen_count > 0:
27.    markPageDirty(page)
28.    updateVisibilityMap(page, frozen_count)

29. RETURN frozen_count
```

### Interface Contracts

#### Function: `vacuumTable()`

```cpp
Status GcManager::vacuumTable(
    const ID &table_id,
    VacuumOptions options,
    VacuumStats *stats_out,
    ErrorContext *ctx
);
```

**Parameters:**
```cpp
struct VacuumOptions {
    bool freeze_table;          // Force freeze all tuples?
    bool truncate;              // Truncate empty pages?
    uint32_t cost_delay;        // Vacuum cost delay (ms)
    uint32_t cost_limit;        // Cost limit per transaction
    uint64_t freeze_min_age;    // Minimum XID age to freeze
};
```

**Algorithm:**
```
1. // Setup
2. stats = {}
3. dead_tuples = []
4. oit = transaction_manager->getOldestXid()
5. freeze_limit = oit - freeze_min_age

6. // Phase 1: Scan heap
7. FOR each page_id IN table:
8.     Pin page
9.     stats.pages_scanned++
10.    
11.    FOR each item IN page:
12.        IF isTupleDead(item, oit):
13.            dead_tuples.push_back({page_id, item.id})
14.            stats.dead_tuples++
15.    
16.    // Cost-based delay
17.    IF options.cost_delay > 0:
18.        applyCostDelay(stats, options)
19.    
20.    Unpin page

21. // Phase 2: Index cleanup
22. FOR each index IN table.indexes:
23.    removeIndexEntriesForDeadTuples(index, dead_tuples)

24. // Phase 3: Heap pruning
25. FOR each (page_id, items) IN groupByPage(dead_tuples):
26.    Pin page (Vacuum strategy)
27.    FOR each item IN items:
28.        removeDeadTuple(page, item)
29.    defragmentPage(page)
30.    stats.pages_vacuumed++
31.    Unpin page (dirty)

32. // Phase 4: Freezing
33. FOR each page_id IN table:
34.    Pin page
35.    stats.tuples_frozen += freezeTuples(page, freeze_limit)
36.    Unpin page (dirty if frozen)

37. // Phase 5: Truncation
38. IF options.truncate:
39.    empty_pages = findTrailingEmptyPages(table)
40.    truncateTable(table, empty_pages)
41.    stats.space_reclaimed += empty_pages * page_size

42. *stats_out = stats
43. RETURN OK
```

## Invariants

1. **Safety**: Never remove a version visible to any active transaction
   - Verification: xmax < OIT check
   
2. **Freeze Monotonicity**: Frozen tuples stay frozen
   - Verification: Never unfreeze
   
3. **Visibility Map Accuracy**: Map reflects actual page state
   - Verification: Update after freeze/defragment

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `LOCK_TIMEOUT` | Can't acquire vacuum lock | Retry or skip page |
| `IO_ERROR` | Page read/write failure | Skip page, continue |
| `INTERRUPTED` | VACUUM interrupted | Commit progress so far |

## Performance Considerations

### Cost-Based Delay
- **Purpose**: Prevent VACUUM from monopolizing I/O
- **Mechanism**: Sleep after accumulating cost points
- **Default**: 20ms delay per 1000 pages

### Visibility Map
- **Benefit**: Skip pages known to be all-frozen
- **Size**: 2 bits per heap page (~0.25% overhead)
- **Update**: During freeze, after prune

### HOT Pruning
- **Benefit**: Clean up HOT chains during VACUUM
- **Trigger**: When chain length > threshold
- **Action**: Compress chain to single visible version

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_vacuum.cpp` | Basic VACUUM |
| `tests/unit/test_vacuum_freeze.cpp` | Tuple freezing |
| `tests/unit/test_vacuum_prune.cpp` | Dead tuple removal |
| `tests/unit/test_vacuum_truncation.cpp` | Page truncation |

## Related Specifications

- [GC Sweep](./gc_sweep.md) - Garbage collection
- [Transaction ID Allocation](./transaction_id_allocation.md) - XID freezing
- [FSM Management](./fsm_management.md) - Space reuse

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
