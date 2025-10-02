# MGA Implementation Plan

**Date:** 2025-10-02
**Status:** 📋 READY FOR IMPLEMENTATION
**Objective:** Complete Multi-Generational Architecture for ScratchBird database

## Executive Summary

This plan details the implementation of the remaining MGA components identified in the gap analysis. The work is divided into 6 phases over approximately 7 weeks, totaling ~9,100 lines of code.

**Current State:** 60% of MGA specification implemented (single-connection only)
**Target State:** 100% MGA implementation with full concurrency support

---

## Phase 1: Multi-Connection Foundation (Week 1)

**Goal:** Remove single-connection limitation, enable tracking of multiple active transactions

**Priority:** CRITICAL (Prerequisite for all concurrency work)

### 1.1 ProcArray Implementation

**New Files:**
- `include/scratchbird/core/proc_array.h` (200 lines)
- `src/core/proc_array.cpp` (600 lines)

**Data Structures:**

```cpp
// Process control block (per connection/backend)
struct ProcessControlBlock {
    uint32_t proc_id;              // Process ID
    pid_t backend_pid;              // OS process ID
    bool is_active;                 // Is this slot active?

    // Transaction state
    uint64_t xid;                   // Current transaction XID (0 = none)
    uint64_t backend_xmin;          // Snapshot horizon for this backend
    uint64_t xmin;                  // Oldest XID visible to this backend

    // Locking state (for Phase 2)
    uint32_t wait_lock_id;          // Lock waiting for (0 = none)
    bool deadlock_check_pending;    // Needs deadlock check

    // Statistics
    uint64_t start_time;            // Backend start timestamp
    uint64_t query_start_time;      // Current query start
};

// Process array (shared memory structure)
struct ProcArray {
    // Configuration
    uint32_t max_backends;          // Maximum number of backends

    // Process control blocks
    ProcessControlBlock* procs;     // Array of PCBs

    // Free list
    uint32_t first_free;            // First free slot
    uint32_t num_active;            // Number of active backends

    // Global transaction state
    uint64_t latest_completed_xid;  // Latest completed XID
    uint64_t oldest_xmin;           // Oldest xmin across all backends

    // Synchronization
    pthread_rwlock_t array_lock;    // Read-write lock for array
    pthread_mutex_t alloc_lock;     // Lock for slot allocation
};

class ProcArrayManager {
public:
    // Initialize shared memory ProcArray
    static Status initialize(Database* db, uint32_t max_backends, ErrorContext* ctx);

    // Backend registration
    static Status registerBackend(uint32_t* proc_id_out, ErrorContext* ctx);
    static Status unregisterBackend(uint32_t proc_id, ErrorContext* ctx);

    // Transaction tracking
    static Status setTransactionId(uint32_t proc_id, uint64_t xid, ErrorContext* ctx);
    static Status clearTransactionId(uint32_t proc_id, ErrorContext* ctx);

    // Snapshot support
    static Status getActiveTransactions(
        std::vector<uint64_t>* xids_out,
        uint64_t* oldest_xmin_out,
        ErrorContext* ctx);

    // Vacuum support
    static Status getVacuumHorizon(uint64_t* horizon_out, ErrorContext* ctx);

private:
    static ProcArray* proc_array_;
};
```

**Implementation Steps:**

1. **Day 1: Data Structure and Shared Memory**
   - Define `ProcessControlBlock` and `ProcArray` structures
   - Implement shared memory allocation in Database class
   - Add initialization in `Database::open()`

2. **Day 2: Backend Registration**
   - Implement `registerBackend()` with free list management
   - Implement `unregisterBackend()` with cleanup
   - Add backend lifecycle tracking

3. **Day 3: Transaction Tracking Integration**
   - Modify `TransactionManager::beginTransaction()` to call `setTransactionId()`
   - Modify `TransactionManager::commitTransaction()` to call `clearTransactionId()`
   - Implement `getActiveTransactions()` for snapshot building

**Changes to Existing Code:**

`transaction_manager.h`:
```cpp
class TransactionManager {
    // Remove single-connection restriction
    // uint64_t active_xid_ = 0;  // DELETE THIS

    // Add per-backend tracking
    uint32_t my_proc_id_ = 0;  // This backend's proc ID

public:
    // Modified signature
    Status beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext* ctx);
    Status commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext* ctx);
    Status rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext* ctx);
};
```

`transaction_manager.cpp` modifications:
```cpp
auto TransactionManager::beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove check for active_xid_ != 0 (ALLOW MULTIPLE NOW)

    // Allocate new XID
    uint64_t new_xid = next_xid_++;

    // Record in cache
    transaction_cache_[new_xid] = TransactionState::ACTIVE;

    // Register in ProcArray
    Status status = ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);
    if (status != Status::OK) {
        transaction_cache_.erase(new_xid);
        return status;
    }

    // Write to TIP
    status = writeTipEntry(new_xid, TransactionState::ACTIVE, ctx);
    // ... rest of existing code
}
```

**Testing:**
- Unit test: Create 100 backends, verify registration
- Unit test: Begin 50 concurrent transactions
- Integration test: Multi-connection transaction isolation

---

## Phase 2: Lock Manager (Weeks 2-3)

**Goal:** Implement full lock manager with table and row locks, deadlock detection

**Priority:** CRITICAL (Enables concurrent transactions)

### 2.1 Lock Manager Core (Days 1-3)

**New Files:**
- `include/scratchbird/core/lock_manager.h` (400 lines)
- `src/core/lock_manager.cpp` (1,500 lines)

**Data Structures:**

