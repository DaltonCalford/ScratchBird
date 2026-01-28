# Index Garbage Collection Protocol

**Version**: 1.0
**Date**: October 18, 2025
**Status**: Phase 2 Task 2.1 - Protocol Definition

---

## Executive Summary

This document defines the protocol for garbage collection of dead index entries in ScratchBird's Firebird MGA architecture. The protocol integrates index cleanup with the existing heap sweep process, ensuring that dead tuple references are removed from all indexes after tuples become permanently unreachable.

---

## Table of Contents

1. [Overview](#overview)
2. [Firebird MGA Garbage Collection Model](#firebird-mga-garbage-collection-model)
3. [Protocol Definition](#protocol-definition)
4. [Index GC Interface](#index-gc-interface)
5. [Integration with Heap Sweep](#integration-with-heap-sweep)
6. [Implementation Guidelines](#implementation-guidelines)
7. [Error Handling](#error-handling)
8. [Performance Considerations](#performance-considerations)
9. [Testing Requirements](#testing-requirements)
10. [Columnstore Index GC (Full Design)](#columnstore-index-gc-full-design)
11. [LSM-Tree Index GC (Full Design)](#lsm-tree-index-gc-full-design)

---

## Overview

### Purpose

The Index GC Protocol defines how index structures remove entries pointing to dead tuples after the heap sweep determines those tuples are no longer visible to any active transaction.

### Key Principles

1. **Heap-Driven**: Heap sweep identifies dead tuples via OIT (Oldest Interesting Transaction)
2. **Bulk Operations**: Dead TIDs collected and passed to indexes in batches
3. **Eventual Consistency**: Index cleanup may lag heap cleanup (acceptable)
4. **Non-Blocking**: Index GC should not block normal index operations excessively
5. **Fire and Forget**: Heap sweep calls index GC but doesn't wait for completion

---

## Firebird MGA Garbage Collection Model

### Transaction Lifecycle

```
Transaction Timeline:
│
├─ T1: BEGIN (XID = 100)
├─ T2: INSERT tuple → xmin=100, xmax=INVALID
├─ T3: COMMIT (XID = 100 committed)
├─ T4: BEGIN (XID = 101)
├─ T5: DELETE tuple → xmax=101, back_version created
├─ T6: COMMIT (XID = 101 committed)
│
├─ [Time passes, new transactions start]
│
├─ OIT advances to 102 (all txns < 102 complete)
│
└─ T7: Tuple is DEAD (xmax=101 < OIT=102)
     └─ Sweep can physically remove it
```

### OIT (Oldest Interesting Transaction)

**Definition**: The oldest transaction ID that any currently active transaction might need to see.

**Calculation**:
```cpp
OIT = min(active_snapshots.oldest_xid)
```

**Properties**:
- All transactions with XID < OIT are complete (committed or aborted)
- No active transaction can see data from transactions < OIT
- Tuples with `xmax < OIT` are permanently invisible → DEAD

### Dead Tuple Identification

A tuple is **DEAD** when:
```cpp
bool is_dead = (tuple.xmax != INVALID_XID) &&    // Tuple was deleted/updated
               (tuple.xmax < oit) &&              // Deleting txn is old
               (txn_state(tuple.xmax) == COMMITTED); // Txn committed
```

**IMPORTANT**: Dead tuples can be physically removed because:
1. No active transaction can see them (OIT guarantee)
2. No future transaction will ever see them (monotonic XIDs)
3. All version chains have been resolved

---

## Protocol Definition

### Phase 1: Heap Sweep (Existing)

**Location**: `garbage_collector.cpp::cleanPage()`

**Process**:
1. Get current OIT from TransactionManager
2. Pin heap page from buffer pool
3. Scan all tuples on page
4. Identify dead tuples (xmax < OIT and committed)
5. Call `HeapPage::prunePage(oit)` to physically remove dead tuples
6. **Collect dead TIDs** for index cleanup

**Output**: Vector of dead TIDs: `std::vector<uint64_t> dead_tids`

### Phase 2: Index Cleanup (NEW)

**Location**: `garbage_collector.cpp::cleanPage()` (extended)

**Process**:
1. After heap sweep identifies dead_tids
2. Get list of indexes for the table
3. For each index:
   ```cpp
   IndexGCInterface* index = get_index(index_id);
   uint64_t entries_removed = 0;
   uint64_t pages_modified = 0;

   Status status = index->removeDeadEntries(
       dead_tids, &entries_removed, &pages_modified, ctx);

   LOG_INFO("Index %s: removed %lu entries from %lu pages",
            index->indexTypeName(), entries_removed, pages_modified);
   ```
4. Continue to next index (don't fail if one index fails)
5. Log statistics

### TID Format

**TID (Tuple Identifier)**: stable tuple address stored as `(GPID, slot)`.

```cpp
struct TID {
    GPID gpid;      // tablespace_id + page_number
    uint16_t slot;  // item slot within page
};
```

**Ordering**: compare by `gpid`, then `slot` (matches heap allocation order).

**Index storage note**:
- Indexes that persist TIDs on disk must store both `gpid` and `slot`.
- Legacy 64-bit packed TIDs are not permitted in v2 formats; on-disk
  formats must be updated to preserve full `GPID` precision.

**Stability**: In Firebird MGA, TIDs NEVER change:
- UPDATEs happen in-place at primary location
- Index entries remain valid forever (until tuple deleted)
- No need to update indexes on UPDATE (key unchanged)

---

## Index GC Interface

### Header File

**Location**: `include/scratchbird/core/index_gc_interface.h`

### Interface Definition

```cpp
class IndexGCInterface
{
public:
    virtual ~IndexGCInterface() = default;

    /**
     * Remove index entries pointing to dead tuples
     *
     * @param dead_tids Vector of TIDs confirmed dead by OIT check
     * @param entries_removed_out [OUT] Number of entries removed
     * @param pages_modified_out [OUT] Number of pages modified
     * @param ctx Error context
     * @return Status::OK on success
     */
    virtual Status removeDeadEntries(
        const std::vector<uint64_t>& dead_tids,
        uint64_t* entries_removed_out = nullptr,
        uint64_t* pages_modified_out = nullptr,
        ErrorContext* ctx = nullptr) = 0;

    /**
     * Get index type name (for logging)
     */
    virtual const char* indexTypeName() const = 0;
};
```

### Implementation Contract

**Requirements**:
1. ✅ **Idempotent**: Safe to call with same TIDs multiple times
2. ✅ **Partial Failure OK**: Remove what you can, log failures
3. ✅ **Thread-Safe**: Must handle concurrent readers
4. ✅ **Non-Blocking**: Use page-level locks, not structure-level
5. ✅ **Statistics**: Return accurate counts (entries_removed, pages_modified)

**Guarantees**:
- Dead TIDs are truly dead (OIT-verified by heap)
- No need to re-verify tuple liveness
- TIDs may not exist in index (duplicate cleanup calls OK)
- Empty vector is valid (no-op)

**Note**: The API uses `TID` (GPID + slot). Pseudocode snippets may use
scalar TID values for readability, but implementations must preserve full
`GPID` precision.

---

## Integration with Heap Sweep

### Current Heap Sweep Flow

**File**: `src/core/garbage_collector.cpp`

**Method**: `GarbageCollector::cleanPage(uint32_t page_id, ...)`

**Current Code** (simplified):
```cpp
uint64_t GarbageCollector::cleanPage(uint32_t page_id, ...)
{
    // 1. Get OIT
    uint64_t oit = txn_manager_->getOldestXid();

    // 2. Pin heap page
    void* page_buffer;
    db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);

    // 3. Prune dead tuples
    HeapPage heap_page(page_buffer, page_size);
    uint32_t tuples_pruned = 0;
    heap_page.prunePage(oit, &tuples_pruned, &space_reclaimed, ctx);

    // 4. Unpin page
    db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);

    return tuples_pruned;
}
```

### Enhanced Heap Sweep Flow (Phase 2)

**NEW Code** (to be implemented in Task 2.6):
```cpp
uint64_t GarbageCollector::cleanPage(uint32_t page_id, ...)
{
    // 1. Get OIT
    uint64_t oit = txn_manager_->getOldestXid();

    // 2. Pin heap page
    void* page_buffer;
    db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);

    // 3. Collect dead TIDs BEFORE pruning (NEW)
    HeapPage heap_page(page_buffer, page_size);
    std::vector<uint64_t> dead_tids;
    heap_page.collectDeadTuples(oit, &dead_tids, ctx);

    // 4. Prune dead tuples from heap
    uint32_t tuples_pruned = 0;
    heap_page.prunePage(oit, &tuples_pruned, &space_reclaimed, ctx);

    // 5. Unpin heap page
    db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);

    // 6. Clean indexes (NEW)
    if (!dead_tids.empty())
    {
        cleanIndexes(page_id, dead_tids, ctx);
    }

    return tuples_pruned;
}

void GarbageCollector::cleanIndexes(uint32_t page_id,
                                    const std::vector<uint64_t>& dead_tids,
                                    ErrorContext* ctx)
{
    // Get table ID from page metadata
    uint32_t table_id = getTableIdForPage(page_id, ctx);

    // Get all indexes for this table
    std::vector<IndexInfo> indexes = catalog_->getIndexesForTable(table_id, ctx);

    // Clean each index
    for (const auto& index_info : indexes)
    {
        IndexGCInterface* index = getIndexInterface(index_info.index_id, ctx);
        if (!index) continue;

        uint64_t entries_removed = 0;
        uint64_t pages_modified = 0;

        Status status = index->removeDeadEntries(
            dead_tids, &entries_removed, &pages_modified, ctx);

        if (status == Status::OK)
        {
            LOG_INFO(VACUUM, "Index %s (table %u): removed %lu entries from %lu pages",
                     index->indexTypeName(), table_id, entries_removed, pages_modified);
        }
        else
        {
            LOG_WARNING(VACUUM, "Index %s (table %u): GC failed with status %d",
                        index->indexTypeName(), table_id, static_cast<int>(status));
        }
    }
}
```

### New Methods Required

**HeapPage::collectDeadTuples()** (NEW):
```cpp
/**
 * Collect TIDs of dead tuples (for index cleanup)
 *
 * Scans page and builds vector of TIDs for tuples that are dead
 * (xmax < OIT and committed). Must be called BEFORE prunePage().
 */
Status HeapPage::collectDeadTuples(uint64_t oit,
                                   std::vector<uint64_t>* dead_tids_out,
                                   ErrorContext* ctx);
```

**GarbageCollector::cleanIndexes()** (NEW):
```cpp
/**
 * Clean index entries for dead tuples
 *
 * Called after heap sweep to remove index entries pointing to dead TIDs.
 */
void GarbageCollector::cleanIndexes(uint32_t page_id,
                                    const std::vector<uint64_t>& dead_tids,
                                    ErrorContext* ctx);
```

---

## Implementation Guidelines

### Per-Index Implementation Strategy

Note: In the examples below, `tid` refers to `TID { gpid, slot }`. Any
page_id/item_id extraction shown is legacy pseudocode and must be replaced
with GPID+slot handling in v2 implementations.

#### B-Tree Index

**Approach**: Bulk scan and remove

```cpp
Status BTree::removeDeadEntries(const std::vector<uint64_t>& dead_tids, ...)
{
    // 1. Sort dead_tids for efficient lookup
    std::set<uint64_t> dead_set(dead_tids.begin(), dead_tids.end());

    // 2. Scan leaf pages
    for (each leaf page)
    {
        // 3. For each entry in leaf page
        for (each entry)
        {
            if (dead_set.contains(entry.tid))
            {
                // Remove entry (shift entries, update count)
                remove_entry(entry);
                (*entries_removed_out)++;
            }
        }

        // 4. If page modified, mark dirty
        if (page_modified)
        {
            (*pages_modified_out)++;
        }
    }

    return Status::OK;
}
```

**Complexity**: O(L * log(D)) where L = leaf entries, D = dead TIDs

#### Hash Index

**Approach**: Hash each dead TID and check bucket

```cpp
Status HashIndex::removeDeadEntries(const std::vector<uint64_t>& dead_tids, ...)
{
    for (uint64_t tid : dead_tids)
    {
        // 1. Hash the key to find bucket
        // Problem: We don't have the key, only the TID!
        // Solution: Scan bucket chains for TID

        // This is INEFFICIENT for hash indexes
        // May need to scan all buckets
    }
}
```

**Problem**: Hash indexes don't store TIDs in predictable locations.
**Solution**: Full scan of all buckets (acceptable for GC workload).

#### GIN Index

**Approach**: Scan posting lists

```cpp
Status GinIndex::removeDeadEntries(const std::vector<uint64_t>& dead_tids, ...)
{
    std::set<uint64_t> dead_set(dead_tids.begin(), dead_tids.end());

    // 1. Scan all posting lists in posting tree
    for (each posting list)
    {
        // 2. Decompress posting list
        // 3. Remove dead TIDs
        // 4. Recompress posting list
        // 5. If empty, remove entry from entry tree
    }
}
```

**Complexity**: Decompression overhead, but bulk removal is efficient.

#### Bitmap Index

**Approach**: Clear bits for dead TIDs

```cpp
Status BitmapIndex::removeDeadEntries(const std::vector<uint64_t>& dead_tids, ...)
{
    for (uint64_t tid : dead_tids)
    {
        // 1. Extract page_id and item_id
        uint32_t page_id = tid >> 32;
        uint16_t item_id = tid & 0xFFFF;

        // 2. Compute global offset
        uint64_t offset = (page_id * MAX_ITEMS_PER_PAGE) + item_id;

        // 3. Clear bit in Roaring bitmap
        bitmap.remove(offset);
    }

    // 4. Optimize bitmap (recompress)
    bitmap.runOptimize();
}
```

**Complexity**: O(D) where D = dead TIDs (very efficient!).

---

## Error Handling

### Error Codes

- **Status::OK**: All dead entries successfully removed
- **Status::PARTIAL_FAILURE**: Some entries removed, some failed (log warnings)
- **Status::IO_ERROR**: Failed to access index pages (may retry later)
- **Status::INTERNAL_ERROR**: Index structure corruption (needs repair)

### Error Recovery

**Principle**: Best effort removal

```cpp
Status BTree::removeDeadEntries(...)
{
    uint64_t total_removed = 0;
    bool had_errors = false;

    for (each page)
    {
        Status s = remove_entries_from_page(page, dead_tids, &removed);
        if (s == Status::OK)
        {
            total_removed += removed;
        }
        else
        {
            LOG_WARNING("Failed to clean page %u: %d", page_id, s);
            had_errors = true;
            // Continue to next page (don't fail entire operation)
        }
    }

    *entries_removed_out = total_removed;
    return had_errors ? Status::PARTIAL_FAILURE : Status::OK;
}
```

### Idempotency

**Requirement**: Safe to call removeDeadEntries() multiple times with same TIDs.

**Implementation**:
- TID not found in index → no-op (not an error)
- TID already removed → no-op (count as success)
- Return actual entries removed (may be < dead_tids.size())

---

## Performance Considerations

### Batching

**Heap Sweep**: Collect dead TIDs per page (typical: 10-100 TIDs per page)

**Index Cleanup**: Process entire batch at once (bulk removal more efficient than one-by-one)

### Locking Strategy

**Page-Level Locks**: Lock index pages during modification (minimize lock duration)

**Example**:
```cpp
for (each index page)
{
    lock_page(page_id, EXCLUSIVE);
    remove_dead_entries_from_page(page, dead_tids);
    unlock_page(page_id);
}
```

**Avoid**: Structure-level locks that block entire index.

### I/O Optimization

**Buffer Pool**: Pin index pages through buffer pool (leverage caching)

**Write Batching**: Modify multiple pages before flushing (reduce I/O)

### Deferred Cleanup

**Option**: Don't block heap sweep waiting for index cleanup

**Implementation**:
```cpp
// Queue index cleanup for background thread
index_gc_queue_.push({table_id, dead_tids});

// Background thread processes queue
while (!shutdown)
{
    auto work = index_gc_queue_.pop();
    cleanIndexes(work.table_id, work.dead_tids);
}
```

**Trade-off**: Indexes may temporarily have dead entries (acceptable).

---

## Testing Requirements

### Unit Tests

**Test File**: `tests/unit/test_index_gc.cpp`

**Test Cases**:
1. ✅ Empty dead_tids vector (no-op)
2. ✅ Single dead TID removal
3. ✅ Bulk dead TID removal (100+ TIDs)
4. ✅ Dead TID not in index (idempotency)
5. ✅ Duplicate dead TIDs (idempotency)
6. ✅ All entries removed (empty page cleanup)
7. ✅ Partial failure handling
8. ✅ Statistics accuracy (entries_removed, pages_modified)

### Integration Tests

**Test File**: `tests/integration/test_sweep_gc.cpp`

**Test Cases**:
1. ✅ Sweep/GC removes dead entries from all indexes
2. ✅ Concurrent index scans during GC
3. ✅ Multiple indexes on same table
4. ✅ Large table with many dead tuples
5. ✅ Verify no dangling TIDs remain after sweep

### Performance Tests

**Metrics**:
- Time to remove 1,000 dead entries
- Time to remove 10,000 dead entries
- I/O amplification (pages modified vs pages scanned)
- Lock contention (blocking readers)

**Targets**:
- < 10ms for 1,000 entries
- < 100ms for 10,000 entries
- < 5% read blocking

---

## Columnstore Index GC (Full Design)

### Scope

Applies to the full columnstore implementation (`ColumnstoreIndex`) and
supersedes any legacy 64-bit TID assumptions. Columnstore GC must:

- Preserve stable TIDs (no row movement)
- Support MGA visibility via heap sweep (dead TIDs provided by GC)
- Avoid blocking analytic scans
- Enable segment rewrite when garbage density is high

### On-Disk Format Additions

Each column segment must persist a TID map and visibility state:

- **TID map pages**: array of `(GPID, slot)` for each row in the segment
- **Visibility bitmap**: live/dead bit per row (separate from NULL bitmap)
- **Garbage counters**: live_count, dead_count, null_count

Recommended page-level fields:

- `cs_tid_map_page` (first page of TID map chain)
- `cs_live_count`, `cs_dead_count`
- `cs_tid_min_gpid`, `cs_tid_min_slot`
- `cs_tid_max_gpid`, `cs_tid_max_slot`

TID map pages store:

```
[header][tid_count][tid_entries...][checksum]
tid_entries = { uint64_t gpid, uint16_t slot } repeated
```

### GC Algorithm

1. **Build a hash set** of dead TIDs from heap sweep.
2. **Segment prefilter**:
   - Skip segments if dead TIDs do not overlap min/max TID range.
3. **Load TID map + visibility bitmap** for candidate segments.
4. **Mark dead rows**:
   - For each TID in the segment, if in dead set:
     - Clear visibility bit
     - Increment dead_count, decrement live_count
     - Set `HAS_GARBAGE` flag
5. **Persist bitmap + counters** and update segment header.

### Segment Rewrite (Compaction)

Segments with `dead_ratio >= columnstore.gc_rewrite_threshold` are rewritten:

- Allocate new segment pages.
- Copy only live rows into new segment.
- Write new TID map and visibility bitmap (all live).
- Swap segment links atomically (update prev/next pointers).
- Mark old segment as obsolete (`cs_xmax = current_xid`) and queue pages
  for reclamation after readers drain.

### Concurrency and Safety

- Use page-level locks for segment updates.
- Rewrite is copy-on-write: readers continue on old segment until swap.
- Buffer pool pin count or epoch-based tracking gates final page reuse.

### Metrics

- `columnstore_gc_entries_removed`
- `columnstore_gc_segments_scanned`
- `columnstore_gc_segments_rewritten`
- `columnstore_gc_bytes_reclaimed`
- `columnstore_gc_dead_ratio`

---

## LSM-Tree Index GC (Full Design)

### Scope

Applies to the disk-based `LSMTreeIndex`. GC must:

- Remove entries whose TIDs are dead (heap sweep driven)
- Drop tombstones with `xmax < OIT`
- Preserve newest visible version per key
- Operate via compaction to avoid blocking readers

### GC Entry Semantics

LSM entries contain:

- `key`
- `value` (encodes TID)
- `entry_type` (INSERT or DELETE)
- `xmin`, `xmax`, `sequence_number`

Visibility is determined by `TransactionManager` (TIP + OIT).

### GC Pipeline

1. **Memtable purge**:
   - Remove entries with dead TIDs.
   - Remove tombstones with `xmax < OIT` if no older visible versions remain.
2. **SSTable filtering** (compaction-based):
   - Add a `dead_tid_filter` to compaction tasks.
   - Merge iterators drop:
     - INSERT entries whose value TID is dead.
     - DELETE entries whose `xmax < OIT` and no shadowed versions exist.
3. **Manifest swap**:
   - Replace compacted SSTables atomically.
   - Delete old SSTables after swap succeeds.

### SSTable Metadata for Efficient GC

To avoid full scans on every GC pass, each SSTable must store:

- `tid_bloom_filter` (Bloom filter over value TIDs)
- `tid_count`
- `tid_min` / `tid_max` (for coarse overlap checks)

Compaction skips SSTables whose `tid_bloom_filter` shows no overlap with
the dead TID set.

### Concurrency and Scheduling

- GC compactions run in `LSMThreadPool`.
- Range conflicts are avoided using existing active-range tracking.
- Normal compactions and GC compactions may coalesce.

### Metrics

- `lsm_gc_entries_removed`
- `lsm_gc_tombstones_dropped`
- `lsm_gc_sstables_rewritten`
- `lsm_gc_bytes_reclaimed`
- `lsm_gc_compactions`

---

## Summary

### Protocol Overview

1. **Heap Sweep**: Identifies dead tuples via OIT, collects dead TIDs
2. **Index Cleanup**: Calls removeDeadEntries() on each index
3. **Bulk Removal**: Indexes process entire batch efficiently
4. **Statistics**: Track entries removed, pages modified
5. **Error Handling**: Best effort, log failures, continue

### Key Design Principles

- ✅ Heap-driven (OIT-based dead tuple identification)
- ✅ Bulk operations (efficient batch processing)
- ✅ Eventual consistency (index cleanup may lag)
- ✅ Non-blocking (page-level locks only)
- ✅ Best effort (partial failures OK)

### Integration Points

- **HeapPage**: Add collectDeadTuples() method
- **GarbageCollector**: Add cleanIndexes() method
- **Indexes**: Implement IndexGCInterface
- **CatalogManager**: Provide getIndexesForTable() method

---

## Appendix A: Implementation Checklist

### Columnstore GC

- [ ] Extend on-disk segment metadata to store TID map pointer and live/dead counters.
- [ ] Add TID map page format (GPID+slot entries) and checksum handling.
- [ ] Add visibility bitmap separate from NULL bitmap.
- [ ] Implement segment prefilter using min/max TID bounds.
- [ ] Implement `ColumnstoreIndex::removeDeadEntries()` using TID map + bitmap.
- [ ] Implement segment rewrite (copy-on-write) when dead_ratio exceeds threshold.
- [ ] Add GC metrics counters for columnstore.

### LSM-Tree GC

- [ ] Extend SSTable metadata with TID bloom, min/max TID, and count.
- [ ] Add dead TID filter plumbing to compaction tasks.
- [ ] Filter INSERT entries whose value TID is dead during merge.
- [ ] Drop tombstones with `xmax < OIT` when no shadowed versions remain.
- [ ] Implement `LSMTreeIndex::removeDeadEntries()` to trigger GC compaction.
- [ ] Add GC metrics counters for LSM.

### GC Integration

- [ ] Ensure GC uses `TID` (GPID+slot) throughout, no legacy packing.
- [ ] Verify GC logging includes index type + entries removed.
- [ ] Add integration tests for columnstore/LSM GC with dead TIDs.

---

**Document Version**: 1.1
**Last Updated**: 2026-01-27
**Status**: Protocol Defined - Ready for Implementation
