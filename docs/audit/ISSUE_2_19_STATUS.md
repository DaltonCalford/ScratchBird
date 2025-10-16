# Issue 2.19: Group Commit Implementation

## Issue Summary
**File**: `src/core/transaction_manager.cpp:324-395`
**Severity**: MAJOR
**Spec Reference**: PostgreSQL group commit architecture

**Original Issue**: Each commit forces individual TIP write and fsync, causing:
- High I/O overhead (one fsync per commit)
- Low throughput on small transactions
- Performance degradation under concurrent workload
- Unnecessary disk contention

## Current Implementation Status: NOT IMPLEMENTED

### Analysis Date: 2025-10-16

**Current Behavior**: ❌ NO GROUP COMMIT
- Each commit writes its TIP entry individually (line 363)
- Each commit calls `db_->sync()` individually (line 374)
- Structure: One commit = One TIP write + One fsync
- Under concurrent load: N commits = N TIP writes + N fsyncs

**Key Code Locations**:
- **Commit Flow**: transaction_manager.cpp:324-395 (commitTransaction)
  - Line 363: `writeTipEntry(xid, TransactionState::COMMITTED, ctx)` - individual write
  - Line 374: `status = db_->sync(ctx)` - individual fsync
- **TIP Write**: transaction_manager.cpp:341-461 (writeTipEntry)
  - Searches TIP page chain to find/update XID entry
  - Writes one transaction at a time

## Group Commit Algorithm: Leader-Follower Pattern

### Why Leader-Follower?

**Problem**: Multiple concurrent commits all waiting for fsync
- Traditional approach: Each commit does its own TIP write + fsync
- Waste: fsync is expensive (~5-10ms), can batch multiple commits into one fsync

**Solution**: Group commits together
- First committer becomes "leader"
- Leader collects all waiting commits into a batch
- Leader writes all TIDs in batch + single fsync
- Leader wakes all followers with success/failure

**Expected Performance**:
- **10x throughput improvement** on small concurrent transactions
- **50-100x reduction** in fsync operations under high concurrency
- PostgreSQL achieves similar improvements with this technique

### Architecture Components

#### 1. Commit Wait Queue

```cpp
struct CommitWaiter {
    uint64_t xid;                           // Transaction ID to commit
    TransactionState state;                  // State to write (COMMITTED/ABORTED)
    Status result;                           // Result from leader (OK/ERROR)
    std::condition_variable cv;              // Condition variable to wake waiter
    std::mutex cv_mutex;                     // Mutex for condition variable
    bool completed;                          // Set to true when leader finishes

    CommitWaiter(uint64_t xid_, TransactionState state_)
        : xid(xid_), state(state_), result(Status::OK), completed(false) {}
};
```

**Purpose**: Hold transactions waiting to be committed in a batch

#### 2. Leader Election

```cpp
// In TransactionManager class
std::mutex group_commit_mutex_;              // Protects group commit queue
std::vector<CommitWaiter*> commit_queue_;    // Queue of waiting commits
bool group_commit_in_progress_;              // True if leader is processing
std::atomic<bool> group_commit_enabled_{true};  // Configuration flag
uint64_t group_commit_timeout_us_{10000};    // Wait up to 10ms for batch (configurable)
```

**Leader Election**: First waiter to acquire `group_commit_mutex_` and see `!group_commit_in_progress_` becomes leader

#### 3. Batch Write Flow

```
┌─────────────┐
│ Commit #1   │──┐
│ (XID 100)   │  │
└─────────────┘  │
                 ├─► Leader Election
┌─────────────┐  │   (Commit #1 becomes leader)
│ Commit #2   │──┤
│ (XID 101)   │  │         ▼
└─────────────┘  │   ┌──────────────────┐
                 │   │ Commit #1 waits  │
┌─────────────┐  │   │ for batch window │
│ Commit #3   │──┤   │ (10ms or N=32)   │
│ (XID 102)   │  │   └──────────────────┘
└─────────────┘  │         │
                 │         ▼
       More commits arrive...
                           │
                           ▼
                ┌────────────────────┐
                │ Leader collects:   │
                │ - XID 100 COMMIT   │
                │ - XID 101 COMMIT   │
                │ - XID 102 COMMIT   │
                │ - XID 103 COMMIT   │
                └────────────────────┘
                           │
                           ▼
                ┌────────────────────┐
                │ Batch TIP Write    │
                │ (4 XIDs at once)   │
                └────────────────────┘
                           │
                           ▼
                ┌────────────────────┐
                │ Single fsync()     │
                │ (for all 4 XIDs)   │
                └────────────────────┘
                           │
                           ▼
                ┌────────────────────┐
                │ Wake all waiters   │
                │ with Status::OK    │
                └────────────────────┘
```