```cpp
// Lock modes (from PostgreSQL)
enum class LockMode : uint8_t {
    LOCK_ACCESS_SHARE = 1,           // SELECT
    LOCK_ROW_SHARE = 2,              // SELECT FOR UPDATE/SHARE
    LOCK_ROW_EXCLUSIVE = 3,          // UPDATE, DELETE, INSERT
    LOCK_SHARE_UPDATE_EXCLUSIVE = 4, // VACUUM, CREATE INDEX CONCURRENTLY
    LOCK_SHARE = 5,                  // CREATE INDEX
    LOCK_SHARE_ROW_EXCLUSIVE = 6,    // LOCK TABLE ... SHARE ROW EXCLUSIVE
    LOCK_EXCLUSIVE = 7,              // ALTER TABLE, DROP TABLE
    LOCK_ACCESS_EXCLUSIVE = 8        // ALTER TABLE, DROP TABLE, TRUNCATE
};

// Lock target (what is being locked)
enum class LockTarget : uint8_t {
    LOCK_TARGET_DATABASE,
    LOCK_TARGET_TABLE,
    LOCK_TARGET_PAGE,
    LOCK_TARGET_TUPLE
};

// Lock tag (identifies a lockable object)
struct LockTag {
    LockTarget target_type;
    UuidV7Bytes object_uuid;    // Table/Index UUID
    uint64_t page_num;          // For page locks
    uint16_t offset_num;        // For tuple locks

    // Hash function for std::unordered_map
    size_t hash() const {
        // Combine all fields
    }

    bool operator==(const LockTag& other) const;
};

// Lock request (one backend waiting for a lock)
struct LockRequest {
    uint32_t proc_id;              // Backend requesting lock
    LockMode mode;                 // Requested mode
    bool granted;                  // Is lock granted?
    uint64_t request_time;         // When requested

    LockRequest* next;             // Queue linkage
};

// Lock object (one lockable resource)
struct Lock {
    LockTag tag;                   // What is locked

    // Granted locks (bitmask of granted modes)
    uint32_t granted_mask;         // Bit i = mode i granted
    uint32_t granted_counts[8];    // Count per mode

    // Waiting queue
    LockRequest* wait_queue_head;
    LockRequest* wait_queue_tail;
    uint32_t wait_queue_size;

    // Conflict detection
    uint32_t conflicting_mask;     // Modes that conflict with granted
};

class LockManager {
public:
    explicit LockManager(Database* db);
    ~LockManager();

    // Initialization
    Status initialize(ErrorContext* ctx);

    // Lock acquisition
    Status acquireLock(
        uint32_t proc_id,
        const LockTag& tag,
        LockMode mode,
        bool wait,                  // Block if conflict?
        uint32_t timeout_ms,        // Wait timeout
        ErrorContext* ctx);

    // Lock release
    Status releaseLock(
        uint32_t proc_id,
        const LockTag& tag,
        LockMode mode,
        ErrorContext* ctx);

    // Release all locks for a backend (on disconnect/abort)
    Status releaseAllLocks(uint32_t proc_id, ErrorContext* ctx);

    // Check if lock conflicts
    bool checkConflict(const LockTag& tag, LockMode mode);

private:
    Database* db_;

    // Lock tables
    std::unordered_map<LockTag, Lock*, LockTag::Hash> lock_table_;
    std::unordered_multimap<uint32_t, Lock*> proc_locks_;  // By proc_id

    // Lock pools
    std::vector<Lock*> lock_pool_;
    std::vector<LockRequest*> request_pool_;

    // Synchronization
    std::mutex lock_table_mutex_;

    // Deadlock detection (Phase 2.2)
    std::unique_ptr<DeadlockDetector> deadlock_detector_;

    // Lock conflict matrix
    static const bool conflict_matrix_[8][8];

    // Helper methods
    Lock* findOrCreateLock(const LockTag& tag);
    void removeLockIfUnused(Lock* lock);
    void grantWaitingLocks(Lock* lock);
};
```

**Lock Conflict Matrix (from PostgreSQL):**
```cpp
// conflict_matrix_[held_mode][requested_mode]
const bool LockManager::conflict_matrix_[8][8] = {
    //     AS  RS  RE  SUE  S  SRE  E  AE
    /* AS */ {0,  0,  0,  0,  0,  0,  0,  1},
    /* RS */ {0,  0,  0,  0,  0,  0,  1,  1},
    /* RE */ {0,  0,  0,  0,  1,  1,  1,  1},
    /* SUE*/ {0,  0,  0,  0,  1,  1,  1,  1},
    /* S  */ {0,  0,  1,  1,  0,  1,  1,  1},
    /* SRE*/ {0,  0,  1,  1,  1,  1,  1,  1},
    /* E  */ {0,  1,  1,  1,  1,  1,  1,  1},
    /* AE */ {1,  1,  1,  1,  1,  1,  1,  1}
};
```

**Implementation Steps:**

1. **Day 1: Data Structures and Conflict Matrix**
   - Implement `LockTag`, `Lock`, `LockRequest` structures
   - Implement lock conflict matrix
   - Implement hash tables for lock storage

2. **Day 2: Lock Acquisition**
   - Implement `acquireLock()` with conflict detection
   - Implement lock queuing for conflicts
   - Implement lock upgrade/downgrade

3. **Day 3: Lock Release**
   - Implement `releaseLock()` with queue processing
   - Implement `releaseAllLocks()` for transaction end
   - Implement lock cleanup

### 2.2 Deadlock Detection (Days 4-5)

**Data Structures:**

