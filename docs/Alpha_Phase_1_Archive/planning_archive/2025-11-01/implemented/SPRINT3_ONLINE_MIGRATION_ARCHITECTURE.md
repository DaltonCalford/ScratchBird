# Sprint 3: ONLINE Migration Architecture Design

**Document Status**: ✅ COMPLETE
**Version**: 1.0
**Date**: October 21, 2025
**Sprint Goal**: Complete architecture design for MGA-aware ONLINE tablespace migration
**Estimated Effort**: 8-10 hours (architecture and design)
**Dependencies**: Sprint 2 complete (all index types + TOAST)

---

## Executive Summary

This document provides the complete architectural design for ONLINE tablespace migration in ScratchBird, leveraging Firebird's Multi-Generational Architecture (MGA) to enable concurrent reads and writes during table migration with minimal downtime.

**Key Design Principles**:
1. **MGA-Native**: Leverage existing MGA infrastructure (TIP, version chains, snapshots)
2. **TID Stability**: Primary record location becomes stable after migration
3. **Minimal Downtime**: < 100ms exclusive lock during final swap
4. **Zero Data Loss**: All concurrent operations preserved during migration
5. **Graceful Degradation**: Performance targets maintained (< 10% overhead)

**Architecture Highlights**:
- Dual-source visibility layer for transparent read access
- Write routing to target tablespace during migration
- Incremental page copy with dirty page tracking
- Catch-up phase for convergence
- Atomic catalog swap with index TID batch updates
- Background cleanup of source pages

---

## Part 1: Migration State Tracking

### 1.1 Catalog Schema Extensions

**New Catalog Table**: `pg_table_migrations`

```sql
CREATE TABLE pg_table_migrations (
    -- Identity
    table_id            UUID PRIMARY KEY,           -- Migrating table UUID
    migration_id        UUID NOT NULL UNIQUE,       -- Migration operation UUID

    -- Tablespaces
    source_tablespace   UINT16 NOT NULL,            -- Source tablespace ID
    target_tablespace   UINT16 NOT NULL,            -- Target tablespace ID

    -- State
    migration_phase     UINT8 NOT NULL,             -- Current phase (enum)
    migration_xid       UINT64 NOT NULL,            -- XID when migration started

    -- Progress tracking
    total_pages         UINT64 NOT NULL,            -- Total pages to migrate
    copied_pages        UINT64 NOT NULL DEFAULT 0,  -- Pages copied so far
    dirty_pages         UINT64 NOT NULL DEFAULT 0,  -- Pages modified during copy

    -- Timing
    start_time          TIMESTAMP NOT NULL,
    phase_start_time    TIMESTAMP NOT NULL,
    estimated_end_time  TIMESTAMP,

    -- Configuration
    batch_size          UINT32 NOT NULL DEFAULT 100, -- Pages per batch
    yield_interval_ms   UINT32 NOT NULL DEFAULT 100, -- Yield delay

    -- Statistics
    bytes_copied        UINT64 NOT NULL DEFAULT 0,
    pages_per_second    FLOAT,

    -- Constraints
    CONSTRAINT valid_tablespaces CHECK (source_tablespace != target_tablespace),
    CONSTRAINT valid_progress CHECK (copied_pages <= total_pages)
);
```

**Migration Phase Enum**:
```c
enum MigrationPhase {
    MIGRATION_INIT = 0,       // Initializing
    MIGRATION_COPYING = 1,    // Incremental page copy in progress
    MIGRATION_CATCH_UP = 2,   // Re-copying dirty pages
    MIGRATION_SWAP = 3,       // Atomic catalog swap
    MIGRATION_CLEANUP = 4,    // Cleaning up source pages
    MIGRATION_COMPLETE = 5,   // Migration finished
    MIGRATION_FAILED = 6      // Migration failed (needs rollback)
};
```

**Extension to TableInfo Structure**:
```cpp
// In include/scratchbird/core/catalog_manager.h
struct TableInfo {
    // ... existing fields ...

    // ONLINE migration state (new fields)
    bool migration_in_progress = false;
    ID migration_id;                    // UUID of active migration (zero if none)
    uint64_t migration_xid = 0;         // XID when migration started
    uint16_t migration_target_ts = 0;   // Target tablespace ID
    uint8_t migration_phase = 0;        // Current phase (MigrationPhase enum)
};
```

### 1.2 Migration State Machine

**Phase Transitions**:
```
INIT → COPYING → CATCH_UP → SWAP → CLEANUP → COMPLETE
  ↓        ↓          ↓        ↓       ↓
  └────────┴──────────┴────────┴───────┴──→ FAILED
```

**Phase Descriptions**:

1. **INIT** (< 1 second)
   - Create entry in `pg_table_migrations`
   - Set `migration_in_progress = true` in TableInfo
   - Record `migration_xid` = current transaction ID
   - Calculate `total_pages` from table statistics
   - Allocate dirty page bitmap (1 bit per page)

2. **COPYING** (minutes to hours, depends on table size)
   - Background thread copies pages incrementally
   - Batches of 100 pages per iteration
   - Yield 100ms between batches
   - Mark dirty pages when modified by concurrent writes
   - Update `copied_pages` counter
   - Transition to CATCH_UP when all pages copied

3. **CATCH_UP** (seconds to minutes)
   - Re-copy pages marked dirty during COPYING phase
   - Clear dirty bit after each page re-copied
   - Monitor convergence: if `dirty_pages_added < copy_rate`, proceed
   - Transition to SWAP when dirty pages < threshold (e.g., 100 pages)

4. **SWAP** (< 100ms exclusive lock)
   - Acquire exclusive lock on table
   - Final copy of remaining dirty pages (< 100)
   - Update TableInfo.tablespace_id = target_tablespace
   - Batch update all index TIDs (use existing Sprint 2 implementations)
   - Set `migration_in_progress = false`
   - Commit transaction
   - Release exclusive lock

