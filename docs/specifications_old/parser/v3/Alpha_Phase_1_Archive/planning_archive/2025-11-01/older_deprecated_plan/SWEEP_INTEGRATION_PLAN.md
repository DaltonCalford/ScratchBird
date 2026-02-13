# Sweep Integration Plan for Index GC

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 18, 2025
**Task**: Phase 2 Task 2.1.3 - Plan Integration with Existing Sweep
**Status**: Planning Complete

---

## Overview

This document details how to integrate index garbage collection with the existing heap sweep process in `garbage_collector.cpp`.

---

## Current Sweep Architecture

### Flow

**File**: `src/core/garbage_collector.cpp`

**Entry Points**:
1. **Cooperative GC**: `processPageCooperative(page_id)` - Called during page reads
2. **Background GC**: `backgroundGCLoop()` - Runs periodically in background thread

**Core Method**: `cleanPage(uint32_t page_id, uint64_t *space_reclaimed_out, ErrorContext *ctx)`

### Current cleanPage Implementation

```cpp
uint64_t GarbageCollector::cleanPage(uint32_t page_id, ...)
{
    // 1. Get OIT (Oldest Interesting Transaction)
    uint64_t oit = txn_manager_->getOldestXid();

    // 2. Pin the page through buffer pool
    void *page_buffer;
    Status s = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);

    // 3. Get page header to check page type
    auto *page_header = reinterpret_cast<PageHeader *>(page_buffer);

    // 4. Only process heap pages
    if (page_header->page_type != PAGE_TYPE_HEAP)
    {
        db_->buffer_pool()->unpinPage(page_id, false, ctx);
        return 0;
    }

    // 5. Use HeapPage::prunePage() for physical tuple removal
    HeapPage heap_page(page_buffer, page_size);
    uint32_t tuples_pruned = 0;
    uint32_t space_reclaimed = 0;
    Status prune_status = heap_page.prunePage(oit, &tuples_pruned, &space_reclaimed, ctx);

    // 6. Unpin page (mark as dirty if we modified it)
    db_->buffer_pool()->unpinPage(page_id, tuples_pruned > 0, ctx);

    // 7. Log and update statistics
    if (tuples_pruned > 0)
    {
        LOG_INFO(VACUUM, "Page %u: pruned %u tuples, reclaimed %u bytes (OIT=%lu)",
                 page_id, tuples_pruned, space_reclaimed, oit);
    }

    return tuples_pruned;
}
```

---

## Integration Points

### 1. Add HeapPage::collectDeadTuples()

**Location**: `src/core/heap_page.cpp`

**Purpose**: Collect TIDs of dead tuples BEFORE pruning (so we can tell indexes which entries to remove)

**Implementation**:
```cpp
auto HeapPage::collectDeadTuples(uint64_t oit,
                                 std::vector<uint64_t> *dead_tids_out,
                                 ErrorContext *ctx) -> Status
{
    ItemPointer *items = getItemArray();
    uint16_t item_count = header()->item_count;
    uint32_t page_id = header()->page_id;

    // Clear output vector
    dead_tids_out->clear();

    // Scan all tuples and collect dead TIDs
    for (uint16_t item_id = 0; item_id < item_count; item_id++)
    {
        // Skip already unused items
        if (items[item_id].isUnused())
        {
            continue;
        }

        // Skip deleted items
        if (items[item_id].isDeleted())
        {
            continue;
        }

        // Validate item pointer bounds
        if (!items[item_id].isValid(page_size_))
        {
            continue; // Skip corrupt items
        }

        // Get tuple header
        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(
            page_data_ + items[item_id].offset);

        // Check if tuple is garbage (SAME LOGIC AS prunePage)
        if (tuple_hdr->xmax != 0 && tuple_hdr->xmax < oit)
        {
            // Check if XMAX_COMMITTED flag is set
            if ((tuple_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0)
            {
                // Tuple is dead - add TID to output
                uint64_t tid = (static_cast<uint64_t>(page_id) << 32) | item_id;
                dead_tids_out->push_back(tid);
            }
        }
    }

    return Status::OK;
}
```