### Batch Collection Heuristics

**When to flush batch**:
1. **Timeout**: Wait up to 10ms for more commits (configurable via `group_commit_timeout_us`)
2. **Batch Size**: Flush when 32 commits collected (configurable via `group_commit_batch_size`)
3. **Leader Election**: If no commits arrive within timeout, flush immediately

**Why these values**:
- 10ms timeout: Balance latency vs. batching (PostgreSQL uses similar)
- 32 commits: Good batch size for modern storage (NVMe handles batches well)
- Configurable: Workload-dependent tuning

## Implementation Plan

### Phase 1: Data Structures (1 day)

**File**: `include/scratchbird/core/transaction_manager.h`

1. **Add CommitWaiter structure**
   ```cpp
   struct CommitWaiter {
       uint64_t xid;
       TransactionState state;
       Status result;
       std::condition_variable cv;
       std::mutex cv_mutex;
       bool completed;

       CommitWaiter(uint64_t xid_, TransactionState state_);
   };
   ```

2. **Add group commit state to TransactionManager**
   ```cpp
   // Group commit infrastructure
   std::mutex group_commit_mutex_;
   std::vector<CommitWaiter*> commit_queue_;
   bool group_commit_in_progress_{false};
   std::atomic<bool> group_commit_enabled_{true};
   uint64_t group_commit_timeout_us_{10000};    // 10ms default
   uint32_t group_commit_batch_size_{32};       // 32 commits default

   // Statistics
   std::atomic<uint64_t> group_commits_performed_{0};
   std::atomic<uint64_t> group_commit_total_xids_{0};
   ```

### Phase 2: Batch Write Function (1 day)

**File**: `src/core/transaction_manager.cpp`

3. **Implement writeTipEntriesBatch()**
   ```cpp
   // Write multiple TIP entries in a single pass
   // Returns Status::OK if all writes succeed
   auto TransactionManager::writeTipEntriesBatch(
       const std::vector<std::pair<uint64_t, TransactionState>>& batch,
       ErrorContext* ctx) -> Status
   {
       // Sort by XID for efficient TIP page traversal
       // Write all entries to their respective TIP pages
       // Update checksums
       // Return Status::OK on success
   }
   ```

   **Implementation Details**:
   - Sort batch by XID to minimize page pin/unpin cycles
   - Traverse TIP page chain once, writing all matching XIDs
   - Handle page allocation if TIP page is full
   - Update page checksums after all writes

### Phase 3: Group Commit Leader Function (2 days)

**File**: `src/core/transaction_manager.cpp`

4. **Implement performGroupCommit()**
   ```cpp
   // Leader function: collect batch, write TIDs, fsync, wake waiters
   auto TransactionManager::performGroupCommit(CommitWaiter* leader_waiter,
                                               ErrorContext* ctx) -> Status
   {
       // 1. Collect batch of waiting commits
       std::vector<CommitWaiter*> batch;
       batch.push_back(leader_waiter);

       // 2. Wait for more commits (with timeout)
       auto deadline = std::chrono::steady_clock::now() +
                       std::chrono::microseconds(group_commit_timeout_us_);

       while (std::chrono::steady_clock::now() < deadline &&
              batch.size() < group_commit_batch_size_) {
           std::lock_guard<std::mutex> lock(group_commit_mutex_);

           // Collect all waiting commits from queue
           while (!commit_queue_.empty() && batch.size() < group_commit_batch_size_) {
               batch.push_back(commit_queue_.back());
               commit_queue_.pop_back();
           }

           // If we have a good batch, break early
           if (batch.size() >= group_commit_batch_size_ / 2) {
               break;
           }

           // Sleep briefly (1ms) to allow more commits to arrive
           std::this_thread::sleep_for(std::chrono::milliseconds(1));
       }

       // 3. Write all TIP entries in batch
       std::vector<std::pair<uint64_t, TransactionState>> xid_batch;
       for (auto* waiter : batch) {
           xid_batch.push_back({waiter->xid, waiter->state});
       }

       Status status = writeTipEntriesBatch(xid_batch, ctx);

       // 4. Single fsync for entire batch
       if (status == Status::OK) {
           status = db_->sync(ctx);
       }

       // 5. Wake all waiters with result
       for (auto* waiter : batch) {
           std::lock_guard<std::mutex> lock(waiter->cv_mutex);
           waiter->result = status;
           waiter->completed = true;
           waiter->cv.notify_one();
       }

       // 6. Update statistics
       group_commits_performed_.fetch_add(1);
       group_commit_total_xids_.fetch_add(batch.size());

       return status;
   }
   ```