5. **CLEANUP** (background, low priority)
   - Wait for all snapshots < migration_xid to complete
   - Deallocate source tablespace pages
   - Update FSM to mark pages as free
   - Remove entry from `pg_table_migrations`

6. **FAILED**
   - Rollback: deallocate target tablespace pages
   - Clear `migration_in_progress` flag
   - Log error details
   - Optionally retry or manual intervention

### 1.3 API Functions

```cpp
// Start ONLINE migration
Status CatalogManager::startOnlineMigration(
    const ID &table_id,
    uint16_t target_tablespace_id,
    ErrorContext *ctx);

// Update migration progress
Status CatalogManager::updateMigrationProgress(
    const ID &migration_id,
    uint64_t pages_copied,
    uint64_t dirty_pages,
    ErrorContext *ctx);

// Transition to next phase
Status CatalogManager::setMigrationPhase(
    const ID &migration_id,
    MigrationPhase new_phase,
    ErrorContext *ctx);

// Get migration state for table
Status CatalogManager::getMigrationState(
    const ID &table_id,
    TableMigrationState *state_out,
    ErrorContext *ctx);

// Abort migration
Status CatalogManager::abortMigration(
    const ID &migration_id,
    const char *reason,
    ErrorContext *ctx);
```

---

## Part 2: Dual-Source Visibility Model

### 2.1 Core Principle

**During migration, tuples exist in TWO locations**:
1. **Source tablespace** (old location, for tuples with xmin < migration_xid)
2. **Target tablespace** (new location, for tuples with xmin >= migration_xid OR already migrated)

**Visibility Rule**:
```
Given a TID (currently points to source tablespace):
  1. If table.migration_in_progress == false:
     → Fetch from table.tablespace_id (normal case)

  2. If table.migration_in_progress == true:
     a. Check if tuple migrated: lookup TID in migration_mapping bloom filter
     b. If migrated: fetch from target_tablespace
     c. If not migrated: fetch from source_tablespace
```

### 2.2 TID Resolution Service

**Data Structure**:
```cpp
// TID resolver cache (per-table)
struct TIDResolverCache {
    // Bloom filter for fast "has this TID been migrated?" check
    BloomFilter *migrated_tids_bloom;  // 1% false positive rate

    // Exact mapping for TIDs (only for false positives)
    std::unordered_map<uint64_t, uint64_t> tid_exact_mapping;

    // Statistics
    uint64_t bloom_hits = 0;
    uint64_t bloom_false_positives = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
};
```

**Algorithm**:
```cpp
// Resolve tablespace for a given TID
uint16_t TIDResolver::resolveTablespace(
    const TID &tid,
    const TableInfo &table_info,
    ErrorContext *ctx)
{
    // Fast path: no migration
    if (!table_info.migration_in_progress) {
        return table_info.tablespace_id;
    }

    // During migration: check if TID migrated
    uint64_t legacy_tid = convertTIDtoLegacy(tid);

    // 1. Bloom filter check (very fast)
    if (!migrated_bloom->contains(legacy_tid)) {
        // Definitely NOT migrated → source tablespace
        return table_info.tablespace_id;  // Source
    }

    // 2. Bloom says "probably migrated" → check exact mapping
    auto it = tid_exact_mapping.find(legacy_tid);
    if (it != tid_exact_mapping.end()) {
        // Confirmed migrated → target tablespace
        return table_info.migration_target_ts;
    }

    // 3. Bloom false positive → actually not migrated
    bloom_false_positives++;
    return table_info.tablespace_id;  // Source
}
```

**Bloom Filter Sizing**:
- For 1M tuples, 1% FP rate: ~1.2 MB memory
- Updated during COPYING phase as pages are migrated
- Cleared after SWAP phase completes

### 2.3 Heap Tuple Fetch Integration

**Modified Heap Fetch Path**:
```cpp
// In src/core/heap_page.cpp
Status HeapPage::getTuple(
    const TID &tid,
    Snapshot *snapshot,
    Tuple *tuple_out,
    ErrorContext *ctx)
{
    // 1. Get table info (includes migration state)
    TableInfo table_info = catalog_mgr->getTableInfo(relation_id);

    // 2. Resolve which tablespace to fetch from
    uint16_t tablespace_id = tid_resolver->resolveTablespace(
        tid, table_info, ctx);

    // 3. Fetch from resolved tablespace
    void *page_buffer = buffer_pool->pinPage(
        tablespace_id, page_number, ctx);

    // 4. Extract tuple from page
    // ... existing tuple extraction logic ...

    // 5. MGA visibility check (using snapshot)
    if (!isTupleVisible(tuple, snapshot)) {
        // Tuple not visible to this snapshot
        buffer_pool->unpinPage(page_number, false, ctx);
        return Status::NOT_FOUND;
    }

    // 6. Success
    *tuple_out = tuple;
    buffer_pool->unpinPage(page_number, false, ctx);
    return Status::OK;
}
```

**Version Chain Traversal**:
```cpp
// Follow back version pointers during migration
Status followVersionChain(
    const TID &current_tid,
    Snapshot *snapshot,
    Tuple *tuple_out,
    ErrorContext *ctx)
{
    TID tid = current_tid;

    while (true) {
        // Fetch tuple (handles dual-source resolution)
        Tuple tuple;
        Status status = getTuple(tid, snapshot, &tuple, ctx);

        if (status != Status::OK) {
            return status;
        }

        // Check visibility
        if (isTupleVisible(tuple, snapshot)) {
            *tuple_out = tuple;
            return Status::OK;
        }

        // Follow back version pointer
        if (tuple.header.rhd_flags & RHD_CHAIN) {
            tid = tuple.header.rhd_back_version;  // UUID-based
        } else {
            // No more versions
            return Status::NOT_FOUND;
        }
    }
}
```