**Add to Header**: `include/scratchbird/core/heap_page.h`
```cpp
// Collect TIDs of dead tuples (for index cleanup)
// Call BEFORE prunePage() to get list of TIDs that will be pruned
// Returns vector of TIDs for tuples that are dead (xmax < OIT and committed)
auto collectDeadTuples(uint64_t oit,
                       std::vector<uint64_t> *dead_tids_out,
                       ErrorContext *ctx = nullptr) -> Status;
```

### 2. Add GarbageCollector::cleanIndexes()

**Location**: `src/core/garbage_collector.cpp`

**Purpose**: Call removeDeadEntries() on all indexes for a table

**Implementation**:
```cpp
void GarbageCollector::cleanIndexes(uint32_t page_id,
                                    const std::vector<uint64_t>& dead_tids,
                                    ErrorContext *ctx)
{
    // 1. Get table ID from page
    // NOTE: We need a way to map page_id -> table_id
    // Option A: Store in page header (PAGE_TYPE_HEAP + table_id)
    // Option B: Maintain mapping in catalog/storage_engine
    // Option C: For now, skip if we can't determine table

    // Pin page to get table ID (read page header)
    void *page_buffer = nullptr;
    Status pin_status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
    if (pin_status != Status::OK)
    {
        LOG_WARNING(VACUUM, "Failed to pin page %u for index GC: %d",
                    page_id, static_cast<int>(pin_status));
        return;
    }

    auto *page_header = reinterpret_cast<PageHeader *>(page_buffer);

    // TODO: Need table_id in PageHeader or use catalog lookup
    // For now, we'll need to enhance PageHeader or use catalog
    uint32_t table_id = 0; // PLACEHOLDER - needs implementation

    db_->buffer_pool()->unpinPage(page_id, false, ctx);

    if (table_id == 0)
    {
        // Can't determine table, skip index cleanup
        return;
    }

    // 2. Get all indexes for this table
    CatalogManager *catalog = db_->catalog_manager();
    std::vector<CatalogManager::IndexInfo> indexes;

    // TODO: Need CatalogManager::getIndexesForTable()
    // Status status = catalog->getIndexesForTable(table_id, indexes, ctx);

    // 3. Clean each index
    for (const auto& index_info : indexes)
    {
        // Get index instance
        // TODO: Need way to get index object from index_id
        // For now, we'll add this in Task 2.6 when we have index implementations

        IndexGCInterface *index = nullptr; // PLACEHOLDER

        if (!index)
        {
            LOG_WARNING(VACUUM, "Failed to get index %u for table %u",
                        index_info.index_id, table_id);
            continue;
        }

        uint64_t entries_removed = 0;
        uint64_t pages_modified = 0;

        Status status = index->removeDeadEntries(
            dead_tids, &entries_removed, &pages_modified, ctx);

        if (status == Status::OK)
        {
            LOG_INFO(VACUUM, "Index %s (ID %u, table %u): removed %lu entries from %lu pages",
                     index->indexTypeName(), index_info.index_id, table_id,
                     entries_removed, pages_modified);

            // Update statistics
            std::lock_guard<std::mutex> lock(stats_mutex_);
            // TODO: Add index GC stats to GCStatistics struct
        }
        else if (status == Status::PARTIAL_FAILURE)
        {
            LOG_WARNING(VACUUM, "Index %s (ID %u, table %u): partial GC, removed %lu entries",
                        index->indexTypeName(), index_info.index_id, table_id,
                        entries_removed);
        }
        else
        {
            LOG_WARNING(VACUUM, "Index %s (ID %u, table %u): GC failed with status %d",
                        index->indexTypeName(), index_info.index_id, table_id,
                        static_cast<int>(status));
        }
    }
}
```

**Add to Header**: `include/scratchbird/core/garbage_collector.h`
```cpp
private:
    // ... existing methods ...

    // Clean index entries for dead tuples
    void cleanIndexes(uint32_t page_id,
                      const std::vector<uint64_t>& dead_tids,
                      ErrorContext *ctx);
```

### 3. Modify GarbageCollector::cleanPage()

**Location**: `src/core/garbage_collector.cpp`

**Changes**: Add index cleanup after heap pruning