```cpp
// Wait-for graph edge
struct WaitEdge {
    uint32_t waiter_proc_id;       // Backend waiting
    uint32_t holder_proc_id;       // Backend holding lock
    LockTag lock_tag;              // What lock

    WaitEdge* next;                // Graph linkage
};

class DeadlockDetector {
public:
    explicit DeadlockDetector(LockManager* lock_mgr);

    // Run deadlock detection (called periodically)
    Status detectDeadlocks(ErrorContext* ctx);

    // Check if adding wait would create cycle
    bool wouldCreateCycle(uint32_t waiter, uint32_t holder);

private:
    LockManager* lock_mgr_;

    // Wait-for graph
    std::unordered_map<uint32_t, std::vector<uint32_t>> wait_graph_;

    // DFS for cycle detection
    bool hasCycle(uint32_t start_proc, std::unordered_set<uint32_t>* visited);

    // Victim selection (abort youngest transaction)
    uint32_t selectVictim(const std::vector<uint32_t>& cycle);

    // Abort transaction to break deadlock
    Status abortTransaction(uint32_t proc_id, ErrorContext* ctx);
};
```

**Algorithm:**

1. **Build Wait-For Graph:**
   - For each waiting lock request:
     - Add edge: waiter → each holder of conflicting lock

2. **Detect Cycle:**
   - Run DFS from each waiter
   - If DFS returns to start node, cycle found

3. **Select Victim:**
   - Choose youngest transaction (highest XID)
   - Abort victim with STATUS_DEADLOCK_DETECTED

4. **Break Deadlock:**
   - Release victim's locks
   - Wake up waiting transactions

**Integration:**
- Run deadlock detector every 1 second (background thread)
- Run on-demand when lock wait timeout exceeded

### 2.3 Row-Level Locking (Days 6-7)

**Integration with HeapPage:**

```cpp
// In heap_page.h
class HeapPage {
    // Add lock support
    Status lockTuple(
        uint16_t item_id,
        LockMode mode,
        uint32_t proc_id,
        ErrorContext* ctx);

    Status unlockTuple(
        uint16_t item_id,
        uint32_t proc_id,
        ErrorContext* ctx);
};

// In heap_page.cpp
auto HeapPage::lockTuple(uint16_t item_id, LockMode mode, uint32_t proc_id, ErrorContext* ctx) -> Status
{
    // Build lock tag
    LockTag tag;
    tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
    tag.object_uuid = table_id_;
    tag.page_num = header()->page_id;
    tag.offset_num = item_id;

    // Acquire lock via lock manager
    return db_->lock_manager()->acquireLock(proc_id, tag, mode, true, 5000, ctx);
}
```

**Integration with StorageEngine:**

```cpp
// In storage_engine.cpp
auto StorageEngine::deleteTuple(const ID& table_id, uint64_t tid, uint64_t xmax, ErrorContext* ctx) -> Status
{
    uint32_t page_id = tid >> 16;
    uint16_t item_id = tid & 0xFFFF;

    // Get current backend's proc_id
    uint32_t proc_id = db_->transaction_manager()->getMyProcId();

    // Pin page
    void* page_data;
    Status status = buffer_pool_->pinPage(page_id, &page_data, ctx);
    if (status != Status::OK) return status;

    HeapPage heap(reinterpret_cast<uint8_t*>(page_data), db_->page_size(),
                  db_->toast_manager(), db_, table_id);

    // LOCK TUPLE BEFORE DELETING
    status = heap.lockTuple(item_id, LockMode::LOCK_ROW_EXCLUSIVE, proc_id, ctx);
    if (status != Status::OK) {
        buffer_pool_->unpinPage(page_id, false, ctx);
        return status;
    }

    // Now safe to delete
    status = heap.deleteTuple(item_id, xmax, ctx);

    buffer_pool_->unpinPage(page_id, status == Status::OK, ctx);
    return status;
}
```

**Testing:**
- Unit test: Lock conflict matrix correctness
- Unit test: Deadlock detection with synthetic cycles
- Integration test: Concurrent UPDATE to same row
- Stress test: 100 concurrent transactions with row locks

---

## Phase 3: UPDATE and Version Chains (Week 4)

**Goal:** Implement tuple UPDATE with version chain management

**Priority:** CRITICAL (Completes basic MVCC)

### 3.1 TupleHeader Expansion (Days 1-2)

**Changes to `heap_page.h`:**

```cpp
// Expanded tuple header (from 20 bytes to 36 bytes)
#pragma pack(push, 1)
struct TupleHeader {
    // MVCC fields
    uint64_t xmin;                  // Creator XID
    uint64_t xmax;                  // Deleter/Updater XID (0 = live)

    // Version chain
    uint64_t next_version_tid;      // TID of next version (0 = none)

    // Infomask (hint bits)
    uint16_t infomask;              // Status flags
    uint16_t infomask2;             // Additional flags

    // Header metadata
    uint8_t hoff;                   // Header offset to user data
    uint8_t natts;                  // Number of attributes

    // Null bitmap offset
    uint16_t null_bitmap_offset;    // Offset to null bitmap (0 = no nulls)

    static constexpr uint16_t HEAP_HASNULL = 0x0001;
    static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0002;
    static constexpr uint16_t HEAP_XMIN_INVALID = 0x0004;
    static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0008;
    static constexpr uint16_t HEAP_XMAX_INVALID = 0x0010;
    static constexpr uint16_t HEAP_UPDATED = 0x0020;
    static constexpr uint16_t HEAP_MOVED = 0x0040;
    static constexpr uint16_t HEAP_HOT_UPDATED = 0x0080;
    static constexpr uint16_t HEAP_KEYS_UPDATED = 0x0100;
};
#pragma pack(pop)
```