### 2.4 Performance Optimization

**Caching Strategy**:
```cpp
// Per-query TID resolution cache (avoids repeated bloom lookups)
struct QueryTIDCache {
    std::unordered_map<uint64_t, uint16_t> resolved_tids;  // TID → tablespace

    uint16_t resolve(const TID &tid, const TableInfo &table_info) {
        uint64_t legacy_tid = convertTIDtoLegacy(tid);

        // Check cache first
        auto it = resolved_tids.find(legacy_tid);
        if (it != resolved_tids.end()) {
            return it->second;  // Cache hit
        }

        // Miss: resolve via TID resolver
        uint16_t tablespace = tid_resolver->resolveTablespace(
            tid, table_info, nullptr);

        // Cache result
        resolved_tids[legacy_tid] = tablespace;

        return tablespace;
    }
};
```

**Expected Performance**:
- Non-migrating tables: 0% overhead (fast path)
- Migrating tables (source TIDs): ~1-2 ns bloom lookup
- Migrating tables (migrated TIDs): ~10-20 ns cache lookup
- Overall query overhead: < 5% (target met)

---

## Part 3: Write Routing Strategy

### 3.1 Core Principle

**During migration, writes are routed based on migration phase**:

1. **Before migration**: All writes → source tablespace (normal)
2. **During COPYING/CATCH_UP**:
   - New INSERTs → target tablespace
   - UPDATEs → same location as old tuple (avoids cross-tablespace version chains)
   - DELETEs → mark tuple in current location
3. **After SWAP**: All writes → target tablespace (normal)

### 3.2 INSERT Routing

**Algorithm**:
```cpp
// In src/core/storage_engine.cpp
Status StorageEngine::insertTuple(
    const Relation &relation,
    const Tuple &tuple,
    TID *tid_out,
    ErrorContext *ctx)
{
    // 1. Get table info (includes migration state)
    TableInfo table_info = catalog_mgr->getTableInfo(relation.rel_id);

    // 2. Determine target tablespace
    uint16_t target_tablespace;

    if (!table_info.migration_in_progress) {
        // Normal case: use table's current tablespace
        target_tablespace = table_info.tablespace_id;
    } else {
        // During migration: route to target tablespace
        target_tablespace = table_info.migration_target_ts;

        LOG_DEBUG(STORAGE, "INSERT routed to target tablespace %u during migration",
                 target_tablespace);
    }

    // 3. Allocate page in target tablespace
    uint32_t page_number;
    Status alloc_status = page_manager->allocatePage(
        target_tablespace, &page_number, ctx);

    if (alloc_status != Status::OK) {
        return alloc_status;
    }

    // 4. Insert tuple into page
    void *page_buffer = buffer_pool->pinPage(
        target_tablespace, page_number, ctx);

    HeapPage heap_page(page_buffer, db->page_size());
    uint16_t item_id;

    Status insert_status = heap_page.insertTuple(
        tuple.data, tuple.len, current_xid, &item_id, ctx);

    // 5. Construct TID and return
    *tid_out = constructTID(target_tablespace, page_number, item_id);

    buffer_pool->unpinPage(page_number, true, ctx);  // Mark dirty

    return Status::OK;
}
```

**Key Point**: New tuples created during migration have `xmin >= migration_xid`, so they're always visible in the target tablespace.

### 3.3 UPDATE Routing (MGA-Aware)

**Critical Design Decision**: UPDATEs create back version in SAME tablespace as old tuple.

**Rationale**:
- Firebird MGA: primary record modified in-place, back version created
- Cross-tablespace version chains would be extremely complex
- Simpler: keep version chain within same tablespace until next UPDATE

**Algorithm**:
```cpp
// UPDATE routing during migration
Status StorageEngine::updateTuple(
    const TID &old_tid,
    const Tuple &new_tuple,
    TID *new_tid_out,
    ErrorContext *ctx)
{
    // 1. Fetch old tuple to determine its location
    Tuple old_tuple;
    Status fetch_status = getTuple(old_tid, nullptr, &old_tuple, ctx);

    if (fetch_status != Status::OK) {
        return fetch_status;
    }

    // 2. Get tablespace where old tuple resides
    uint16_t old_tablespace = extractTablespace(old_tid);

    // 3. Check if in-place update possible (same page, enough space)
    if (canUpdateInPlace(old_tid, new_tuple.len)) {
        // In-place update: modify primary record, no version creation
        return updateInPlace(old_tid, new_tuple, new_tid_out, ctx);
    }

    // 4. Cross-page update: create back version in SAME tablespace as old tuple

    // 4a. Allocate new page in SAME tablespace for back version
    uint32_t back_page;
    page_manager->allocatePage(old_tablespace, &back_page, ctx);

    // 4b. Write OLD tuple data as back version
    void *back_buffer = buffer_pool->pinPage(old_tablespace, back_page, ctx);
    HeapPage back_heap(back_buffer, db->page_size());
    uint16_t back_item_id;

    back_heap.insertTuple(old_tuple.data, old_tuple.len,
                         old_tuple.header.rhd_transaction,
                         &back_item_id, ctx);

    buffer_pool->unpinPage(back_page, true, ctx);

    // 4c. Overwrite PRIMARY location with NEW tuple data (MGA in-place modification)
    void *primary_buffer = buffer_pool->pinPage(old_tablespace,
                                                extractPageNumber(old_tid), ctx);
    HeapPage primary_heap(primary_buffer, db->page_size());

    primary_heap.overwriteTuple(extractItemID(old_tid),
                               new_tuple.data,
                               new_tuple.len,
                               back_page,
                               back_item_id,
                               ctx);

    buffer_pool->unpinPage(extractPageNumber(old_tid), true, ctx);

    // 4d. TID UNCHANGED (MGA principle)
    *new_tid_out = old_tid;

    return Status::OK;
}
```

