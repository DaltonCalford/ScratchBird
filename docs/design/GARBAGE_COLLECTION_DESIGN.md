# Garbage Collection Design

**Date:** October 10, 2025
**Status:** Design Document
**Phase:** Phase 3, Task 3.4
**Reference:** `docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/implemented/ALPHA_1_2_IMPLEMENTATION_PLAN.md`

---

## Table of Contents

1. [Overview](#overview)
2. [Goals and Requirements](#goals-and-requirements)
3. [GC Strategies](#gc-strategies)
4. [Architecture](#architecture)
5. [Cooperative GC](#cooperative-gc)
6. [Background GC](#background-gc)
7. [Configuration](#configuration)
8. [Monitoring and Statistics](#monitoring-and-statistics)
9. [Integration with Sweep](#integration-with-sweep)
10. [Implementation Phases](#implementation-phases)

---

## Overview

Garbage collection (GC) is the process of reclaiming space occupied by old tuple versions that are no longer visible to any transaction. In an MVCC system like ScratchBird, UPDATE and DELETE operations create new tuple versions, leaving old versions in place for transactions that may still need them.

The garbage collector works in conjunction with the sweep mechanism:
- **Sweep**: Advances OIT (Oldest Interesting Transaction) to enable garbage identification
- **Garbage Collection**: Actually removes old tuple versions and reclaims space

### Key Concepts

**Garbage Tuple**: A tuple version with `xmax < OIT` that has been superseded or deleted.

**Tuple Version Chain**: Linked list of tuple versions connected by forward pointers (`t_ctid`).

**Cooperative GC**: Cleanup performed during normal read operations (opportunistic).

**Background GC**: Dedicated thread that periodically scans for garbage (proactive).

---

## Goals and Requirements

### Primary Goals

1. **Space Reclamation**: Free space occupied by dead tuple versions
2. **Performance**: Minimal impact on normal database operations
3. **Configurable**: Support different GC policies for different workloads
4. **Observable**: Provide monitoring and statistics

### Requirements

1. **Correctness**: Never remove tuples visible to active transactions
2. **Concurrency**: Work safely with concurrent reads/writes
3. **Integration**: Coordinate with sweep mechanism
4. **Flexibility**: Support both cooperative and background GC modes

### Non-Goals

1. **Compaction**: Not implementing full page compaction (future work)
2. **Index GC**: Index cleanup handled separately (future work)
3. **Cross-page chains**: Not handling version chains across pages (future work)

---

## GC Strategies

ScratchBird supports three GC strategies:

### 1. Cooperative GC (COOPERATIVE)

**How it works:**
- Cleanup happens during normal page reads
- When reading a page, check for garbage tuples
- Remove dead tuples if found
- Update tuple pointers and free space

**Advantages:**
- No additional thread overhead
- Focuses on "hot" pages (pages being actively accessed)
- No separate scanning cost

**Disadvantages:**
- Only cleans pages that are read
- Cold data may accumulate garbage
- Cleanup adds latency to read operations

**Best for:**
- Read-heavy workloads
- Systems with limited resources
- Applications with good data locality

### 2. Background GC (BACKGROUND)

**How it works:**
- Dedicated GC thread runs periodically
- Scans pages looking for garbage
- Tracks "dirty pages" (pages with known garbage)
- Prioritizes pages with most garbage

**Advantages:**
- Proactive cleanup before space issues
- Cleans both hot and cold data
- Predictable space reclamation

**Disadvantages:**
- Additional CPU and I/O overhead
- May scan pages unnecessarily
- Thread management complexity

**Best for:**
- Write-heavy workloads
- Systems with many updates/deletes
- Databases with strict space requirements

### 3. Combined Mode (COMBINED)

**How it works:**
- Both cooperative and background GC enabled
- Cooperative GC during normal operations
- Background GC fills in the gaps
- Background GC skips recently cleaned pages

**Advantages:**
- Best of both worlds
- Adaptive to workload
- Comprehensive coverage

**Disadvantages:**
- Highest complexity
- Most resource usage
- Requires careful coordination

**Best for:**
- Production environments (default)
- Mixed workloads
- General purpose databases

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────┐
│                     GarbageCollector                     │
│                                                           │
│  ┌──────────────────┐        ┌────────────────────┐    │
│  │  Cooperative GC  │        │   Background GC    │    │
│  │                  │        │                    │    │
│  │  - Page hooks    │        │  - GC thread       │    │
│  │  - Opportunistic │        │  - Dirty page list │    │
│  │  - Inline clean  │        │  - Periodic scan   │    │
│  └──────────────────┘        └────────────────────┘    │
│                                                           │
│  ┌───────────────────────────────────────────────────┐  │
│  │             Statistics & Monitoring                │  │
│  │  - tuples_removed, pages_cleaned, gc_runs        │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                        │
                        ├─→ StorageEngine (cooperative hooks)
                        ├─→ TransactionManager (OIT checks)
                        ├─→ SweepManager (coordination)
                        └─→ Config (policy, intervals)
```

### Class Structure

```cpp
class GarbageCollector {
public:
    // Lifecycle
    GarbageCollector(Database* db);
    ~GarbageCollector();
    Status initialize(ErrorContext* ctx);

    // Cooperative GC (called by StorageEngine during page reads)
    void processPageCooperative(uint32_t page_id, ErrorContext* ctx);

    // Background GC control
    Status startBackgroundGC(ErrorContext* ctx);
    Status stopBackgroundGC(ErrorContext* ctx);

    // Statistics
    GCStatistics getStatistics() const;

    // Dirty page tracking
    void markPageDirty(uint32_t page_id);

private:
    Database* db_;
    TransactionManager* txn_manager_;
    StorageEngine* storage_engine_;

    // GC policy
    GCPolicy policy_;  // COOPERATIVE, BACKGROUND, COMBINED

    // Background GC thread
    std::thread background_thread_;
    std::atomic<bool> background_running_;
    std::atomic<bool> shutdown_requested_;

    // Dirty page tracking
    mutable std::mutex dirty_pages_mutex_;
    std::unordered_set<uint32_t> dirty_pages_;

    // Statistics
    mutable std::mutex stats_mutex_;
    GCStatistics stats_;

    // Internal methods
    void backgroundGCLoop();
    void cleanPage(uint32_t page_id, ErrorContext* ctx);
    bool isTupleGarbage(const TupleHeader* header);
    void removeTupleVersion(uint32_t page_id, uint16_t offset,
                           ErrorContext* ctx);
};

enum class GCPolicy {
    COOPERATIVE,  // Only cooperative GC
    BACKGROUND,   // Only background GC
    COMBINED      // Both cooperative and background (default)
};

struct GCStatistics {
    uint64_t tuples_removed;           // Total tuples removed
    uint64_t pages_cleaned;            // Pages cleaned
    uint64_t cooperative_runs;         // Cooperative GC executions
    uint64_t background_runs;          // Background GC passes
    uint64_t last_background_time;     // Timestamp of last background run
    uint64_t last_background_duration_ms;  // Duration of last run
    uint64_t dirty_page_count;         // Current dirty pages
};
```

---

## Cooperative GC

### Trigger Points

Cooperative GC is triggered when StorageEngine reads a page:

```cpp
// In StorageEngine::selectPage() or similar
Status StorageEngine::selectPage(uint32_t page_id, ...) {
    // Read page
    Page* page = readPage(page_id);

    // Cooperative GC hook
    if (gc_enabled_ && shouldRunCooperativeGC(page_id)) {
        gc_->processPageCooperative(page_id, &err_ctx);
    }

    // Continue with normal page processing
    ...
}
```

### Implementation Logic

```cpp
void GarbageCollector::processPageCooperative(uint32_t page_id,
                                                ErrorContext* ctx) {
    // 1. Check if cooperative GC is enabled
    if (policy_ == GCPolicy::BACKGROUND) {
        return;  // Cooperative disabled
    }

    // 2. Get current OIT from TransactionManager
    uint64_t oit = txn_manager_->getOldestXid();

    // 3. Read page and scan tuples
    Page* page = storage_engine_->getPage(page_id);
    if (!page) return;

    bool page_modified = false;
    uint64_t tuples_removed = 0;

    // 4. Scan all tuples on page
    for (uint16_t offset = 0; offset < page->tuple_count; offset++) {
        TupleHeader* header = getTupleAt(page, offset);

        // 5. Check if tuple is garbage
        if (isTupleGarbage(header, oit)) {
            // Remove tuple version
            removeTupleVersion(page_id, offset, ctx);
            tuples_removed++;
            page_modified = true;
        }
    }

    // 6. Update statistics
    if (page_modified) {
        updateStatistics(tuples_removed, 1, true);
        dirty_pages_.erase(page_id);  // No longer dirty
    }
}

bool GarbageCollector::isTupleGarbage(const TupleHeader* header,
                                       uint64_t oit) {
    // Tuple is garbage if:
    // 1. It has been deleted or updated (xmax != INVALID_XID)
    // 2. The deleting/updating transaction is old (xmax < OIT)
    // 3. The transaction committed (check CLOG)

    if (header->xmax == INVALID_XID) {
        return false;  // Still visible
    }

    if (header->xmax >= oit) {
        return false;  // Deleting transaction too new
    }

    // Check if deleting transaction committed
    TransactionState state;
    Status s = txn_manager_->getTransactionState(header->xmax, state, nullptr);

    return (s == Status::OK && state == TransactionState::COMMITTED);
}
```

### Rate Limiting

To avoid excessive overhead, cooperative GC uses rate limiting:

```cpp
bool shouldRunCooperativeGC(uint32_t page_id) {
    // Don't run on every page read - only periodically
    // Use a simple counter or timestamp-based throttling

    static thread_local uint32_t counter = 0;
    counter++;

    // Run cooperative GC on ~1% of page reads
    return (counter % 100) == 0;
}
```

---

## Background GC

### Background Thread

The background GC thread runs continuously, sleeping between passes:

```cpp
void GarbageCollector::backgroundGCLoop() {
    LOG_INFO(VACUUM, "Background GC thread started");

    while (!shutdown_requested_.load(std::memory_order_acquire)) {
        auto start_time = std::chrono::steady_clock::now();

        // 1. Get current OIT
        uint64_t oit = txn_manager_->getOldestXid();

        // 2. Get dirty pages or scan all pages
        std::vector<uint32_t> pages_to_clean;
        {
            std::lock_guard<std::mutex> lock(dirty_pages_mutex_);
            pages_to_clean.assign(dirty_pages_.begin(), dirty_pages_.end());
        }

        // 3. Clean each dirty page
        uint64_t tuples_removed = 0;
        uint64_t pages_cleaned = 0;

        for (uint32_t page_id : pages_to_clean) {
            if (shutdown_requested_.load(std::memory_order_acquire)) {
                break;
            }

            ErrorContext err_ctx;
            cleanPage(page_id, &err_ctx);
            pages_cleaned++;
        }

        // 4. Update statistics
        auto end_time = std::chrono::steady_clock::now();
        uint64_t duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        updateBackgroundStatistics(pages_cleaned, duration_ms);

        // 5. Sleep before next pass
        uint32_t sleep_ms = Config::get<uint32_t>("garbage_collection",
                                                    "background_interval_ms",
                                                    5000);  // 5 seconds default

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }

    LOG_INFO(VACUUM, "Background GC thread stopped");
}
```

### Dirty Page Tracking

Pages are marked as dirty when tuples are deleted or updated:

```cpp
// In StorageEngine::deleteTuple() or updateTuple()
void StorageEngine::deleteTuple(TupleId tuple_id, uint64_t xid) {
    // Mark tuple as deleted
    setTupleXmax(tuple_id, xid);

    // Mark page as dirty for GC
    if (gc_) {
        gc_->markPageDirty(tuple_id.page_id);
    }
}
```

### Priority Queue

For efficiency, background GC can prioritize pages with most garbage:

```cpp
struct DirtyPage {
    uint32_t page_id;
    uint32_t estimated_garbage_count;
    uint64_t last_cleaned_time;

    bool operator<(const DirtyPage& other) const {
        return estimated_garbage_count < other.estimated_garbage_count;
    }
};

std::priority_queue<DirtyPage> dirty_page_queue_;
```

---

## Configuration

### Configuration Parameters

From `sb_config.ini`:

```ini
[garbage_collection]
# GC policy: COOPERATIVE, BACKGROUND, COMBINED
gc_policy = COMBINED

# Background GC interval (milliseconds)
background_interval_ms = 5000

# Cooperative GC rate (1 in N page reads)
cooperative_rate = 100

# Maximum pages to clean per background pass
max_pages_per_pass = 1000

# Enable GC (can be disabled for debugging)
gc_enabled = true
```

### Runtime Configuration

```cpp
class GarbageCollector {
public:
    // Runtime policy changes
    void setPolicy(GCPolicy policy);
    GCPolicy getPolicy() const;

    // Enable/disable GC
    void enable();
    void disable();
    bool isEnabled() const;
};
```

---

## Monitoring and Statistics

### MON_GARBAGE_COLLECTION Table

New monitoring table for GC statistics:

```sql
SELECT * FROM MON_GARBAGE_COLLECTION;
```

Returns:

| Column | Type | Description |
|--------|------|-------------|
| MON$TUPLES_REMOVED | BIGINT | Total tuples removed |
| MON$PAGES_CLEANED | BIGINT | Pages cleaned |
| MON$COOPERATIVE_RUNS | BIGINT | Cooperative GC executions |
| MON$BACKGROUND_RUNS | BIGINT | Background GC passes |
| MON$LAST_BG_TIME | BIGINT | Timestamp of last background run |
| MON$LAST_BG_DURATION_MS | BIGINT | Duration of last background run |
| MON$DIRTY_PAGES | BIGINT | Current dirty page count |
| MON$GC_POLICY | VARCHAR | Current GC policy |
| MON$GC_ENABLED | BOOLEAN | GC enabled status |

### Statistics in Code

```cpp
struct GCStatistics {
    uint64_t tuples_removed;
    uint64_t pages_cleaned;
    uint64_t cooperative_runs;
    uint64_t background_runs;
    uint64_t last_background_time;
    uint64_t last_background_duration_ms;
    uint64_t dirty_page_count;
};

GCStatistics GarbageCollector::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}
```

---

## Integration with Sweep

### Coordination

Sweep and GC work together:

1. **Sweep advances OIT** → enables GC to identify more garbage
2. **GC removes garbage** → actually reclaims space
3. **Sweep can mark pages dirty** → tells GC where work is needed

### Integration Points

```cpp
// In SweepManager::executeSweep()
void SweepManager::executeSweep(bool foreground, ErrorContext* ctx) {
    // ... sweep logic ...

    // After advancing OIT, notify GC
    if (new_oit > old_oit && gc_) {
        gc_->notifySweepComplete(old_oit, new_oit);
    }
}

// In GarbageCollector
void GarbageCollector::notifySweepComplete(uint64_t old_oit, uint64_t new_oit) {
    // OIT advanced - background GC can now clean more tuples
    // Optionally trigger immediate background pass
    if (policy_ == GCPolicy::BACKGROUND || policy_ == GCPolicy::COMBINED) {
        wakeBackgroundThread();
    }
}
```

### Avoiding Duplicate Work

When running in foreground mode, sweep can optionally do space reclamation:

```cpp
// In SweepManager
if (foreground && !gc_enabled) {
    // GC disabled - do space reclamation during sweep
    reclaimSpace(new_oit, ctx);
} else {
    // GC enabled - let GC handle space reclamation
    // Just mark pages as dirty
    markDirtyPagesForGC(old_oit, new_oit);
}
```

---

## Implementation Phases

### Phase 1: Core Infrastructure (2 days)

**Goal**: Create GarbageCollector class and basic structure

**Tasks**:
1. Create `include/scratchbird/core/garbage_collector.h`
2. Create `src/core/garbage_collector.cpp`
3. Define `GCPolicy` enum and `GCStatistics` struct
4. Implement constructor, destructor, initialize()
5. Add getStatistics() method
6. Integrate with Database class

**Deliverable**: GarbageCollector skeleton compiles and can be accessed from Database

### Phase 2: Cooperative GC (3 days)

**Goal**: Implement cooperative GC triggered during page reads

**Tasks**:
1. Implement `processPageCooperative()` method
2. Implement `isTupleGarbage()` helper
3. Implement `removeTupleVersion()` helper
4. Add hooks to StorageEngine page read paths
5. Implement rate limiting
6. Update statistics for cooperative runs

**Deliverable**: Cooperative GC removes dead tuples during normal operations

### Phase 3: Background GC Thread (3 days)

**Goal**: Implement background GC thread with dirty page tracking

**Tasks**:
1. Implement `backgroundGCLoop()` method
2. Implement `startBackgroundGC()` / `stopBackgroundGC()`
3. Add dirty page tracking (markPageDirty, dirty_pages_ set)
4. Implement thread lifecycle management
5. Add sleep/wake mechanism
6. Update statistics for background runs

**Deliverable**: Background thread runs periodically and cleans dirty pages

### Phase 4: Configuration Integration (1 day)

**Goal**: Read GC configuration from config file

**Tasks**:
1. Add [garbage_collection] section to `sb_config.ini.example`
2. Read gc_policy from config
3. Read background_interval_ms from config
4. Read cooperative_rate from config
5. Implement setPolicy() / getPolicy() for runtime changes

**Deliverable**: GC behavior configurable via config file

### Phase 5: Monitoring and Statistics (1 day)

**Goal**: Expose GC statistics via MON_GARBAGE_COLLECTION query

**Tasks**:
1. Add MON_GARBAGE_COLLECTION case to Executor
2. Implement monitoring query handler
3. Return all GC statistics columns
4. Test query returns correct data

**Deliverable**: SELECT * FROM MON_GARBAGE_COLLECTION works

### Phase 6: Integration with Sweep (1 day)

**Goal**: Coordinate GC with sweep mechanism

**Tasks**:
1. Add notifySweepComplete() method
2. Call from SweepManager after OIT advancement
3. Implement wakeBackgroundThread() to trigger immediate pass
4. Avoid duplicate work between sweep and GC

**Deliverable**: GC and sweep work together efficiently

### Phase 7: Testing (2 days)

**Goal**: Comprehensive test coverage for all GC modes

**Tasks**:
1. Create `tests/unit/test_garbage_collection.cpp`
2. Test cooperative GC removes dead tuples
3. Test background GC thread lifecycle
4. Test dirty page tracking
5. Test different GC policies (COOPERATIVE, BACKGROUND, COMBINED)
6. Test integration with sweep
7. Test MON_GARBAGE_COLLECTION query
8. Test statistics accuracy

**Deliverable**: All GC tests passing

---

## Testing Strategy

### Unit Tests

1. **Basic GC Tests**
   - GarbageCollector creation and initialization
   - Statistics retrieval
   - Policy get/set

2. **Cooperative GC Tests**
   - Tuple garbage detection
   - Dead tuple removal during page reads
   - Rate limiting
   - Statistics updates

3. **Background GC Tests**
   - Thread start/stop
   - Dirty page tracking
   - Periodic cleaning
   - Statistics updates

4. **Integration Tests**
   - GC with concurrent transactions
   - GC with sweep mechanism
   - GC policy switching
   - MON_GARBAGE_COLLECTION query

### Performance Tests

1. **Overhead Measurement**
   - Cooperative GC impact on read latency
   - Background GC CPU usage
   - Memory overhead of dirty page tracking

2. **Effectiveness Tests**
   - Space reclamation rate
   - Dead tuple accumulation with GC disabled vs enabled
   - Comparison of COOPERATIVE vs BACKGROUND vs COMBINED

---

## Success Criteria

Task 3.4 is complete when:

1. ✅ GarbageCollector class implemented
2. ✅ Cooperative GC removes dead tuples during page reads
3. ✅ Background GC thread runs and cleans pages
4. ✅ Dirty page tracking functional
5. ✅ All three GC policies (COOPERATIVE, BACKGROUND, COMBINED) work
6. ✅ Configuration integrated
7. ✅ MON_GARBAGE_COLLECTION monitoring query works
8. ✅ Integration with sweep mechanism complete
9. ✅ All tests passing
10. ✅ Documentation complete

---

## Future Enhancements

Not in scope for Task 3.4, but documented for future work:

1. **Index GC**: Clean up dead index entries
2. **Page Compaction**: Defragment pages with many dead tuples
3. **Cross-Page Cleanup**: Handle version chains spanning multiple pages
4. **Vacuum Full**: Rewrite entire tables to reclaim maximum space
5. **Smart Scheduling**: Adjust GC frequency based on workload
6. **Parallel GC**: Multiple background GC threads

---

**End of Design Document**