**Migration Strategy:**
- Old TupleHeader (20 bytes) → New TupleHeader (36 bytes)
- Add upgrade flag in DatabaseHeader
- On first open after upgrade, scan all heap pages and rewrite headers

### 3.2 HeapPage::updateTuple() (Days 3-4)

**Implementation:**

```cpp
auto HeapPage::updateTuple(
    uint16_t old_item_id,
    const uint8_t* new_tuple_data,
    uint32_t new_tuple_size,
    uint64_t updater_xid,
    uint16_t* new_item_id_out,
    bool* hot_update_out,
    ErrorContext* ctx) -> Status
{
    // 1. Lock old tuple
    Status status = lockTuple(old_item_id, LockMode::LOCK_ROW_EXCLUSIVE,
                              proc_id, ctx);
    if (status != Status::OK) return status;

    // 2. Check if old tuple is still visible
    const uint8_t* old_data;
    uint32_t old_size;
    status = getTuple(old_item_id, &old_data, &old_size, ctx);
    if (status != Status::OK) return status;

    auto* old_header = reinterpret_cast<const TupleHeader*>(old_data);
    if (old_header->xmax != 0) {
        // Already updated/deleted by another transaction
        return Status::CONCURRENT_MODIFICATION;
    }

    // 3. Determine if HOT update is possible
    // HOT = Heap-Only Tuple (no index update needed)
    bool hot_update = canHotUpdate(old_item_id, new_tuple_data, new_tuple_size);

    // 4. Check if new version fits on same page
    bool same_page = hasFreeSpace(new_tuple_size + sizeof(ItemPointer));

    uint64_t new_tid;
    if (same_page) {
        // 5a. Insert new version on same page
        uint16_t new_item;
        status = insertTuple(new_tuple_data, new_tuple_size, updater_xid,
                             &new_item, ctx);
        if (status != Status::OK) return status;

        new_tid = (static_cast<uint64_t>(header()->page_id) << 16) | new_item;
        *new_item_id_out = new_item;
    } else {
        // 5b. Need new page (handled by StorageEngine)
        return Status::PAGE_FULL;  // Caller will allocate new page
    }

    // 6. Link old tuple to new version
    auto* old_header_mut = const_cast<TupleHeader*>(old_header);
    old_header_mut->xmax = updater_xid;
    old_header_mut->next_version_tid = new_tid;
    old_header_mut->infomask |= HEAP_UPDATED;
    if (hot_update) {
        old_header_mut->infomask |= HEAP_HOT_UPDATED;
    }

    // 7. Mark page dirty
    updateHeaderStats();

    *hot_update_out = hot_update;
    return Status::OK;
}
```

### 3.3 Version Chain Traversal (Day 5)

**Implementation:**

```cpp
auto HeapPage::followVersionChain(
    uint16_t start_item_id,
    uint64_t snapshot_xid,
    std::vector<uint64_t>* visible_tids_out,
    ErrorContext* ctx) -> Status
{
    uint64_t current_tid = (static_cast<uint64_t>(header()->page_id) << 16) | start_item_id;

    while (current_tid != 0) {
        uint32_t page_id = current_tid >> 16;
        uint16_t item_id = current_tid & 0xFFFF;

        // Get tuple header
        const uint8_t* tuple_data;
        uint32_t tuple_size;
        Status status = getTuple(item_id, &tuple_data, &tuple_size, ctx);
        if (status != Status::OK) return status;

        auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);

        // Check visibility
        bool visible = isVersionVisible(header, snapshot_xid);
        if (visible) {
            visible_tids_out->push_back(current_tid);
        }

        // Follow chain
        current_tid = header->next_version_tid;
    }

    return Status::OK;
}

bool HeapPage::isVersionVisible(const TupleHeader* header, uint64_t snapshot_xid) const
{
    // 1. Check if tuple was created after snapshot
    if (header->xmin > snapshot_xid) {
        return false;  // Too new
    }

    // 2. Check if tuple was deleted/updated before snapshot
    if (header->xmax != 0 && header->xmax < snapshot_xid) {
        // Check if xmax was committed
        TransactionState state;
        db_->transaction_manager()->getTransactionState(header->xmax, state, nullptr);
        if (state == TransactionState::COMMITTED) {
            return false;  // Deleted by committed transaction
        }
    }

    // 3. Check xmin committed
    if (!(header->infomask & HEAP_XMIN_COMMITTED)) {
        TransactionState state;
        db_->transaction_manager()->getTransactionState(header->xmin, state, nullptr);
        if (state != TransactionState::COMMITTED) {
            return false;  // Creator not committed
        }
    }

    return true;
}
```

**Integration with StorageEngine:**

```cpp
// In storage_engine.h
Status updateTuple(
    const ID& table_id,
    uint64_t old_tid,
    const uint8_t* new_tuple_data,
    uint32_t new_tuple_size,
    uint64_t updater_xid,
    uint64_t* new_tid_out,
    ErrorContext* ctx);

// In storage_engine.cpp
auto StorageEngine::updateTuple(...) -> Status
{
    uint32_t old_page_id = old_tid >> 16;
    uint16_t old_item_id = old_tid & 0xFFFF;

    // Pin old page
    void* page_data;
    Status status = buffer_pool_->pinPage(old_page_id, &page_data, ctx);
    if (status != Status::OK) return status;

    HeapPage heap(...);

    uint16_t new_item_id;
    bool hot_update;
    status = heap.updateTuple(old_item_id, new_tuple_data, new_tuple_size,
                               updater_xid, &new_item_id, &hot_update, ctx);

    if (status == Status::PAGE_FULL) {
        // Allocate new page for new version
        buffer_pool_->unpinPage(old_page_id, true, ctx);

        uint32_t new_page_id;
        status = allocateHeapPage(table_id, &new_page_id, ctx);
        // ... insert on new page, link old → new
    }

    if (status == Status::OK) {
        *new_tid_out = (static_cast<uint64_t>(old_page_id) << 16) | new_item_id;

        // If not HOT update, update all indexes
        if (!hot_update) {
            status = updateIndexes(table_id, old_tid, *new_tid_out, ctx);
        }
    }

    buffer_pool_->unpinPage(old_page_id, status == Status::OK, ctx);
    return status;
}
```

