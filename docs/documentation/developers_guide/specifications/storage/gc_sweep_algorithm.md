# Specification: GC Sweep Algorithm

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/gc_manager.h:25`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/sweep_manager.h:27`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/gc_manager.cpp:51`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/sweep_manager.cpp:57`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_garbage_collector.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gc_safe_horizon.cpp`

## Synopsis

This specification defines the Garbage Collection (GC) and Sweep algorithms used in ScratchBird. GC reclaims space from dead tuple versions, while Sweep advances the Oldest Interesting Transaction (OIT) marker to enable more aggressive cleanup.

## Scope

### In Scope

- GC horizon calculation (OIT, OAT-based)
- Dead tuple detection algorithm
- Version chain pruning
- Page compaction
- Sweep trigger conditions
- OIT advancement algorithm
- Freeze operations for XID wraparound prevention

### Out of Scope

- TOAST garbage collection (separate specification)
- Index cleanup during GC (covered in index specifications)
- Background/auto-vacuum scheduling policy

## Background

ScratchBird implements cooperative garbage collection:

1. **GC Manager**: Reclaims space from dead tuples at table/page level
2. **Sweep Manager**: Advances OIT by scanning TIP pages
3. **Freeze Operation**: Prevents XID wraparound by freezing old tuples

Key concepts:
- **GC Horizon**: Oldest XID that might still see a tuple (min of OAT values)
- **Dead Tuple**: Tuple with xmax < GC horizon and xmax is committed
- **Prunable Version**: Back version that is no longer needed

## Specification

### Data Structures

#### GC Statistics

```cpp
// From include/scratchbird/core/gc_manager.h:25
struct GcStats {
    uint64_t pages_scanned;         // Pages examined
    uint64_t tuples_scanned;        // Tuples examined
    uint64_t dead_tuples_found;     // Tuples marked as dead
    uint64_t dead_tuples_removed;   // Tuples actually removed
    uint64_t version_chains_pruned; // Versions removed from chains
    uint64_t pages_compacted;       // Pages compacted
    uint64_t free_space_recovered;  // Bytes reclaimed
    uint64_t tuples_frozen;         // Tuples frozen
    uint64_t gc_time_us;            // Time spent in GC
};
```

#### Sweep Statistics

```cpp
// From include/scratchbird/core/sweep_manager.h:27
struct SweepStatistics {
    uint64_t sweep_count = 0;              // Total sweeps executed
    uint64_t last_sweep_time = 0;          // Timestamp of last sweep
    uint64_t last_sweep_duration_ms = 0;   // Duration in ms
    uint64_t last_oit_before = 0;          // OIT before sweep
    uint64_t last_oit_after = 0;           // OIT after sweep
    uint64_t total_transactions_swept = 0; // Cumulative transactions
    bool sweep_in_progress = false;        // Currently running?
};
```

### Interface Contracts

#### Function: `GcManager::gcTable()`

```cpp
// Source: src/core/gc_manager.cpp:51
Status GcManager::gcTable(
    const ID &table_id,    // Table to GC
    GcStats *stats_out,    // Output: statistics
    ErrorContext *ctx
);
```

**Preconditions:**
- GC horizon can be computed (at least one backend registered)
- Table exists

**Postconditions:**
- Dead tuples identified and removed
- Version chains pruned
- Pages compacted if beneficial
- Statistics collected

#### Function: `SweepManager::checkSweepTrigger()`

```cpp
// Source: src/core/sweep_manager.cpp:57
bool SweepManager::checkSweepTrigger(ErrorContext *ctx);
```

**Preconditions:**
- Sweep manager initialized
- Transaction manager available

**Postconditions:**
- Returns true if sweep was triggered
- Sweep may run in background

**Trigger Condition:**
```
IF (OST - OIT) > sweep_interval (default 20000):
    TRIGGER_SWEEP()
```

#### Function: `SweepManager::executeSweep()`

```cpp
// Source: src/core/sweep_manager.cpp:102
Status SweepManager::executeSweep(
    bool foreground,      // true = with space reclamation
    ErrorContext *ctx
);
```

**Preconditions:**
- No sweep currently in progress
- Transaction manager available

**Postconditions:**
- OIT advanced to first uncommitted transaction
- Statistics updated
- If foreground: space reclaimed from old versions

### Algorithms

#### Algorithm: GC Table

```
Input:  table_id
Output: stats

