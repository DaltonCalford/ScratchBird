# ScratchBird Locking Protocol

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created**: October 16, 2025
**Purpose**: Document lock ordering hierarchy to prevent deadlocks
**Related Issue**: CRITICAL-3 - Lock Ordering Inconsistency

---

## Lock Hierarchy (Global Ordering)

To prevent deadlock, locks MUST be acquired in the following order. Violating this order can cause system-wide deadlocks under concurrent load.

### 1. TransactionManager::mutex_
- **Purpose**: Protects transaction state (next_xid, oldest_xid, transaction cache)
- **Type**: std::mutex
- **Location**: `include/scratchbird/core/transaction_manager.h:342`
- **Scope**: All transaction-related operations

### 2. ProcArray::array_lock
- **Purpose**: Protects process control blocks (active transactions, backend state)
- **Type**: pthread_rwlock_t (reader-writer lock)
- **Location**: `include/scratchbird/core/proc_array.h:60`
- **Scope**: Backend registration, transaction tracking, snapshot creation

### 3. ProcArray::alloc_lock
- **Purpose**: Protects slot allocation/deallocation
- **Type**: pthread_mutex_t
- **Location**: `include/scratchbird/core/proc_array.h:61`
- **Scope**: Backend registration/unregistration only

### 4. TransactionManager::group_commit_mutex_
- **Purpose**: Protects group commit queue
- **Type**: std::mutex
- **Location**: `include/scratchbird/core/transaction_manager.h:345`
- **Scope**: Group commit optimization
- **Note**: Independent of mutex_ - can be acquired in any order relative to mutex_

### 5. BufferPool::mutex_
- **Purpose**: Protects buffer pool metadata (frames, page table, LRU list)
- **Type**: std::mutex (mutable)
- **Location**: `include/scratchbird/core/buffer_pool.h:295`
- **Scope**: Page pinning, eviction, flushing

### 6. BufferPool::Frame::content_mutex
- **Purpose**: Protects individual page content from concurrent modifications
- **Type**: std::unique_ptr<std::mutex>
- **Location**: `include/scratchbird/core/buffer_pool.h:190-191`
- **Scope**: Per-page content locking (via lockPage/unlockPage)

---

## Critical Lock Ordering Rules

### Rule 1: TransactionManager::mutex_ → ProcArray::array_lock
**ALWAYS acquire TransactionManager::mutex_ BEFORE ProcArray::array_lock**

✅ **CORRECT Examples:**
```cpp
// updateTransactionMarkers()
std::lock_guard<std::mutex> lock(mutex_);                    // 1. mutex_
pthread_rwlock_rdlock(&proc_array->array_lock);              // 2. array_lock

// getSnapshot()
std::lock_guard<std::mutex> lock(mutex_);                    // 1. mutex_
ProcArrayManager::getActiveTransactions(...);                 // 2. array_lock (internal)
pthread_rwlock_rdlock(&proc_array->array_lock);              // 2. array_lock (second acquisition OK - rdlock reentrant)

// beginTransaction()
std::lock_guard<std::mutex> lock(mutex_);                    // 1. mutex_
ProcArrayManager::setTransactionId(proc_id, new_xid, ctx);   // 2. array_lock (internal wrlock)
```

❌ **INCORRECT (Deadlock Risk):**
```cpp
pthread_rwlock_rdlock(&proc_array->array_lock);              // 1. array_lock
std::lock_guard<std::mutex> lock(mutex_);                    // 2. mutex_ ← DEADLOCK!
```

**Why this matters:**
If Thread A acquires `mutex_` → `array_lock` while Thread B acquires `array_lock` → `mutex_`, they will deadlock.

### Rule 2: ProcArray::alloc_lock → ProcArray::array_lock
**ALWAYS acquire ProcArray::alloc_lock BEFORE ProcArray::array_lock** (ProcArray internal contract)

✅ **CORRECT:**
```cpp
pthread_mutex_lock(&proc_array->alloc_lock);
pthread_rwlock_rdlock(&proc_array->array_lock);
```

### Rule 3: Never Acquire TransactionManager::mutex_ While Holding ProcArray::array_lock
**This is the reverse of Rule 1 and will cause deadlock!**

❌ **NEVER DO THIS:**
```cpp
pthread_rwlock_rdlock(&proc_array->array_lock);
// ... some code ...
transaction_manager->getTransactionState(xid, state, ctx);   // ← This acquires mutex_ internally! DEADLOCK!
```

**Solution:**
If you need both locks, acquire mutex_ first, then array_lock. Or restructure code to avoid holding both.

### Rule 4: group_commit_mutex_ is Independent
**group_commit_mutex_ can be acquired in any order relative to mutex_**

This is safe because group commit operations release mutex_ before acquiring group_commit_mutex_:
```cpp
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... pre-commit work ...
}
// mutex_ released here

{
    std::lock_guard<std::mutex> lock(group_commit_mutex_);
    // ... group commit coordination ...
}
```

---

## Lock Acquisition Patterns

### Pattern 1: Short Critical Section (Preferred)
```cpp
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Quick operation (no I/O, no nested locks)
    stats_.transactions_started++;
}
```

### Pattern 2: Nested Locks (Careful!)
```cpp
{
    std::lock_guard<std::mutex> lock(mutex_);           // Lock 1
    // ... work requiring mutex_ ...

    pthread_rwlock_rdlock(&proc_array->array_lock);     // Lock 2 (OK: correct order)
    // ... work requiring both locks ...
    pthread_rwlock_unlock(&proc_array->array_lock);     // Unlock 2

    // ... more work with mutex_ only ...
}  // Unlock 1
```