**Testing:**
- Unit test: updateTuple() creates version chain
- Unit test: followVersionChain() returns correct versions
- Integration test: Concurrent UPDATEs to same row
- Integration test: Snapshot sees correct version

---

## Phase 4: Vacuum Subsystem (Weeks 5-6)

**Goal:** Implement vacuum to reclaim space from dead tuples

**Priority:** CRITICAL (Prevents unbounded database growth)

### 4.1 Vacuum Horizon Calculation (Days 1-2)

**Implementation:**

```cpp
// In proc_array.cpp
auto ProcArrayManager::getVacuumHorizon(uint64_t* horizon_out, ErrorContext* ctx) -> Status
{
    pthread_rwlock_rdlock(&proc_array_->array_lock);

    uint64_t oldest_xmin = UINT64_MAX;

    // Scan all active backends
    for (uint32_t i = 0; i < proc_array_->max_backends; ++i) {
        ProcessControlBlock* pcb = &proc_array_->procs[i];

        if (!pcb->is_active) continue;

        // Consider backend's xmin (snapshot horizon)
        if (pcb->backend_xmin != 0 && pcb->backend_xmin < oldest_xmin) {
            oldest_xmin = pcb->backend_xmin;
        }

        // Consider backend's active transaction
        if (pcb->xid != 0 && pcb->xid < oldest_xmin) {
            oldest_xmin = pcb->xid;
        }
    }

    pthread_rwlock_unlock(&proc_array_->array_lock);

    if (oldest_xmin == UINT64_MAX) {
        // No active transactions, use latest completed
        oldest_xmin = proc_array_->latest_completed_xid;
    }

    *horizon_out = oldest_xmin;
    return Status::OK;
}
```

### 4.2 Dead Tuple Identification (Days 3-4)

**New File:** `include/scratchbird/core/vacuum.h` (200 lines)

```cpp
// Vacuum statistics
struct VacuumStats {
    uint64_t pages_scanned;
    uint64_t tuples_scanned;
    uint64_t dead_tuples_found;
    uint64_t dead_tuples_removed;
    uint64_t pages_pruned;
    uint64_t index_scans;
};

class Vacuum {
public:
    explicit Vacuum(Database* db);

    // Vacuum a single table
    Status vacuumTable(
        const ID& table_id,
        VacuumStats* stats_out,
        ErrorContext* ctx);

    // Vacuum entire database
    Status vacuumDatabase(
        VacuumStats* stats_out,
        ErrorContext* ctx);

private:
    Database* db_;

    // Scan heap for dead tuples
    Status scanHeapForDeadTuples(
        const ID& table_id,
        uint64_t horizon,
        std::vector<uint64_t>* dead_tids_out,
        ErrorContext* ctx);

    // Remove dead tuples from heap
    Status removeDeadTuples(
        const ID& table_id,
        const std::vector<uint64_t>& dead_tids,
        ErrorContext* ctx);

    // Update indexes after vacuum
    Status vacuumIndexes(
        const ID& table_id,
        const std::vector<uint64_t>& dead_tids,
        ErrorContext* ctx);

    // Truncate empty pages at end
    Status truncateEmptyPages(
        const ID& table_id,
        ErrorContext* ctx);
};
```

**Implementation:**

```cpp
auto Vacuum::scanHeapForDeadTuples(
    const ID& table_id,
    uint64_t horizon,
    std::vector<uint64_t>* dead_tids_out,
    ErrorContext* ctx) -> Status
{
    // Get table info from catalog
    TableInfo table_info;
    Status status = db_->catalog_manager()->getTable(table_id, &table_info, ctx);
    if (status != Status::OK) return status;

    // Scan all heap pages
    for (uint32_t page_id = table_info.first_page;
         page_id != 0;
         page_id = getNextPage(page_id)) {

        // Pin page
        void* page_data;
        status = db_->buffer_pool()->pinPage(page_id, &page_data, ctx);
        if (status != Status::OK) continue;

        HeapPage heap(reinterpret_cast<uint8_t*>(page_data),
                      db_->page_size(), nullptr, db_, table_id);

        // Scan tuples on page
        uint16_t item_count = heap.getItemCount();
        for (uint16_t item_id = 0; item_id < item_count; ++item_id) {
            const uint8_t* tuple_data;
            uint32_t tuple_size;
            status = heap.getTuple(item_id, &tuple_data, &tuple_size, ctx);
            if (status != Status::OK) continue;

            auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);

            // Check if dead
            if (isTupleDead(header, horizon)) {
                uint64_t tid = (static_cast<uint64_t>(page_id) << 16) | item_id;
                dead_tids_out->push_back(tid);
            }
        }

        db_->buffer_pool()->unpinPage(page_id, false, ctx);
    }

    return Status::OK;
}

bool Vacuum::isTupleDead(const TupleHeader* header, uint64_t horizon) const
{
    // Tuple is dead if:
    // 1. xmax is set (deleted/updated)
    // 2. xmax < horizon (deleted before oldest snapshot)
    // 3. xmax is committed

    if (header->xmax == 0) {
        return false;  // Not deleted
    }

    if (header->xmax >= horizon) {
        return false;  // Deleted too recently
    }

    // Check if xmax committed
    if (header->infomask & TupleHeader::HEAP_XMAX_COMMITTED) {
        return true;
    }

    TransactionState state;
    db_->transaction_manager()->getTransactionState(header->xmax, state, nullptr);
    return (state == TransactionState::COMMITTED);
}
```