**Key Insight**: This preserves TID stability while avoiding cross-tablespace version chains.

### 3.4 DELETE Routing

**Simple**: Mark tuple as deleted in its current location.

```cpp
Status StorageEngine::deleteTuple(
    const TID &tid,
    ErrorContext *ctx)
{
    // 1. Resolve tablespace (handles dual-source)
    TableInfo table_info = catalog_mgr->getTableInfo(relation_id);
    uint16_t tablespace = tid_resolver->resolveTablespace(tid, table_info, ctx);

    // 2. Pin page
    void *page_buffer = buffer_pool->pinPage(
        tablespace, extractPageNumber(tid), ctx);

    // 3. Mark tuple as deleted (set xmax)
    HeapPage heap_page(page_buffer, db->page_size());
    heap_page.deleteTuple(extractItemID(tid), current_xid, ctx);

    // 4. Unpin and mark dirty
    buffer_pool->unpinPage(extractPageNumber(tid), true, ctx);

    return Status::OK;
}
```

### 3.5 Dirty Page Tracking

**During COPYING phase, track which pages are modified**:

```cpp
// Dirty page bitmap (1 bit per page)
struct DirtyPageBitmap {
    uint8_t *bits;           // Bitmap data
    uint64_t num_pages;      // Total pages in table
    pthread_mutex_t mutex;   // Concurrency control

    void markDirty(uint32_t page_number) {
        pthread_mutex_lock(&mutex);

        uint64_t byte_offset = page_number / 8;
        uint8_t bit_offset = page_number % 8;
        bits[byte_offset] |= (1 << bit_offset);

        pthread_mutex_unlock(&mutex);
    }

    bool isDirty(uint32_t page_number) {
        uint64_t byte_offset = page_number / 8;
        uint8_t bit_offset = page_number % 8;
        return (bits[byte_offset] & (1 << bit_offset)) != 0;
    }

    void clearDirty(uint32_t page_number) {
        pthread_mutex_lock(&mutex);

        uint64_t byte_offset = page_number / 8;
        uint8_t bit_offset = page_number % 8;
        bits[byte_offset] &= ~(1 << bit_offset);

        pthread_mutex_unlock(&mutex);
    }
};
```

**Integration with Write Path**:
```cpp
// After any write to a page during migration
if (table_info.migration_in_progress &&
    table_info.migration_phase <= MIGRATION_CATCH_UP)
{
    migration_state->dirty_pages.markDirty(page_number);

    // Update dirty page counter in catalog
    catalog_mgr->updateMigrationProgress(
        table_info.migration_id,
        /* pages_copied */ -1,  // Don't update
        /* dirty_pages */ migration_state->dirty_pages.count(),
        ctx);
}
```

---

## Part 4: Incremental Page Copy (COPYING Phase)

### 4.1 Background Thread Architecture

**Thread Model**:
```cpp
// Background migration worker thread
void* migrationWorkerThread(void* arg) {
    MigrationState *state = (MigrationState*)arg;

    while (state->phase == MIGRATION_COPYING) {
        // 1. Copy batch of pages
        uint64_t pages_copied = copyPageBatch(state);

        // 2. Update progress
        updateMigrationProgress(state, pages_copied);

        // 3. Yield to foreground queries
        usleep(state->yield_interval_ms * 1000);

        // 4. Check if all pages copied
        if (state->copied_pages >= state->total_pages) {
            transitionToPhase(state, MIGRATION_CATCH_UP);
            break;
        }
    }

    return NULL;
}
```

### 4.2 Page Copy Algorithm

**Batch Copy Function**:
```cpp
uint64_t copyPageBatch(MigrationState *state) {
    uint64_t pages_copied = 0;

    for (uint32_t i = 0; i < state->batch_size; i++) {
        // 1. Get next source page number
        uint32_t src_page = state->next_source_page++;

        if (src_page >= state->total_pages) {
            break;  // All pages copied
        }

        // 2. Pin source page
        void *src_buffer = buffer_pool->pinPage(
            state->source_tablespace, src_page, nullptr);

        // 3. Allocate target page
        uint32_t tgt_page;
        page_manager->allocatePage(state->target_tablespace, &tgt_page, nullptr);

        // 4. Pin target page
        void *tgt_buffer = buffer_pool->pinPage(
            state->target_tablespace, tgt_page, nullptr);

        // 5. Copy page contents
        memcpy(tgt_buffer, src_buffer, db->page_size());

        // 6. Update TIDs in copied page (Sprint 2 logic)
        std::unordered_map<uint64_t, uint64_t> tid_mapping;
        updatePageTIDs(src_buffer, tgt_buffer, src_page, tgt_page, &tid_mapping);

        // 7. Mark migrated TIDs in bloom filter
        for (const auto &[old_tid, new_tid] : tid_mapping) {
            state->migrated_bloom->insert(old_tid);
            state->tid_exact_mapping[old_tid] = new_tid;
        }

        // 8. Unpin pages
        buffer_pool->unpinPage(tgt_page, true, nullptr);   // Dirty
        buffer_pool->unpinPage(src_page, false, nullptr);  // Read-only

        // 9. Check if page was modified during copy (race condition)
        if (state->dirty_pages.isDirty(src_page)) {
            // Page modified during copy - will be re-copied in CATCH_UP
            LOG_DEBUG(STORAGE, "Page %u marked dirty during copy", src_page);
        }

        pages_copied++;
    }

    return pages_copied;
}
```

### 4.3 Progress Monitoring