### Pattern 3: Release Before I/O (Important!)
```cpp
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... prepare data ...
}
// mutex_ released here

// Perform I/O without holding lock
Status status = writeTipEntry(xid, state, ctx);
status = db_->sync(ctx);
```

**Why:** Never hold locks during I/O operations - this blocks all other threads and degrades performance.

---

## Debugging Deadlocks

### Compile with ThreadSanitizer (TSAN)
```bash
cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" ..
make
./run_tests
```

TSAN will detect lock order violations and report them.

### Use Helgrind
```bash
valgrind --tool=helgrind ./scratchbird_test
```

### Add Debug Assertions
In debug builds, add assertions to verify lock ordering:
```cpp
#ifdef DEBUG
static thread_local bool holding_mutex = false;
static thread_local bool holding_array_lock = false;

// In mutex acquisition:
assert(!holding_array_lock && "Violates lock order: acquiring mutex_ while holding array_lock!");
holding_mutex = true;

// In array_lock acquisition:
assert(!holding_mutex || "OK if mutex_ held - correct order");
holding_array_lock = true;
#endif
```

---

## Common Pitfalls

### Pitfall 1: Calling TransactionManager Methods While Holding array_lock
```cpp
// ❌ WRONG
pthread_rwlock_rdlock(&proc_array->array_lock);
if (transaction_manager->isTransactionVisible(xid, snapshot_xid))  // ← Acquires mutex_! DEADLOCK!
    // ...
pthread_rwlock_unlock(&proc_array->array_lock);

// ✅ CORRECT
// Option A: Release array_lock first
pthread_rwlock_rdlock(&proc_array->array_lock);
uint64_t xid = pcb->xid;
pthread_rwlock_unlock(&proc_array->array_lock);

if (transaction_manager->isTransactionVisible(xid, snapshot_xid))  // Now safe
    // ...

// Option B: Restructure to not need both
```

### Pitfall 2: Forgetting to Release Locks on Error Paths
```cpp
// ❌ WRONG
pthread_rwlock_rdlock(&proc_array->array_lock);
if (error_condition) {
    return Status::ERROR;  // ← Forgot to unlock! Deadlock on next call!
}
pthread_rwlock_unlock(&proc_array->array_lock);

// ✅ CORRECT: Use RAII or ensure all paths unlock
pthread_rwlock_rdlock(&proc_array->array_lock);
if (error_condition) {
    pthread_rwlock_unlock(&proc_array->array_lock);
    return Status::ERROR;
}
pthread_rwlock_unlock(&proc_array->array_lock);
```

### Pitfall 3: Reader-Writer Lock Upgrade Deadlock
```cpp
// ❌ WRONG
pthread_rwlock_rdlock(&proc_array->array_lock);  // Read lock
// ... decide we need to modify ...
pthread_rwlock_wrlock(&proc_array->array_lock);  // ← DEADLOCK! Can't upgrade!
pthread_rwlock_unlock(&proc_array->array_lock);

// ✅ CORRECT: Release read lock, then acquire write lock
pthread_rwlock_rdlock(&proc_array->array_lock);
bool need_write = check_condition();
pthread_rwlock_unlock(&proc_array->array_lock);

if (need_write) {
    pthread_rwlock_wrlock(&proc_array->array_lock);  // OK
    // ... modify ...
    pthread_rwlock_unlock(&proc_array->array_lock);
}
```

---

## Performance Considerations

### Read-Write Locks
- ProcArray::array_lock is a reader-writer lock
- Multiple readers can hold it simultaneously (parallel snapshot creation)
- Only one writer can hold it (transaction start/commit blocks readers)
- Prefer read locks when possible

### Lock Contention Hotspots
1. **TransactionManager::mutex_** - High contention on transaction start/commit
   - Mitigation: Use atomic next_xid_ for XID allocation (already implemented)
   - Mitigation: Group commit to batch fsync operations (already implemented)

2. **ProcArray::array_lock** - Moderate contention on snapshot creation
   - Mitigation: Reader-writer lock allows parallel snapshots
   - Mitigation: Read-only transaction optimization filters active XIDs

3. **BufferPool::mutex_** - High contention on page access
   - Mitigation: Per-page content_mutex for fine-grained locking
   - Mitigation: Clock sweep algorithm reduces lock hold time

---

## Testing Requirements

### Before Merging Lock-Related Changes
1. ✅ Run with TSAN: `make clean && cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" .. && make && ./tests`
2. ✅ Run with Helgrind: `valgrind --tool=helgrind ./tests`
3. ✅ Run multi-threaded stress tests (100+ concurrent threads)
4. ✅ Verify no deadlocks under high load
5. ✅ Verify no lock order violations reported

### Continuous Integration
- Add TSAN to every commit CI pipeline
- Add concurrency stress tests to nightly builds
- Monitor deadlock incidents in production logs

---

## References

- **Issue Tracker**: `/docs/specifications/parser/v3/audit/ALPHA_ISSUES_TRACKER.md` - CRITICAL-3
- **Transaction Manager Header**: `/include/scratchbird/core/transaction_manager.h` (lines 66-97)
- **ProcArray Header**: `/include/scratchbird/core/proc_array.h`
- **BufferPool Header**: `/include/scratchbird/core/buffer_pool.h`

---

**Last Updated**: October 16, 2025
**Maintained By**: ScratchBird Core Team
**Review Frequency**: After any lock-related changes