### 4.3 Space Reclamation (Days 5-7)

**Implementation:**

```cpp
auto Vacuum::removeDeadTuples(
    const ID& table_id,
    const std::vector<uint64_t>& dead_tids,
    ErrorContext* ctx) -> Status
{
    // Group dead TIDs by page
    std::unordered_map<uint32_t, std::vector<uint16_t>> tids_by_page;
    for (uint64_t tid : dead_tids) {
        uint32_t page_id = tid >> 16;
        uint16_t item_id = tid & 0xFFFF;
        tids_by_page[page_id].push_back(item_id);
    }

    // Process each page
    for (const auto& [page_id, item_ids] : tids_by_page) {
        void* page_data;
        Status status = db_->buffer_pool()->pinPage(page_id, &page_data, ctx);
        if (status != Status::OK) continue;

        HeapPage heap(...);

        // Compact page (remove dead tuples)
        status = heap.compactPage(item_ids, ctx);

        db_->buffer_pool()->unpinPage(page_id, status == Status::OK, ctx);
    }

    return Status::OK;
}

auto HeapPage::compactPage(
    const std::vector<uint16_t>& dead_items,
    ErrorContext* ctx) -> Status
{
    // 1. Build list of live tuples
    std::vector<std::pair<uint16_t, std::vector<uint8_t>>> live_tuples;

    uint16_t item_count = getItemCount();
    for (uint16_t i = 0; i < item_count; ++i) {
        if (std::find(dead_items.begin(), dead_items.end(), i) != dead_items.end()) {
            continue;  // Skip dead tuple
        }

        const uint8_t* tuple_data;
        uint32_t tuple_size;
        Status status = getTuple(i, &tuple_data, &tuple_size, ctx);
        if (status != Status::OK) continue;

        std::vector<uint8_t> tuple_copy(tuple_data, tuple_data + tuple_size);
        live_tuples.push_back({i, std::move(tuple_copy)});
    }

    // 2. Clear page (keep header)
    auto* special = getSpecial();
    special->pd_lower = sizeof(PageHeader);
    special->pd_upper = page_size_ - sizeof(HeapPageSpecial);

    // 3. Re-insert live tuples
    for (const auto& [old_item_id, tuple_data] : live_tuples) {
        uint16_t new_item_id;
        // Extract xmin from tuple header
        auto* header = reinterpret_cast<const TupleHeader*>(tuple_data.data());
        insertTuple(tuple_data.data(), tuple_data.size(), header->xmin,
                    &new_item_id, ctx);
    }

    updateHeaderStats();
    return Status::OK;
}
```

### 4.4 Auto-Vacuum (Days 8-10)

**New File:** `src/core/autovacuum.cpp` (400 lines)

```cpp
class AutoVacuum {
public:
    explicit AutoVacuum(Database* db);
    ~AutoVacuum();

    // Start background worker
    Status start(ErrorContext* ctx);

    // Stop background worker
    Status stop(ErrorContext* ctx);

private:
    Database* db_;
    std::thread worker_thread_;
    std::atomic<bool> running_;

    // Worker thread function
    void workerLoop();

    // Decide which tables need vacuum
    void selectTablesToVacuum(std::vector<ID>* tables_out);

    // Configuration
    uint32_t autovacuum_naptime_ms_ = 60000;  // 1 minute
    float vacuum_threshold_ratio_ = 0.2;       // 20% dead tuples
};

void AutoVacuum::workerLoop()
{
    while (running_) {
        // Sleep
        std::this_thread::sleep_for(
            std::chrono::milliseconds(autovacuum_naptime_ms_));

        if (!running_) break;

        // Select tables to vacuum
        std::vector<ID> tables;
        selectTablesToVacuum(&tables);

        // Vacuum each table
        Vacuum vacuum(db_);
        for (const ID& table_id : tables) {
            VacuumStats stats;
            vacuum.vacuumTable(table_id, &stats, nullptr);
        }
    }
}
```

**Testing:**
- Unit test: isTupleDead() correctness
- Unit test: compactPage() removes dead tuples
- Integration test: Vacuum after 1000 DELETEs
- Integration test: Auto-vacuum triggers automatically

---

## Phase 5: CLOG Optimization (Week 7)

**Goal:** Convert TIP to CLOG with 2-bit encoding (160x space savings)

**Priority:** MEDIUM (Performance optimization)

### 5.1 CLOG Implementation (Days 1-3)

**New File:** `include/scratchbird/core/clog.h` (150 lines)

```cpp
// CLOG page (8KB = 32,768 transactions with 2 bits each)
#pragma pack(push, 1)
struct ClogPage {
    PageHeader header;
    uint64_t oldest_xid;         // First XID on this page
    uint64_t newest_xid;         // Last XID on this page
    uint8_t status_data[8092];   // 2 bits per XID
};
#pragma pack(pop)

// Transaction status (2 bits)
enum class ClogStatus : uint8_t {
    ACTIVE = 0,
    COMMITTED = 1,
    ABORTED = 2,
    SUB_COMMITTED = 3
};

class Clog {
public:
    explicit Clog(Database* db);

    // Set transaction status
    Status setStatus(uint64_t xid, ClogStatus status, ErrorContext* ctx);

    // Get transaction status
    Status getStatus(uint64_t xid, ClogStatus* status_out, ErrorContext* ctx);

private:
    Database* db_;
    uint32_t clog_root_page_;

    // CLOG page capacity
    static constexpr uint32_t XIDS_PER_PAGE = 32768;

    // Calculate page and offset for XID
    uint32_t getPageForXid(uint64_t xid) const {
        return clog_root_page_ + (xid / XIDS_PER_PAGE);
    }

    uint32_t getOffsetInPage(uint64_t xid) const {
        return (xid % XIDS_PER_PAGE);
    }

    // Encode/decode 2-bit status
    void setStatusBits(uint8_t* data, uint32_t offset, ClogStatus status);
    ClogStatus getStatusBits(const uint8_t* data, uint32_t offset) const;
};
```