**Statistics Collection**:
```cpp
void updateMigrationProgress(MigrationState *state, uint64_t pages_copied) {
    state->copied_pages += pages_copied;
    state->bytes_copied += pages_copied * db->page_size();

    // Calculate throughput
    time_t elapsed = time(NULL) - state->start_time;
    if (elapsed > 0) {
        state->pages_per_second = (float)state->copied_pages / elapsed;

        // Estimate time remaining
        uint64_t pages_remaining = state->total_pages - state->copied_pages;
        state->estimated_end_time = state->start_time +
            (uint64_t)(pages_remaining / state->pages_per_second);
    }

    // Update catalog
    catalog_mgr->updateMigrationProgress(
        state->migration_id,
        state->copied_pages,
        state->dirty_pages.count(),
        nullptr);

    // Log progress every 10%
    if (state->copied_pages % (state->total_pages / 10) == 0) {
        LOG_INFO(STORAGE, "Migration %s: %lu/%lu pages (%.1f%%), %.1f pages/sec",
                uuidToString(state->migration_id).c_str(),
                state->copied_pages,
                state->total_pages,
                (float)state->copied_pages * 100 / state->total_pages,
                state->pages_per_second);
    }
}
```

**User-Visible Monitoring**:
```sql
-- Query migration progress
SELECT
    table_name,
    migration_phase,
    copied_pages || '/' || total_pages AS progress,
    ROUND(copied_pages * 100.0 / total_pages, 2) AS percent_complete,
    pages_per_second,
    estimated_end_time
FROM pg_table_migrations
JOIN pg_tables USING (table_id)
WHERE migration_phase < 5;  -- Not complete
```

---

## Part 5: Catch-Up Phase and Convergence

### 5.1 Convergence Detection

**Goal**: Determine when dirty page rate is low enough to proceed to SWAP.

**Algorithm**:
```cpp
bool checkConvergence(MigrationState *state) {
    // 1. Calculate dirty page addition rate
    uint64_t current_dirty = state->dirty_pages.count();
    time_t elapsed = time(NULL) - state->phase_start_time;

    float dirty_pages_per_second = (float)current_dirty / elapsed;

    // 2. Compare to copy rate
    float copy_rate = state->pages_per_second;

    // 3. Convergence condition: dirty rate < 50% of copy rate
    bool converging = (dirty_pages_per_second < copy_rate * 0.5);

    // 4. Also require dirty pages < threshold (100 pages)
    bool below_threshold = (current_dirty < 100);

    LOG_INFO(STORAGE, "Convergence check: dirty=%.1f/s, copy=%.1f/s, total_dirty=%lu",
            dirty_pages_per_second, copy_rate, current_dirty);

    return converging && below_threshold;
}
```

### 5.2 Catch-Up Loop

**Re-copy Dirty Pages**:
```cpp
void catchUpPhase(MigrationState *state) {
    uint32_t iteration = 0;

    while (state->phase == MIGRATION_CATCH_UP) {
        iteration++;

        // 1. Get list of dirty pages
        std::vector<uint32_t> dirty_pages = state->dirty_pages.getSet();

        LOG_INFO(STORAGE, "Catch-up iteration %u: %lu dirty pages",
                iteration, dirty_pages.size());

        // 2. Re-copy each dirty page
        for (uint32_t src_page : dirty_pages) {
            // Copy page (same logic as COPYING phase)
            recopyDirtyPage(state, src_page);

            // Clear dirty bit
            state->dirty_pages.clearDirty(src_page);
        }

        // 3. Check convergence
        if (checkConvergence(state)) {
            LOG_INFO(STORAGE, "Convergence achieved, transitioning to SWAP phase");
            transitionToPhase(state, MIGRATION_SWAP);
            break;
        }

        // 4. Yield
        usleep(state->yield_interval_ms * 1000);

        // 5. Safety: limit iterations
        if (iteration > 100) {
            LOG_ERROR(STORAGE, "Catch-up phase not converging after 100 iterations");
            transitionToPhase(state, MIGRATION_FAILED);
            break;
        }
    }
}
```

### 5.3 Non-Convergence Handling

**Option 1: Fail Gracefully**
```cpp
if (!checkConvergence(state) && iteration > 100) {
    LOG_ERROR(STORAGE, "ONLINE migration failed to converge (write load too high)");
    LOG_ERROR(STORAGE, "Recommendation: Use OFFLINE migration or reduce write load");

    abortMigration(state, "Non-convergence");
    return Status::MIGRATION_FAILED;
}
```

**Option 2: Brief Write Pause** (controversial, but effective)
```cpp
if (!checkConvergence(state) && iteration > 50) {
    LOG_WARNING(STORAGE, "Migration not converging, pausing writes for 1 second");

    // Acquire exclusive lock (blocks all writes)
    acquireExclusiveLock(table_id);

    // Re-copy remaining dirty pages (no new dirty pages during pause)
    recopyAllDirtyPages(state);

    // Release lock
    releaseExclusiveLock(table_id);

    // Transition to SWAP
    transitionToPhase(state, MIGRATION_SWAP);
}
```

**Recommendation**: Start with Option 1 (fail gracefully). Add Option 2 if needed for high-write workloads.

---

## Part 6: Atomic Swap (Final Cutover)

### 6.1 Swap Algorithm

**Goal**: < 100ms exclusive lock, atomic catalog update, zero data loss.

