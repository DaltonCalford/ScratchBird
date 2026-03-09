# Specification: Lock Manager

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/lock_manager.h:30`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/lock_manager.cpp`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_lock_manager.cpp`

## Synopsis

This specification defines the Lock Manager which provides heavyweight locking for concurrency control at database, table, page, and tuple levels. It implements deadlock detection and supports multiple lock modes.

## Scope

### In Scope

- Lock modes and compatibility matrix
- Lock acquisition and release
- Deadlock detection
- Lock timeout handling
- Lock statistics

### Out of Scope

- Lightweight locks (see LWLock spec)
- Predicate locks (see Predicate Locks spec)
- Advisory locks (user-level)

## Background

The Lock Manager implements standard heavyweight locking similar to PostgreSQL:
- **Lock levels**: Database, table, page, tuple
- **Lock modes**: 8 modes from ACCESS SHARE to ACCESS EXCLUSIVE
- **Conflict matrix**: Defines which modes conflict
- **Deadlock detection**: Builds wait-for graph, detects cycles

## Specification

### Data Structures

#### LockMode Enum

```cpp
// From include/scratchbird/core/lock_manager.h:30
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
```

#### LockTag (Lock Identity)

```cpp
// From include/scratchbird/core/lock_manager.h:52
struct LockTag {
    LockTarget target_type;     // DATABASE, TABLE, PAGE, TUPLE
    UuidV7Bytes object_uuid;    // Table/Index UUID
    uint64_t page_num;          // For page locks
    uint16_t offset_num;        // For tuple locks
    uint16_t padding;
    
    bool operator==(const LockTag &other) const;
    struct Hash { size_t operator()(const LockTag &tag) const; };
};
```

#### Lock Object

```cpp
// From include/scratchbird/core/lock_manager.h:117
struct Lock {
    LockTag tag;                    // What is locked
    uint32_t granted_mask;          // Bitmask of granted modes
    uint32_t granted_counts[8];     // Count per mode
    std::list<std::unique_ptr<LockRequest>> wait_queue;
    uint64_t total_acquisitions;
    uint64_t total_waits;
};
```

#### LockRequest

```cpp
// From include/scratchbird/core/lock_manager.h:91
struct LockRequest {
    uint32_t proc_id;       // Backend requesting lock
    LockMode mode;          // Requested mode
    bool granted;           // Is lock granted?
    uint64_t request_time;  // When requested
};
```

### Conflict Matrix

```
Conflicts: X = conflict, space = compatible

Requested →  ACCESS  ROW   ROW   SHARE UPDATE SHARE  SHARE  EXCL   ACCESS
Held ↓       SHARE   SHARE EXCL  EXCL  SHARE  ROW EXCL EXCL   EXCL
────────────────────────────────────────────────────────────────────────────
ACCESS SHARE              X
ROW SHARE                 X     X
ROW EXCL            X     X     X     X
SHARE UPDATE EXCL   X     X     X     X     X     X
SHARE               X     X     X     X           X     X
SHARE ROW EXCL      X     X     X     X     X     X     X
EXCL                X     X     X     X     X     X     X     X
ACCESS EXCL   X     X     X     X     X     X     X     X     X
```

### Interface Contracts

#### Function: `acquireLock()`

```cpp
// Source: src/core/lock_manager.cpp
Status LockManager::acquireLock(
    uint32_t proc_id,
    const LockTag &tag,
    LockMode mode,
    bool wait,              // Block if conflict?
    uint32_t timeout_ms,    // 0 = infinite
    ErrorContext *ctx
);
```

**Preconditions:**
- Backend registered in ProcArray
- Lock mode valid

**Postconditions:**
- Lock granted (return OK) or
- Timeout (return LOCK_TIMEOUT) or
- Deadlock detected (return DEADLOCK)

**Algorithm:**
```
1. lock = findOrCreateLock(tag)

2. // Check for conflict
3. IF NOT checkConflict(lock, mode, proc_id):
4.     // No conflict - grant immediately
5.     lock->granted_mask |= (1 << (mode - 1))
6.     lock->granted_counts[mode - 1]++
7.     proc_locks_[proc_id].insert({lock, mode})
8.     stats_.locks_acquired++
9.     RETURN OK

10. IF NOT wait:
11.    RETURN LOCK_CONFLICT

12. // Must wait
13. request = new LockRequest{proc_id, mode, false, now()}
14. lock->wait_queue.push_back(request)
15. stats_.lock_waits++