1. // Get GC horizon
2. horizon = getGcHorizon()
3. IF horizon == UINT64_MAX:
4.     RETURN (nothing can be cleaned)
5.
6. // Phase 1: Scan for dead tuples
7. dead_tids = []
8. FOR each page IN table:
9.     Pin page with Vacuum strategy
10.    IF page.type != HEAP:
11.        Unpin, CONTINUE
12.    stats.pages_scanned++
13.    
14.    FOR each item_id IN page:
15.        tuple = page.getTuple(item_id)
16.        stats.tuples_scanned++
17.        
18.        IF isTupleDead(tuple, horizon):
19.            dead_tids.push(MAKE_TID(page_id, item_id))
20.            stats.dead_tuples_found++
21.    
22.    Unpin page
23.
24. // Phase 2: Group by page
25. dead_by_page = GROUP dead_tids BY page_id
26.
27. // Phase 3: Remove dead tuples page by page
28. FOR each (page_id, item_ids) IN dead_by_page:
29.    removeDeadTuplesFromPage(page_id, item_ids)
30.    stats.dead_tuples_removed += item_ids.count
31.
32. // Phase 4: Prune version chains
33. FOR each page IN table:
34.    pruneVersionChains(page, horizon)
35.
36. RETURN stats
```

#### Algorithm: Dead Tuple Detection

```
Input:  tuple_data, horizon
Output: is_dead (boolean)

1. tuple_hdr = CAST tuple_data TO TupleHeader
2.
3. // Tuple is dead if:
4. // 1. xmax is set (tuple was deleted or updated)
5. // 2. xmax < horizon (delete visible to all)
6. // 3. xmax is committed
7.
8. IF tuple_hdr.xmax == 0:
9.     RETURN false  // Still alive
10.
11. IF tuple_hdr.xmax >= horizon:
12.     RETURN false  // Delete not visible to all yet
13.
14. // Check if this was an update or delete
15. IF tuple_hdr.infomask & HEAP_UPDATED:
16.     // Was updated - dead only if has next version
17.     RETURN tuple_hdr.hasNextVersion()
18.
19. // Was deleted (not updated)
20. RETURN (tuple_hdr.infomask & HEAP_XMAX_COMMITTED) != 0
```

#### Algorithm: Sweep (OIT Advancement)

```
Input:  foreground (bool)
Output: Status

1. // Prevent concurrent sweeps
2. IF NOT sweep_in_progress_.compare_exchange_strong(false, true):
3.     RETURN IO_ERROR (sweep already in progress)
4.
5. start_time = now()
6. oit_before = txn_manager_->getOldestXid()
7.
8. // Phase 1: Find new OIT
9. new_oit = findFirstUncommittedTransaction()
10.
11. IF new_oit == 0 OR new_oit == oit_before:
12.     sweep_in_progress_ = false
13.     updateStatistics(oit_before, oit_before, duration)
14.     RETURN OK
15.
16. // Phase 2: Update OIT in database header
17. status = txn_manager_->setOldestXid(new_oit)
18. IF status != OK:
19.     sweep_in_progress_ = false
20.     RETURN status
21.
22. // Phase 3: Space reclamation (foreground only)
23. IF foreground:
24.     reclaimSpace(new_oit)
25.
26. // Phase 4: Update statistics
27. duration = now() - start_time
28. updateStatistics(oit_before, new_oit, duration)
29.
30. // Phase 5: Notify GC
31. IF db_->garbage_collector() != nullptr:
32.     db_->garbage_collector()->notifySweepComplete(oit_before, new_oit)
33.
34. sweep_in_progress_ = false
35. RETURN OK
```

#### Algorithm: Find First Uncommitted Transaction

```
Input:  (none)
Output: new_oit

1. current_oit = txn_manager_->getOldestXid()
2. current_xmax = txn_manager_->getCurrentXid()
3.
4. FOR xid FROM current_oit TO current_xmax:
5.     // Skip special XIDs
6.     IF xid <= FROZEN_XID:
7.         CONTINUE
8.    
9.     state = getTransactionState(xid)
10.    IF state != COMMITTED AND state != ABORTED:
11.        // First uncommitted transaction found
12.        RETURN xid
13.
14. // All transactions committed/aborted
15. RETURN current_xmax
```

#### Algorithm: Version Chain Pruning

```
Input:  page_id, horizon
Output: (prunes versions in place)

1. Pin page with Vacuum strategy
2. page_modified = false
3.
4. FOR each item_id IN page:
5.    tuple = page.getTuple(item_id)
6.    
7.    IF isVersionPrunable(tuple, horizon):
8.        // Mark tuple as deleted
9.        tuple.xmax = horizon
10.       tuple.infomask |= HEAP_XMAX_COMMITTED
11.       stats.version_chains_pruned++
12.       page_modified = true
13.
14. Unpin page with page_modified flag

isVersionPrunable(tuple, horizon):
1. // Version is prunable if:
2. // 1. It was updated (HEAP_UPDATED set)
3. // 2. Has a next version
4. // 3. xmax is committed and < horizon
5.
6. IF NOT (tuple.infomask & HEAP_UPDATED):
7.     RETURN false
8.
9. IF NOT tuple.hasNextVersion():
10.    RETURN false  // This is the latest version
11.
12. IF tuple.xmax == 0 OR tuple.xmax >= horizon:
13.    RETURN false  // Update not visible to all yet
14.
15. RETURN true
```

#### Algorithm: Page Compaction

```
Input:  page_id
Output: stats