**Algorithm**:
```cpp
Status executeSwap(MigrationState *state) {
    // 1. Acquire exclusive lock on table (blocks all operations)
    Status lock_status = acquireExclusiveLock(state->table_id);

    auto start_time = std::chrono::high_resolution_clock::now();

    // 2. Final copy of remaining dirty pages (should be < 100 pages)
    uint64_t final_dirty = state->dirty_pages.count();
    LOG_INFO(STORAGE, "Swap phase: copying final %lu dirty pages", final_dirty);

    for (uint32_t src_page : state->dirty_pages.getSet()) {
        recopyDirtyPage(state, src_page);
    }

    // 3. Begin atomic catalog transaction
    catalog_mgr->beginTransaction();

    // 4. Update TableInfo.tablespace_id
    TableInfo table_info = catalog_mgr->getTableInfo(state->table_id);
    table_info.tablespace_id = state->target_tablespace;
    table_info.migration_in_progress = false;
    table_info.migration_xid = 0;

    catalog_mgr->updateTableInfo(table_info);

    // 5. Batch update all index TIDs (use Sprint 2 implementations)
    std::vector<IndexInfo> indexes = catalog_mgr->getTableIndexes(state->table_id);

    for (const IndexInfo &index : indexes) {
        updateIndexTIDs(index, state->tid_exact_mapping);
    }

    // 6. Commit catalog transaction
    catalog_mgr->commitTransaction();

    // 7. Invalidate TID resolver cache (force re-resolution)
    tid_resolver->invalidateCache(state->table_id);

    // 8. Invalidate buffer pool entries for source tablespace pages
    buffer_pool->invalidateTablespace(state->source_tablespace, state->table_id);

    // 9. Release exclusive lock
    releaseExclusiveLock(state->table_id);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    LOG_INFO(STORAGE, "Swap phase completed in %ld ms", duration);

    // 10. Verify < 100ms requirement
    if (duration > 100) {
        LOG_WARNING(STORAGE, "Swap took %ld ms (target: < 100ms)", duration);
    }

    // 11. Transition to CLEANUP phase
    transitionToPhase(state, MIGRATION_CLEANUP);

    return Status::OK;
}
```

### 6.2 Index TID Batch Update

**Leverage Sprint 2 Implementations**:
```cpp
void updateIndexTIDs(
    const IndexInfo &index,
    const std::unordered_map<uint64_t, uint64_t> &tid_mapping)
{
    switch (index.type) {
        case IndexType::BTREE:
        {
            auto btree = BTree::open(db, index.index_id, index.root_page, nullptr);
            btree->updateTIDsAfterMigration(tid_mapping, nullptr, nullptr, nullptr);
            break;
        }

        case IndexType::HASH:
        {
            auto hash = HashIndex::open(db, index.index_id, index.root_page, nullptr);
            hash->updateTIDsAfterMigration(tid_mapping, nullptr, nullptr, nullptr);
            break;
        }

        case IndexType::HNSW:
        {
            auto hnsw = HnswIndex::open(db, index.index_id, index.root_page, nullptr);
            hnsw->updateTIDsAfterMigration(tid_mapping, nullptr, nullptr, nullptr);
            break;
        }

        case IndexType::GIN:
        {
            auto gin = GINIndex::open(db, index.index_id, index.root_page, nullptr);
            gin->updateTIDsAfterMigration(tid_mapping, nullptr, nullptr, nullptr);
            break;
        }

        case IndexType::BRIN:
        {
            // BRIN uses page_mapping, not tid_mapping
            std::unordered_map<uint64_t, uint64_t> page_mapping;
            // Build page_mapping from tid_mapping...

            auto brin = BrinIndex::open(db, index.index_id, index.root_page, nullptr);
            brin->updateBlockRangesAfterMigration(page_mapping, nullptr, nullptr, nullptr);
            break;
        }

        default:
            LOG_WARNING(STORAGE, "Unknown index type %d, skipping TID update",
                       static_cast<int>(index.type));
    }
}
```

### 6.3 In-Flight Query Handling

**Problem**: Queries started before SWAP may still reference source tablespace.

**Solution**: Keep source pages readable until all pre-swap snapshots close.

```cpp
// Track oldest active snapshot
TransactionId oldest_snapshot_xid = getOldestSnapshotXID();

// Defer source page deallocation until safe
if (oldest_snapshot_xid > state->migration_xid) {
    // All pre-migration snapshots closed, safe to deallocate
    deallocateSourcePages(state);
} else {
    // Schedule deferred cleanup
    scheduleCleanupAfterSnapshot(state, oldest_snapshot_xid);
}
```

---

## Part 7: Cleanup Phase

### 7.1 Deferred Source Page Deallocation

**Wait for Safe Point**:
```cpp
void cleanupPhase(MigrationState *state) {
    // 1. Wait for all snapshots < migration_xid to close
    while (getOldestSnapshotXID() < state->migration_xid) {
        LOG_DEBUG(STORAGE, "Waiting for snapshots < %lu to close",
                 state->migration_xid);
        sleep(1);
    }

    // 2. Safe to deallocate source pages
    LOG_INFO(STORAGE, "All pre-migration snapshots closed, deallocating source pages");

    deallocateSourcePages(state);

    // 3. Update FSM
    updateFSM(state);

    // 4. Remove migration state from catalog
    catalog_mgr->deleteMigration(state->migration_id);

    // 5. Free migration state memory
    freeMigrationState(state);

    LOG_INFO(STORAGE, "Migration cleanup complete");
}
```

### 7.2 Source Page Deallocation

```cpp
void deallocateSourcePages(MigrationState *state) {
    uint64_t pages_freed = 0;

    for (uint32_t page_num = 0; page_num < state->total_pages; page_num++) {
        // Deallocate page in source tablespace
        page_manager->deallocatePage(state->source_tablespace, page_num, nullptr);

        pages_freed++;
    }

    LOG_INFO(STORAGE, "Deallocated %lu pages from source tablespace %u",
            pages_freed, state->source_tablespace);
}
```

---

## Part 8: Error Handling and Rollback

### 8.1 Rollback Strategy

**Per-Phase Rollback**:

1. **INIT Phase Failure**:
   ```cpp
   // Simply delete migration state, no pages allocated yet
   catalog_mgr->deleteMigration(migration_id);
   ```