### Phase 4: Integrate into commitTransaction() (1 day)

**File**: `src/core/transaction_manager.cpp`

5. **Modify commitTransaction()** (lines 324-395)
   ```cpp
   auto TransactionManager::commitTransaction(uint32_t proc_id, uint64_t xid,
                                              ErrorContext* ctx) -> Status
   {
       Status status;

       // Perform pre-commit work within mutex
       {
           std::lock_guard<std::mutex> lock(mutex_);

           // Update cache
           auto cache_it = transaction_cache_.find(xid);
           if (cache_it != transaction_cache_.end()) {
               cache_it->second = TransactionState::COMMITTED;
               touchCacheEntry(xid);
           } else {
               addToCacheLRU(xid, TransactionState::COMMITTED);
           }

           // Write to CLOG
           status = db_->clog()->setStatus(xid, ClogStatus::COMMITTED, ctx);
           if (status != Status::OK) {
               // Rollback logic...
               return status;
           }

           stats_.transactions_committed++;
       }
       // Mutex released - don't hold during I/O!

       // GROUP COMMIT OPTIMIZATION
       if (group_commit_enabled_.load(std::memory_order_acquire)) {
           // Create waiter for this commit
           CommitWaiter waiter(xid, TransactionState::COMMITTED);

           bool is_leader = false;

           // Try to become leader
           {
               std::lock_guard<std::mutex> lock(group_commit_mutex_);

               if (!group_commit_in_progress_) {
                   // Become leader
                   is_leader = true;
                   group_commit_in_progress_ = true;
               } else {
                   // Join queue as follower
                   commit_queue_.push_back(&waiter);
               }
           }

           if (is_leader) {
               // Perform group commit as leader
               status = performGroupCommit(&waiter, ctx);

               // Mark group commit complete
               {
                   std::lock_guard<std::mutex> lock(group_commit_mutex_);
                   group_commit_in_progress_ = false;
               }
           } else {
               // Wait for leader to complete
               std::unique_lock<std::mutex> lock(waiter.cv_mutex);
               waiter.cv.wait(lock, [&waiter] { return waiter.completed; });
               status = waiter.result;
           }
       } else {
           // Fallback: Traditional individual commit (for testing/debugging)
           status = writeTipEntry(xid, TransactionState::COMMITTED, ctx);
           if (status != Status::OK) {
               LOG_WARNING(TRANSACTION, "Failed to update TIP entry for committed XID %lu", xid);
           }
           status = db_->sync(ctx);
       }

       // Clear ProcArray slot after durability guaranteed
       Status clear_status = ProcArrayManager::clearTransactionId(proc_id, ctx);
       if (clear_status != Status::OK) {
           LOG_WARNING(TRANSACTION, "Failed to clear ProcArray slot for committed XID %lu", xid);
       }

       // Check sweep trigger (non-blocking)
       if (status == Status::OK && db_->sweep_manager()) {
           db_->sweep_manager()->checkSweepTrigger(ctx);
       }

       return status;
   }
   ```

### Phase 5: Configuration and Tuning (1 day)

**File**: `include/scratchbird/core/config.h`

6. **Add configuration parameters**
   ```cpp
   namespace config {
       // Group commit configuration
       constexpr bool DEFAULT_GROUP_COMMIT_ENABLED = true;
       constexpr uint64_t DEFAULT_GROUP_COMMIT_TIMEOUT_US = 10000;  // 10ms
       constexpr uint32_t DEFAULT_GROUP_COMMIT_BATCH_SIZE = 32;     // 32 commits
   }
   ```