1. Pin page with Vacuum strategy
2.
3. // Save old state
4. old_free_space = pageUpper - pageLower
5.
6. // Create temporary buffer
7. temp_buffer = ALLOCATE page_size
8.
9. // Copy header
10. COPY page.header TO temp_buffer
11.
12. // Build list of live tuples
13. live_tuples = []
14. FOR each item IN page:
15.     IF NOT item.isDeleted() AND NOT item.isUnused():
16.         live_tuples.push(item)
17.
18. // Sort by offset (descending for top-down compaction)
19. SORT live_tuples BY offset DESCENDING
20.
21. // Compact tuples toward upper end
22. new_upper = page_size - sizeof(HeapPageSpecial)
23.
24. FOR each tuple IN live_tuples:
25.     new_offset = new_upper - tuple.length
26.     new_offset = ALIGN_8(new_offset)
27.     
28.     IF new_offset != tuple.old_offset:
29.         MEMMOVE tuple.data TO temp_buffer[new_offset]
30.    
31.     UPDATE item pointer to new_offset
32.     new_upper = new_offset
33.
34. // Update header
35. page.header.pd_upper = new_upper
36. page.header.pd_lower = sizeof(PageHeader) + (item_count * sizeof(ItemPointer))
37.
38. // Copy special area
39. COPY page.special TO temp_buffer
40.
41. // Copy back to original page
42. COPY temp_buffer TO page
43.
44. // Calculate space recovered
45. new_free_space = pageUpper - pageLower
46. stats.free_space_recovered += (new_free_space - old_free_space)
47.
48. Unpin page as dirty
```

### State Machines

```
Sweep State Machine:
┌─────────┐  trigger   ┌─────────┐  complete   ┌─────────┐
│  IDLE   │ ─────────► │RUNNING  │ ──────────► │ COMPLETE│
└─────────┘            └─────────┘             └─────────┘
     ▲                                          │
     └──────────────────────────────────────────┘
                   next trigger

GC State Machine:
┌─────────┐  gcTable()  ┌─────────┐  finish    ┌─────────┐
│  IDLE   │ ───────────►│SCANNING │ ─────────►│ CLEANUP │
└─────────┘             └─────────┘           └─────────┘
                              │                     │
                              ▼                     ▼
                        ┌─────────┐           ┌─────────┐
                        │PRUNING  │           │  DONE   │
                        └─────────┘           └─────────┘
```

| State | Event | Action | Next State |
|-------|-------|--------|------------|
| IDLE | gcTable() | Start scanning | SCANNING |
| SCANNING | Dead tuple found | Add to removal list | SCANNING |
| SCANNING | Scan complete | Begin pruning | PRUNING |
| PRUNING | Version pruned | Increment counter | PRUNING |
| PRUNING | Prune complete | Compact pages | CLEANUP |
| CLEANUP | Page compacted | Reclaim space | CLEANUP |
| CLEANUP | All pages done | Update stats | DONE |

## Invariants

1. **Safety Horizon**: Never remove a version visible to any active transaction
   - Verification: Check `xmax < horizon` before removal
   
2. **OIT Monotonicity**: OIT never moves backward
   - Verification: `new_oit >= old_oit` enforced
   
3. **Chain Integrity**: Pruning preserves chain navigability
   - Verification: Only prunable versions (with next version) are removed
   
4. **Freeze Safety**: Only committed tuples can be frozen
   - Verification: `HEAP_XMIN_COMMITTED` flag checked before freeze

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `IO_ERROR` | Page read/write failure | Skip page, continue with others |
| `PAGE_CORRUPT` | Invalid version chain | Log error, skip corrupted data |
| `LOCK_TIMEOUT` | Cannot acquire GC lock | Retry or defer to next GC pass |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_garbage_collector.cpp` | Basic GC functionality |
| `tests/unit/test_gc_safe_horizon.cpp` | Horizon calculation |
| `tests/unit/test_version_chain_gc.cpp` | Version chain pruning |
| `tests/integration/test_toast_garbage_collection.cpp` | TOAST GC integration |
| `tests/unit/test_wraparound_detection.cpp` | Freeze operations |

## Migration Notes

N/A - Initial GC/Sweep specification for ScratchBird Alpha.

## Related Specifications

- [Version Chain Format](./version_chain_format.md) - Structure being cleaned
- [MGA Visibility Rules](./mga_visibility_rules.md) - Determines dead vs live
- [Transaction Lifecycle](./transaction_lifecycle.md) - Transaction states
- [Page Layout](./page_layout.md) - Physical page structure

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| GC | Garbage Collection - reclaiming space from dead versions |
| Sweep | Advancing OIT by scanning TIP pages |
| OIT | Oldest Interesting Transaction - GC horizon boundary |
| OAT | Oldest Active Transaction - oldest running transaction |
| Horizon | Oldest XID that might still see a tuple |
| Dead Tuple | Tuple no longer visible to any transaction |
| Prunable | Version that can be safely removed |
| Freeze | Setting xmin to FROZEN_XID to prevent wraparound |

### References

- Firebird MGA documentation
- PostgreSQL VACUUM documentation (contrast)

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