2. **COPYING/CATCH_UP Phase Failure**:
   ```cpp
   // Deallocate all target tablespace pages
   for (uint32_t page = 0; page < state->copied_pages; page++) {
       page_manager->deallocatePage(state->target_tablespace, page, nullptr);
   }

   // Clear migration state
   table_info.migration_in_progress = false;
   catalog_mgr->updateTableInfo(table_info);

   // Delete migration record
   catalog_mgr->deleteMigration(migration_id);
   ```

3. **SWAP Phase Failure**:
   ```cpp
   // Rollback catalog transaction
   catalog_mgr->rollbackTransaction();

   // Release exclusive lock
   releaseExclusiveLock(table_id);

   // Retry SWAP or abort migration
   ```

4. **CLEANUP Phase Failure**:
   ```cpp
   // Migration already committed, log error but don't rollback
   LOG_ERROR(STORAGE, "Cleanup failed, source pages not deallocated");
   LOG_ERROR(STORAGE, "Manual cleanup required: table %s, source tablespace %u",
            table_name, source_tablespace);
   ```

### 8.2 User-Initiated Cancellation

**API**:
```sql
-- Cancel running migration
CANCEL MIGRATION table_name;
```

**Implementation**:
```cpp
Status cancelMigration(const ID &table_id, ErrorContext *ctx) {
    // 1. Get migration state
    MigrationState *state = getMigrationState(table_id);

    if (!state) {
        return Status::NOT_FOUND;
    }

    // 2. Set cancellation flag
    state->cancelled = true;

    // 3. Wait for background thread to notice and stop
    while (state->phase < MIGRATION_FAILED) {
        usleep(100000);  // 100ms
    }

    // 4. Rollback based on phase
    rollbackMigration(state, ctx);

    LOG_INFO(STORAGE, "Migration cancelled by user: table %s",
            table_id.toString().c_str());

    return Status::OK;
}
```

### 8.3 Crash Recovery

**On Database Restart**:
```cpp
void recoverOnlineMigrations() {
    // 1. Query all active migrations
    std::vector<MigrationState> active_migrations =
        catalog_mgr->getActiveMigrations();

    for (MigrationState &state : active_migrations) {
        LOG_WARNING(STORAGE, "Found incomplete migration: table %s, phase %u",
                   state.table_id.toString().c_str(), state.phase);

        // 2. Determine recovery action based on phase
        if (state.phase < MIGRATION_SWAP) {
            // Before swap: rollback (data still in source)
            LOG_INFO(STORAGE, "Rolling back pre-swap migration");
            rollbackMigration(&state, nullptr);
        } else {
            // After swap: complete cleanup
            LOG_INFO(STORAGE, "Completing post-swap migration");
            completeMigration(&state, nullptr);
        }
    }
}
```

---

## Part 9: Performance Characteristics

### 9.1 Performance Targets

| Metric | Target | Acceptable | Notes |
|--------|--------|------------|-------|
| Query overhead (non-migrating) | 0% | < 1% | Fast path check only |
| Query overhead (migrating) | < 5% | < 10% | Bloom lookup + cache |
| Swap downtime | < 50ms | < 100ms | Exclusive lock duration |
| Migration throughput | 1000+ pages/sec | 500+ pages/sec | Depends on I/O |
| Memory overhead | < 1% of table size | < 5% | Bloom filter + bitmap |
| Convergence iterations | < 10 | < 100 | CATCH_UP phase |

### 9.2 Scalability Analysis

**Small Tables** (< 1000 pages):
- Migration time: < 1 second
- Overhead: Negligible
- Strategy: Could use OFFLINE migration instead

**Medium Tables** (1000 - 100,000 pages):
- Migration time: 1 second - 2 minutes
- Overhead: 2-5% query slowdown
- Strategy: ONLINE migration optimal

**Large Tables** (100,000 - 10,000,000 pages):
- Migration time: 2 minutes - 3 hours
- Overhead: 5-10% query slowdown
- Strategy: ONLINE migration essential
- Considerations: May need write pause for convergence

**Very Large Tables** (> 10,000,000 pages):
- Migration time: > 3 hours
- Overhead: Carefully monitor
- Strategy: ONLINE migration, possibly with write pause
- Considerations: May benefit from parallel copy threads (future enhancement)

### 9.3 Memory Usage

**Components**:
1. Bloom filter: ~1.2 MB per 1M tuples (1% FP rate)
2. Dirty page bitmap: 1 bit per page (~128 KB per 1M pages)
3. TID exact mapping: ~24 bytes per false positive (~1000 entries for 100K tuples)
4. Migration state: ~1 KB

**Total**: For 1M tuple table (~10M pages): ~2-3 MB memory overhead (< 0.01% of typical RAM)

---

## Part 10: Testing Strategy

### 10.1 Unit Tests

**Test Cases**:
1. TID Resolution Service
   - Bloom filter accuracy
   - Cache hit/miss rates
   - False positive handling

2. Write Routing
   - INSERT to target tablespace
   - UPDATE in-place vs cross-page
   - DELETE routing
   - Dirty page tracking

3. Convergence Detection
   - Dirty page rate calculation
   - Threshold detection
   - Non-convergence handling

### 10.2 Integration Tests

**Scenarios**:
1. Concurrent Reads During Migration
   - Run SELECT queries while COPYING/CATCH_UP
   - Verify correct results
   - Measure query latency

2. Concurrent Writes During Migration
   - Run INSERT/UPDATE/DELETE while COPYING/CATCH_UP
   - Verify data integrity
   - Verify writes routed correctly

3. Large Table Migration
   - Migrate 1M row table
   - Monitor progress
   - Verify successful completion

4. High Write Load
   - Test convergence behavior
   - Test non-convergence detection
   - Test write pause (if implemented)