16. // Wait loop
17. deadline = (timeout_ms > 0) ? now() + timeout_ms : INF
18. WHILE NOT request->granted:
19.    remaining = deadline - now()
20.    IF remaining <= 0:
21.        lock->wait_queue.remove(request)
22.        RETURN LOCK_TIMEOUT
23.    
24.    // Check for deadlock periodically
25.    IF now() - last_deadlock_check > deadlock_timeout_ms_:
26.        detectDeadlocks()
27.    
28.    WAIT lock_wait_cv_ FOR remaining
29.    IF deadlock_detected_for(proc_id):
30.        RETURN DEADLOCK

31. RETURN OK
```

#### Function: `releaseLock()`

```cpp
// Source: src/core/lock_manager.cpp
Status LockManager::releaseLock(
    uint32_t proc_id,
    const LockTag &tag,
    LockMode mode,
    ErrorContext *ctx
);
```

**Algorithm:**
```
1. lock = findLock(tag)
2. IF lock == nullptr: RETURN NOT_FOUND

3. // Update granted counts
4. lock->granted_counts[mode - 1]--
5. IF lock->granted_counts[mode - 1] == 0:
6.     lock->granted_mask &= ~(1 << (mode - 1))

7. // Remove from proc_locks_
8. proc_locks_[proc_id].remove({lock, mode})

9. // Try to grant waiting locks
10. grantWaitingLocks(lock)

11. // Cleanup if unused
12. IF lock->granted_mask == 0 AND lock->wait_queue.empty():
13.    removeLock(tag)

14. RETURN OK
```

#### Function: `detectDeadlocks()`

```cpp
// Source: src/core/lock_manager.cpp
Status LockManager::detectDeadlocks(ErrorContext *ctx);
```

**Algorithm:**
```
1. // Build wait-for graph
2. graph = empty_map<proc_id, vector<proc_id>>
3. 
4. FOR each lock IN lock_table_:
5.     holders = getHolders(lock)  // Granted procs
6.     waiters = getWaiters(lock)  // Waiting procs
7.     
8.     FOR each waiter IN waiters:
9.         graph[waiter] = holders  // Waiter waits for all holders

10. // Find cycles using DFS
11. FOR each proc IN graph:
12.     IF hasCycle(proc, visited={}, rec_stack={}):
13.         cycle = findCycle(proc)
14.         victim = selectVictim(cycle)
15.         abortTransaction(victim)
16.         stats_.deadlocks_detected++

17. RETURN OK
```

### Deadlock Detection

```
Algorithm: hasCycle(node, visited, rec_stack)

1. visited.insert(node)
2. rec_stack.insert(node)

3. FOR each neighbor IN graph[node]:
4.     IF neighbor NOT IN visited:
5.         IF hasCycle(neighbor, visited, rec_stack):
6.             RETURN true
7.     ELSE IF neighbor IN rec_stack:
8.         RETURN true  // Found cycle

9. rec_stack.remove(node)
10. RETURN false
```

```
Algorithm: selectVictim(cycle)

1. // Select youngest transaction in cycle
2. youngest = cycle[0]
3. FOR each proc_id IN cycle:
4.     IF getTransactionStartTime(proc_id) > 
5.        getTransactionStartTime(youngest):
6.         youngest = proc_id

7. RETURN youngest  // Youngest transaction aborted
```

## Invariants

1. **Granted Counts**: Sum of granted_counts equals total granted locks
   - Verification: Updated atomically with mask
   
2. **Wait Queue Order**: FIFO ordering within same priority
   - Verification: push_back, grant from front
   
3. **No Self-Deadlock**: Transaction cannot be in its own wait-for list
   - Verification: Skip self in conflict check
   
4. **Lock Release**: All locks released on transaction end
   - Verification: releaseAllLocks() called

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `LOCK_CONFLICT` | Lock unavailable (nowait) | Retry or fail |
| `LOCK_TIMEOUT` | Timeout waiting | Retry or fail |
| `DEADLOCK` | Deadlock detected | Transaction aborted |

## Performance Considerations

### Lock Table Partitioning
- **Future optimization**: Partition by hash(tag)
- **Benefit**: Reduce contention
- **Current**: Single mutex (acceptable for Alpha)

### Deadlock Check Frequency
- **Default**: Every 1 second while waiting
- **Configurable**: deadlock_timeout_ms
- **Cost**: O(V + E) graph traversal

### Fast Path
- **No conflict**: Grant without queue manipulation
- **Single holder**: O(1) conflict check
- **Statistics**: Lock-free counters

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_lock_manager.cpp` | Basic lock/unlock |
| `tests/unit/test_lock_conflicts.cpp` | Conflict matrix |
| `tests/unit/test_deadlock_detection.cpp` | Deadlock handling |
| `tests/unit/test_lock_timeouts.cpp` | Timeout behavior |

## Related Specifications

- [LWLock](./lwlock.md) - Lightweight locks
- [Predicate Locks](./predicate_locks.md) - Serializable isolation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