7. **Add enable/disable methods**
   ```cpp
   // In TransactionManager class
   void enableGroupCommit(bool enabled) {
       group_commit_enabled_.store(enabled, std::memory_order_release);
   }

   void setGroupCommitTimeout(uint64_t timeout_us) {
       group_commit_timeout_us_ = timeout_us;
   }

   void setGroupCommitBatchSize(uint32_t batch_size) {
       group_commit_batch_size_ = batch_size;
   }

   auto getGroupCommitStats() const -> std::pair<uint64_t, uint64_t> {
       return {group_commits_performed_.load(), group_commit_total_xids_.load()};
   }
   ```

### Phase 6: Testing (2 days)

**File**: `tests/unit/test_group_commit.cpp` (NEW)

8. **Unit tests for group commit**
   - Test leader election (first waiter becomes leader)
   - Test follower queueing (subsequent waiters join queue)
   - Test batch collection (timeout and batch size limits)
   - Test batch TIP write (multiple XIDs written together)
   - Test single fsync (verify only one fsync per batch)

9. **Integration tests**
   - Test concurrent commits (spawn 100 threads, commit simultaneously)
   - Test mixed commit/rollback batches
   - Test error handling (TIP write failure, fsync failure)
   - Test fallback to individual commits (when group commit disabled)

10. **Performance benchmarks**
    - Measure throughput: commits/second with vs. without group commit
    - Measure fsync reduction: count fsync calls under load
    - Measure latency: p50, p95, p99 commit latency
    - **Target**: 10x throughput improvement under concurrent load

**File**: `tools/benchmark_group_commit.cpp` (NEW)

11. **Benchmark tool**
    ```cpp
    // Spawn N threads, each committing M transactions
    // Measure:
    // - Total throughput (commits/sec)
    // - Average batch size
    // - fsync count reduction
    // - p50/p95/p99 latency
    //
    // Run with group commit ON and OFF to compare
    ```

## Expected Benefits

Based on PostgreSQL and other database implementations:

### Throughput Improvements
- **Low concurrency (1-4 threads)**: 2-3x improvement
- **Medium concurrency (8-16 threads)**: 5-10x improvement
- **High concurrency (32+ threads)**: 10-50x improvement

### I/O Reduction
- **fsync calls**: 50-100x reduction under high concurrency
- **Disk writes**: Slightly reduced (TIP writes still per-page)
- **Write amplification**: Significantly reduced

### Latency
- **p50 latency**: May increase slightly (1-10ms due to batching wait)
- **p95 latency**: Should improve (less contention)
- **p99 latency**: Should improve significantly (no fsync storms)

### Example: 32 Concurrent Commits

**Without group commit**:
- 32 commits × 1 TIP write = 32 TIP writes
- 32 commits × 1 fsync = 32 fsyncs (~5ms each = 160ms total)
- **Throughput**: 32 commits / 160ms = 200 commits/sec

**With group commit**:
- 1 batch × 32 TIP writes = 32 TIP writes (same, but batched)
- 1 fsync = 5ms total
- **Throughput**: 32 commits / 5ms = 6,400 commits/sec
- **Improvement**: 32x throughput, 32x fewer fsyncs

## Files to Modify/Create

### Modified Files
1. **include/scratchbird/core/transaction_manager.h** (~50 lines added)
   - CommitWaiter structure
   - Group commit state members
   - Group commit method declarations

2. **src/core/transaction_manager.cpp** (~250-300 lines added/modified)
   - writeTipEntriesBatch() (~80 lines)
   - performGroupCommit() (~100 lines)
   - Modified commitTransaction() (~50 lines changed)
   - Modified rollbackTransaction() (optional, ~30 lines changed)

3. **include/scratchbird/core/config.h** (~10 lines added)
   - Group commit configuration constants

### New Files
1. **tests/unit/test_group_commit.cpp** (~500-600 lines)
   - Unit tests for group commit logic
   - Integration tests for concurrent commits
   - Error handling tests

2. **tools/benchmark_group_commit.cpp** (~300-400 lines)
   - Multi-threaded benchmark tool
   - Performance measurement and reporting

## Estimated Effort

**Total Time**: 8-9 days (1.5-2 weeks)

- **Phase 1** (Data Structures): 1 day
- **Phase 2** (Batch Write): 1 day
- **Phase 3** (Leader Function): 2 days
- **Phase 4** (Integration): 1 day
- **Phase 5** (Configuration): 1 day
- **Phase 6** (Testing): 2-3 days