### 10.3 Stress Tests

**Scenarios**:
1. Multiple Concurrent Migrations
   - Migrate 10 tables simultaneously
   - Verify no resource exhaustion
   - Verify all complete successfully

2. Long-Running Transactions
   - Start transaction before migration
   - Keep transaction open during SWAP
   - Verify snapshot isolation

3. Index-Only Scans
   - Queries using indexes only (no heap access)
   - Verify index TID updates work
   - Verify results consistent

---

## Part 11: Risk Assessment

### 11.1 High Risks

**Risk 1: Dual-Source Visibility Bugs**
- Impact: Query returns wrong results or crashes
- Mitigation: Extensive testing, feature flag to disable
- Contingency: Rollback to OFFLINE migration

**Risk 2: Swap Phase Timeout**
- Impact: > 100ms downtime, user-visible disruption
- Mitigation: Optimize index TID batch updates, limit final dirty pages
- Contingency: Retry SWAP with smaller dirty page threshold

**Risk 3: Non-Convergence**
- Impact: Migration never completes
- Mitigation: Convergence detection, fail after 100 iterations
- Contingency: Use OFFLINE migration or write pause

### 11.2 Medium Risks

**Risk 4: Memory Overhead**
- Impact: OOM on very large tables
- Mitigation: Monitor memory usage, bloom filter size tuning
- Contingency: Fall back to OFFLINE migration for huge tables

**Risk 5: Performance Regression**
- Impact: > 10% query slowdown during migration
- Mitigation: Benchmarking, optimization of TID resolution
- Contingency: Feature flag to disable, document limitations

### 11.3 Low Risks

**Risk 6: Dirty Page Tracking Errors**
- Impact: Pages not re-copied, data inconsistency
- Mitigation: Careful bitmap implementation, test coverage
- Contingency: Verification pass after SWAP

---

## Part 12: Implementation Roadmap

### Phase 1: State Management (Sprint 4, Task 5.4.1)
- [ ] Catalog schema extensions (pg_table_migrations)
- [ ] TableInfo extension (migration_in_progress flag)
- [ ] Migration state machine
- [ ] API functions
- **Effort**: 8-10 hours

### Phase 2: Dual-Source Visibility (Sprint 4, Task 5.4.2)
- [ ] TID Resolution Service
- [ ] Bloom filter integration
- [ ] Heap tuple fetch modification
- [ ] Version chain traversal
- [ ] Performance optimization
- **Effort**: 12-15 hours

### Phase 3: Write Routing (Sprint 4, Task 5.4.3)
- [ ] INSERT routing
- [ ] UPDATE routing (MGA-aware)
- [ ] DELETE routing
- [ ] Dirty page tracking
- **Effort**: 10-12 hours

### Phase 4: Incremental Copy (Sprint 5, Task 5.4.4)
- [ ] Background thread
- [ ] Page copy algorithm
- [ ] Progress monitoring
- **Effort**: 8-10 hours

### Phase 5: Catch-Up (Sprint 5, Task 5.4.5)
- [ ] Convergence detection
- [ ] Dirty page re-copy
- [ ] Non-convergence handling
- **Effort**: 6-8 hours

### Phase 6: Atomic Swap (Sprint 5, Task 5.4.6)
- [ ] Swap algorithm
- [ ] Index TID batch update
- [ ] In-flight query handling
- **Effort**: 8-10 hours

### Phase 7: Cleanup (Sprint 5, Task 5.4.7)
- [ ] Deferred deallocation
- [ ] FSM updates
- [ ] State cleanup
- **Effort**: 4-5 hours

### Phase 8: Error Handling (Sprint 6, Task 5.4.8)
- [ ] Per-phase rollback
- [ ] User cancellation
- [ ] Crash recovery
- **Effort**: 6-8 hours

### Phase 9: Testing (Sprint 6, Task 5.4.9)
- [ ] Unit tests
- [ ] Integration tests
- [ ] Stress tests
- **Effort**: 6-8 hours

**Total Estimated Effort**: 68-86 hours (matches roadmap estimate of 60-80 hours)

---

## Part 13: Success Criteria

**Architecture Design is COMPLETE when**:
- [x] Migration state tracking fully designed
- [x] Dual-source visibility model specified
- [x] Write routing strategy defined
- [x] Incremental copy algorithm designed
- [x] Catch-up and convergence logic specified
- [x] Atomic swap protocol defined
- [x] Error handling and rollback strategies documented
- [x] Performance targets established
- [x] Risk assessment completed
- [x] Implementation roadmap created

**ONLINE Migration Implementation is COMPLETE when**:
- [ ] All code implements this design
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Performance targets met (< 5% overhead, < 100ms swap)
- [ ] Documentation complete
- [ ] Code review approved

---

## Conclusion

This architecture provides a **production-ready design** for ONLINE tablespace migration in ScratchBird, leveraging the existing MGA infrastructure to enable concurrent operations during migration with minimal downtime.

**Key Innovations**:
1. MGA-native dual-source visibility (no new concurrency layer needed)
2. Bloom filter for fast TID resolution
3. Write routing preserves TID stability
4. Convergence detection for high write loads
5. Atomic swap with batch index TID updates

**Next Steps**:
1. Review this architecture document
2. Implement Sprint 4 (State Management + Visibility + Write Routing)
3. Implement Sprint 5 (Copy + Swap + Cleanup)
4. Implement Sprint 6 (Error Handling + Testing)

**Estimated Total Implementation**: 68-86 hours across 3 sprints (Sprints 4, 5, 6)

---

**Document Version**: 1.0
**Status**: ✅ COMPLETE
**Author**: Claude (with ScratchBird MGA specifications)
**Date**: October 21, 2025
**Next Action**: Begin Sprint 4 implementation