**New Implementation**:
```cpp
uint64_t GarbageCollector::cleanPage(uint32_t page_id,
                                     uint64_t *space_reclaimed_out,
                                     ErrorContext *ctx)
{
    // 1. Get OIT (Oldest Interesting Transaction)
    uint64_t oit = txn_manager_->getOldestXid();

    // 2. Pin the page through buffer pool
    void *page_buffer;
    Status s = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
    if (s != Status::OK)
    {
        LOG_WARNING(VACUUM, "Failed to pin page %u for GC: %d", page_id, static_cast<int>(s));
        if (space_reclaimed_out != nullptr)
        {
            *space_reclaimed_out = 0;
        }
        return 0;
    }

    // 3. Get page header to check page type
    auto *page_header = reinterpret_cast<PageHeader *>(page_buffer);

    // 4. Only process heap pages
    if (page_header->page_type != PAGE_TYPE_HEAP)
    {
        db_->buffer_pool()->unpinPage(page_id, false, ctx);
        if (space_reclaimed_out != nullptr)
        {
            *space_reclaimed_out = 0;
        }
        return 0;
    }

    // 5. NEW: Collect dead TIDs BEFORE pruning
    HeapPage heap_page(reinterpret_cast<uint8_t *>(page_buffer), page_header->page_size);
    std::vector<uint64_t> dead_tids;
    Status collect_status = heap_page.collectDeadTuples(oit, &dead_tids, ctx);

    if (collect_status != Status::OK)
    {
        LOG_WARNING(VACUUM, "Failed to collect dead TIDs for page %u: %d",
                    page_id, static_cast<int>(collect_status));
        // Continue anyway - we can still prune the heap
    }

    // 6. Prune dead tuples from heap
    uint32_t tuples_pruned = 0;
    uint32_t space_reclaimed = 0;
    Status prune_status = heap_page.prunePage(oit, &tuples_pruned, &space_reclaimed, ctx);

    bool page_modified = (tuples_pruned > 0);

    // 7. Log heap cleanup results
    if (tuples_pruned > 0)
    {
        LOG_INFO(VACUUM, "Page %u: pruned %u tuples, reclaimed %u bytes (OIT=%lu)",
                 page_id, tuples_pruned, space_reclaimed, oit);
    }

    // 8. Unpin heap page (mark as dirty if we modified it)
    db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);

    // 9. NEW: Clean indexes for dead tuples
    if (!dead_tids.empty())
    {
        LOG_DEBUG(VACUUM, "Page %u: cleaning %zu dead entries from indexes",
                  page_id, dead_tids.size());

        cleanIndexes(page_id, dead_tids, ctx);
    }

    // 10. Return statistics
    if (space_reclaimed_out != nullptr)
    {
        *space_reclaimed_out = space_reclaimed;
    }

    // Remove from dirty pages
    {
        std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
        dirty_pages_.erase(page_id);
    }

    return tuples_pruned;
}
```

---

## Missing Components (To Be Added)

### 1. Table ID in Page Header

**Problem**: Need to map `page_id` → `table_id` to get indexes

**Options**:

**Option A**: Add `table_id` to PageHeader
```cpp
struct PageHeader
{
    uint32_t magic;
    uint32_t page_id;
    uint32_t page_size;
    uint16_t page_type;
    uint16_t reserved;
    uint32_t table_id;  // NEW: Table this page belongs to
    // ... rest of fields
};
```

**Option B**: Use CatalogManager to lookup table by page_id
```cpp
// Add method to CatalogManager
Status getTableIdForPage(uint32_t page_id, uint32_t *table_id_out, ErrorContext *ctx);
```

**Option C**: Use StorageEngine to track page → table mapping
```cpp
// StorageEngine already tracks table metadata
uint32_t table_id = storage_engine_->getTableIdForPage(page_id);
```

**Recommendation**: Option C (use StorageEngine) - least invasive, leverages existing metadata

### 2. CatalogManager::getIndexesForTable()

**Purpose**: Get all indexes for a given table

**Signature**:
```cpp
// Add to CatalogManager
Status getIndexesForTable(uint32_t table_id,
                          std::vector<IndexInfo> *indexes_out,
                          ErrorContext *ctx = nullptr);
```

**Implementation**: Query catalog tables for indexes where `index.table_id == table_id`

### 3. Index Instance Management

**Problem**: Need to get `IndexGCInterface*` from `index_id`

**Options**:

**Option A**: StorageEngine maintains open index cache
```cpp
class StorageEngine
{
    std::unordered_map<uint32_t, IndexGCInterface*> index_cache_;

public:
    IndexGCInterface* getIndex(uint32_t index_id);
};
```