### 5.2 Migration from TIP to CLOG (Days 4-5)

**Implementation:**

```cpp
auto Database::migrateTipToClog(ErrorContext* ctx) -> Status
{
    // 1. Read all TIP entries
    std::vector<std::pair<uint64_t, TransactionState>> tip_entries;
    Status status = readAllTipEntries(&tip_entries, ctx);
    if (status != Status::OK) return status;

    // 2. Allocate CLOG pages
    Clog clog(this);
    status = clog.initialize(ctx);
    if (status != Status::OK) return status;

    // 3. Convert each TIP entry to CLOG
    for (const auto& [xid, state] : tip_entries) {
        ClogStatus clog_status;
        switch (state) {
            case TransactionState::ACTIVE: clog_status = ClogStatus::ACTIVE; break;
            case TransactionState::COMMITTED: clog_status = ClogStatus::COMMITTED; break;
            case TransactionState::ABORTED: clog_status = ClogStatus::ABORTED; break;
            case TransactionState::PREPARED: clog_status = ClogStatus::SUB_COMMITTED; break;
        }

        status = clog.setStatus(xid, clog_status, ctx);
        if (status != Status::OK) return status;
    }

    // 4. Free old TIP pages
    status = freeAllTipPages(ctx);

    // 5. Update database header
    db_header_->clog_root_page = clog.getRootPage();
    db_header_->flags |= DB_FLAG_USES_CLOG;

    return sync(ctx);
}
```

**Testing:**
- Unit test: 2-bit encoding/decoding
- Unit test: CLOG page capacity (32,768 XIDs)
- Integration test: Migration from TIP to CLOG
- Performance test: CLOG vs TIP lookup speed

---

## Phase 6: Testing and Documentation (Week 7)

**Goal:** Comprehensive test coverage and documentation

### 6.1 Unit Tests (1000 lines)

**New File:** `tests/unit/test_mga.cpp`

```cpp
// Transaction Manager Tests
TEST(TransactionManagerTest, MultipleActiveTransactions) {
    // Test 100 concurrent transactions
}

TEST(TransactionManagerTest, SnapshotIsolation) {
    // Test snapshot sees consistent view
}

// Lock Manager Tests
TEST(LockManagerTest, TableLockConflicts) {
    // Test all 8 lock mode conflicts
}

TEST(LockManagerTest, RowLockConflicts) {
    // Test row-level lock conflicts
}

TEST(LockManagerTest, DeadlockDetection) {
    // Create cycle: T1 waits T2, T2 waits T1
}

// Version Chain Tests
TEST(VersionChainTest, UpdateCreatesChain) {
    // UPDATE should link old → new
}

TEST(VersionChainTest, SnapshotSeesCorrectVersion) {
    // Snapshot should see correct version in chain
}

// Vacuum Tests
TEST(VacuumTest, RemovesDeadTuples) {
    // Vacuum should reclaim space
}

TEST(VacuumTest, RespectsHorizon) {
    // Vacuum should not remove visible tuples
}
```

### 6.2 Integration Tests (1000 lines)

**New File:** `tests/integration/test_mga_concurrency.cpp`

```cpp
TEST(MGAConcurrencyTest, ConcurrentInserts) {
    // 10 threads inserting 1000 rows each
}

TEST(MGAConcurrencyTest, ConcurrentUpdates) {
    // 5 threads updating same rows
}

TEST(MGAConcurrencyTest, ReadWriteConflicts) {
    // Readers should not block writers
}

TEST(MGAConcurrencyTest, LongRunningSnapshot) {
    // Long snapshot should block vacuum
}
```

### 6.3 Stress Tests

**New File:** `tests/stress/test_mga_stress.cpp`

```cpp
TEST(MGAStressTest, ThousandConcurrentTransactions) {
    // 1000 concurrent transactions
}

TEST(MGAStressTest, MillionRowUpdateAndVacuum) {
    // Update 1M rows, vacuum, verify
}
```

### 6.4 Documentation Updates

**Update Files:**
- `MGA_STATUS.md` - New status document
- `docs/specifications/MGA_IMPLEMENTATION.md` - Update with actual implementation
- `docs/api/TRANSACTION_API.md` - API documentation
- `docs/api/LOCK_MANAGER_API.md` - Lock manager API

---

## Implementation Checklist

### Phase 1: Multi-Connection Foundation ✓
- [ ] Implement ProcArray data structure
- [ ] Add shared memory allocation in Database class
- [ ] Implement backend registration/unregistration
- [ ] Remove single-connection restriction from TransactionManager
- [ ] Update snapshot management to scan ProcArray
- [ ] Test: 100 concurrent backends
- [ ] Test: Multi-connection transaction isolation