**Risk Level**: MEDIUM
- Well-understood algorithm (PostgreSQL reference implementation)
- Concurrency complexity (mutexes, condition variables, leader election)
- Performance critical (must not regress single-threaded case)
- Extensive testing required (race conditions, deadlocks)

## Safety Considerations

### Correctness Guarantees

1. **Atomicity**: Each transaction's commit is atomic (all or nothing)
2. **Durability**: fsync ensures batch is durable before returning success
3. **Isolation**: Group commit does not affect transaction isolation
4. **No Data Loss**: If leader fails, all followers get failure status

### Failure Scenarios

1. **TIP Write Failure**: All waiters in batch get Status::ERROR
2. **fsync Failure**: All waiters in batch get Status::ERROR
3. **Leader Thread Crash**: Need timeout mechanism for followers
4. **Deadlock Prevention**: Never hold `mutex_` and `group_commit_mutex_` simultaneously

### Rollback Handling

**Question**: Should rollback also use group commit?

**Answer**: YES, for consistency and performance
- Rollbacks also write to TIP and call fsync
- Same batching benefits apply
- Implementation: `rollbackTransaction()` follows same leader-follower pattern

## Implementation Status

**Status**: ✅ IMPLEMENTED & COMPILED (2025-10-16)
**Build Status**: Core library builds successfully (libscratchbird_core.a)
**Group Commit Status**: ENABLED BY DEFAULT

### Implementation Summary

**Phase 1: Data Structures** ✅ COMPLETE
- CommitWaiter structure implemented (transaction_manager.h:220-234)
- Group commit state members added (transaction_manager.h:256-266)
- Configuration and statistics members added

**Phase 2: Batch Write Function** ✅ COMPLETE
- `writeTipEntriesBatch()` implemented (transaction_manager.cpp:1109-1137)
- Sorts XIDs for efficient page traversal
- Reuses `writeTipEntry()` for individual writes within batch

**Phase 3: Group Commit Leader Function** ✅ COMPLETE
- `performGroupCommit()` implemented (transaction_manager.cpp:1139-1226)
- Collects batch with timeout (10ms default) and size limit (32 commits default)
- Single fsync for entire batch (KEY OPTIMIZATION!)
- Wakes all waiters with success/failure status
- Tracks statistics (group commits performed, total XIDs)

**Phase 4: Integration** ✅ COMPLETE
- `commitTransaction()` rewritten with group commit (transaction_manager.cpp:325-434)
- Leader election pattern: first waiter becomes leader
- Followers join queue and wait for leader to complete
- Fallback to individual commits when group commit disabled
- Maintains compatibility with Issue 1.14 fix (clear ProcArray after sync)

**Phase 5: Configuration** ✅ COMPLETE
- Group commit enabled by default (`group_commit_enabled_ = true`)
- Configurable timeout: 10ms default (`group_commit_timeout_us_`)
- Configurable batch size: 32 commits default (`group_commit_batch_size_`)
- Control methods: `enableGroupCommit()`, `setGroupCommitTimeout()`, `setGroupCommitBatchSize()`
- Statistics: `getGroupCommitStats()` returns (commits performed, total XIDs)

**Phase 6: Testing** ⏳ PENDING
- Unit tests for group commit logic - TO DO
- Integration tests for concurrent commits - TO DO
- Performance benchmarks - TO DO

### Files Modified

**Header Files**:
1. `include/scratchbird/core/transaction_manager.h` (~50 lines added)
   - Added CommitWaiter structure (lines 220-234)
   - Added group commit state members (lines 256-266)
   - Added group commit control methods (lines 197-217)
   - Added group commit method declarations (lines 295-298)

**Source Files**:
2. `src/core/transaction_manager.cpp` (~200 lines added/modified)
   - Added `#include <thread>` for sleep_for (line 16)
   - Added `writeTipEntriesBatch()` function (lines 1109-1137)
   - Added `performGroupCommit()` function (lines 1139-1226)
   - Rewritten `commitTransaction()` with group commit (lines 325-434)

### Next Steps
1. Create unit tests in `tests/unit/test_group_commit.cpp`
2. Create performance benchmarks in `tools/benchmark_group_commit.cpp`
3. Measure throughput improvement under concurrent load
4. Document configuration parameters in README

---

**Document Version**: 1.0
**Last Updated**: 2025-10-16
**Author**: Claude (AI Assistant)
**Review Status**: Design complete, ready for implementation