**Option B**: CatalogManager instantiates indexes on-demand
```cpp
Status CatalogManager::openIndex(uint32_t index_id,
                                  IndexGCInterface **index_out,
                                  ErrorContext *ctx);
```

**Option C**: GarbageCollector caches index instances
```cpp
class GarbageCollector
{
    std::unordered_map<uint32_t, std::unique_ptr<IndexGCInterface>> index_cache_;
};
```

**Recommendation**: Option A (StorageEngine cache) - indexes already managed by StorageEngine

### 4. Enhanced GCStatistics

**Add index GC metrics**:
```cpp
struct GCStatistics
{
    // ... existing fields ...

    // NEW: Index GC statistics
    uint64_t index_entries_removed;    // Total index entries removed
    uint64_t index_pages_modified;     // Total index pages modified
    uint64_t index_cleanup_calls;      // Number of cleanIndexes() calls
    uint64_t index_cleanup_failures;   // Number of failed index cleanups
};
```

---

## Implementation Sequence

### Task 2.1: Define Index GC Protocol ✅ COMPLETE
- [x] Create IndexGCInterface header
- [x] Document INDEX_GC_PROTOCOL.md
- [x] Plan sweep integration (this document)

### Task 2.2-2.5: Implement Index removeDeadEntries() (Next)
- [ ] B-Tree implementation
- [ ] Hash Index implementation
- [ ] GIN Index implementation
- [ ] Bitmap Index implementation

### Task 2.6: Integrate with Heap Sweep (Final)
- [ ] Add HeapPage::collectDeadTuples()
- [ ] Add GarbageCollector::cleanIndexes()
- [ ] Modify GarbageCollector::cleanPage()
- [ ] Add CatalogManager::getIndexesForTable()
- [ ] Add StorageEngine::getTableIdForPage()
- [ ] Add StorageEngine::getIndex()
- [ ] Add index GC statistics to GCStatistics
- [ ] Integration tests

---

## Call Points Summary

**When does index GC run?**

1. **Cooperative GC**: During normal page reads (rate-limited)
   ```
   User Query → BufferPool::getPage()
              → GarbageCollector::processPageCooperative()
              → cleanPage()
              → cleanIndexes()
   ```

2. **Background GC**: Periodic background thread
   ```
   Background Thread → backgroundGCLoop()
                    → cleanPage()
                    → cleanIndexes()
   ```

3. **Explicit VACUUM**: User-triggered cleanup
   ```
   VACUUM command → manually trigger GC
                  → cleanPage()
                  → cleanIndexes()
   ```

---

## Performance Expectations

**Overhead per Page**:
- collectDeadTuples(): ~0.1-0.5ms (scan heap page)
- cleanIndexes() per index: ~1-10ms (depending on dead entry count)
- Total overhead: ~5-50ms per heap page with indexes

**Batching**:
- Typical: 10-100 dead tuples per heap page
- Indexes process entire batch at once (efficient)

**I/O Amplification**:
- Heap: 1 page read + 1 page write (if modified)
- Indexes: M pages read + N pages write (where M = pages scanned, N = pages modified)
- Typical: 2-5x I/O amplification for indexed tables

**Acceptable**: Index GC is background work, doesn't block foreground queries.

---

## Summary

### Integration Plan

1. ✅ Add `HeapPage::collectDeadTuples()` to collect dead TIDs before pruning
2. ✅ Add `GarbageCollector::cleanIndexes()` to call index GC
3. ✅ Modify `GarbageCollector::cleanPage()` to call cleanIndexes()
4. ⏸️ Add helper methods (getIndexesForTable, getTableIdForPage, getIndex)
5. ⏸️ Add index GC statistics

### Dependencies

- **Task 2.2-2.5**: Index implementations must provide removeDeadEntries()
- **CatalogManager**: Must provide getIndexesForTable()
- **StorageEngine**: Must provide getTableIdForPage() and getIndex()

### Ready to Proceed

Phase 2 Task 2.1 (Define Index GC Protocol) is **COMPLETE**. Ready to implement Task 2.2-2.5 (index-specific removeDeadEntries()).

---

**Document Version**: 1.0
**Last Updated**: October 18, 2025
**Status**: Planning Complete