### Phase 2: Lock Manager ✓
- [ ] Implement lock data structures (LockTag, Lock, LockRequest)
- [ ] Implement lock conflict matrix
- [ ] Implement acquireLock() with queuing
- [ ] Implement releaseLock() with queue processing
- [ ] Implement deadlock detection (wait-for graph)
- [ ] Integrate row locks with HeapPage
- [ ] Integrate table locks with StorageEngine
- [ ] Test: Lock conflicts
- [ ] Test: Deadlock detection
- [ ] Test: Concurrent row locks

### Phase 3: UPDATE and Version Chains ✓
- [ ] Expand TupleHeader with next_version_tid and infomask
- [ ] Implement HeapPage::updateTuple()
- [ ] Implement version chain creation
- [ ] Implement followVersionChain()
- [ ] Implement isVersionVisible()
- [ ] Add StorageEngine::updateTuple()
- [ ] Integrate with index updates
- [ ] Test: Update creates chain
- [ ] Test: Snapshot sees correct version
- [ ] Test: Concurrent updates

### Phase 4: Vacuum Subsystem ✓
- [ ] Implement getVacuumHorizon() in ProcArray
- [ ] Implement Vacuum class with scanHeapForDeadTuples()
- [ ] Implement isTupleDead() logic
- [ ] Implement removeDeadTuples() with compactPage()
- [ ] Implement vacuumIndexes() for index cleanup
- [ ] Implement truncateEmptyPages()
- [ ] Implement AutoVacuum background worker
- [ ] Test: Vacuum removes dead tuples
- [ ] Test: Vacuum respects horizon
- [ ] Test: Auto-vacuum triggers

### Phase 5: CLOG Optimization ✓
- [ ] Implement CLOG with 2-bit encoding
- [ ] Implement setStatus() and getStatus()
- [ ] Implement migration from TIP to CLOG
- [ ] Test: 2-bit encoding correctness
- [ ] Test: CLOG capacity (32K XIDs per page)
- [ ] Test: Migration preserves all transactions

### Phase 6: Testing and Documentation ✓
- [ ] Write 20+ unit tests
- [ ] Write 10+ integration tests
- [ ] Write 5+ stress tests
- [ ] Update MGA_STATUS.md
- [ ] Update specification documents
- [ ] Write API documentation
- [ ] Create user guide for transactions

---

## Risk Assessment

### High Risk:
1. **Deadlock Detection Complexity**
   - Mitigation: Start with timeout-based detection, add graph later

2. **Version Chain Performance**
   - Mitigation: Implement HOT updates to reduce index overhead

3. **Vacuum Blocking**
   - Mitigation: Implement incremental vacuum with small batches

### Medium Risk:
4. **Lock Manager Memory Usage**
   - Mitigation: Use lock pools with fixed allocation

5. **CLOG Migration**
   - Mitigation: Support both TIP and CLOG during transition

### Low Risk:
6. **ProcArray Scalability**
   - Mitigation: Use read-write locks for scan performance

---

## Success Criteria

### Phase 1 Complete When:
✅ 100+ concurrent backends registered
✅ Multiple active transactions tracked
✅ Snapshot scans all active XIDs

### Phase 2 Complete When:
✅ Table locks with all 8 modes working
✅ Row locks prevent concurrent updates
✅ Deadlock detection breaks cycles
✅ Integration test passes with 50 concurrent transactions

### Phase 3 Complete When:
✅ UPDATE creates version chains
✅ Snapshot sees correct version in chain
✅ HOT updates work (no index update)
✅ Concurrent updates don't corrupt data

### Phase 4 Complete When:
✅ Vacuum removes dead tuples
✅ Vacuum respects snapshot horizon
✅ Auto-vacuum runs in background
✅ Database size stabilizes after 1M updates

### Phase 5 Complete When:
✅ CLOG uses 2 bits per transaction
✅ Migration from TIP succeeds
✅ CLOG performance > TIP performance

### Full MGA Complete When:
✅ All 6 phases done
✅ 100% of tests pass
✅ 1000 concurrent connections stable
✅ Vacuum prevents unbounded growth
✅ Documentation complete

---

## Timeline Summary

| Phase | Duration | Priority | LOC | Completion Criteria |
|-------|----------|----------|-----|---------------------|
| **Phase 1: Multi-Connection** | 1 week | CRITICAL | 800 | 100+ concurrent backends |
| **Phase 2: Lock Manager** | 2 weeks | CRITICAL | 2,500 | Deadlock detection working |
| **Phase 3: Version Chains** | 1 week | CRITICAL | 1,200 | UPDATE creates chains |
| **Phase 4: Vacuum** | 1.5 weeks | CRITICAL | 2,000 | Dead tuples reclaimed |
| **Phase 5: CLOG** | 2 days | MEDIUM | 600 | 160x space savings |
| **Phase 6: Testing** | 1 week | HIGH | 2,000 | All tests pass |
| **TOTAL** | **~7 weeks** | - | **~9,100** | Full MGA working |

---

## Post-Implementation

### Performance Optimization:
- Profile lock manager for hotspots
- Optimize version chain traversal
- Tune vacuum thresholds
- Add lock partitioning for scalability

### Future Enhancements:
- Savepoints and subtransactions
- Two-phase commit completion
- Careful writes / double-write buffer
- Distributed transaction coordination
- Parallel vacuum
- Online index builds

---

## References

- Gap Analysis: `MGA_GAP_ANALYSIS.md`
- Specification: `docs/specifications/Specification for a Multi-Generational Database Architecture.md`
- Current Code: `transaction_manager.cpp`, `heap_page.cpp`, `storage_engine.cpp`
- PostgreSQL Reference: https://www.postgresql.org/docs/current/mvcc.html
- Firebird MGA: https://firebirdsql.org/file/documentation/papers_presentations/html/paper-mva.html

