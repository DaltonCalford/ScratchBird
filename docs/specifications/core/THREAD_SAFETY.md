# Thread Safety Specification

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).

## Thread Safety Levels

### Level 1: Immutable
No synchronization needed after initialization.

### Level 2: Thread-Local
No sharing between threads.

### Level 3: Read-Write Lock
Multiple readers, single writer.

### Level 4: Mutex Protected
Exclusive access required.

### Level 5: Lock-Free
Atomic operations only.

## Component-Level Rules (Alpha)

The following components MUST adhere to the specified level and contracts.

1) Page I/O (readPage/writePage)
- Level: 4 (Mutex)
- Contract:
  - Single writer at a time for a given file descriptor
  - Serialize writes per-page_id to avoid torn updates
  - Reads may proceed concurrently with other reads

2) Buffer Pool (single-threaded in Alpha; multi-threaded in Beta)
- Level: 3 (RWLock) — enabled when concurrency flag is on; default single-threaded
- Contract:
  - pinPage/unpinPage require at least read lock
  - page replacement and flush require write lock

3) Transaction ID allocation
- Level: 4 (Mutex)
- Contract:
  - getNextTransactionId() is globally serialized
  - XID counter is 64-bit and monotonically increasing; wrap-around not permitted

4) Transaction Inventory Page (TIP) access
- Level: 3 (RWLock) for in-memory TIP cache; Level 4 for I/O updates
- Contract:
  - Reads acquire shared lock; state transitions (Active→Committed/Aborted) require exclusive lock

5) Catalog cache (in-memory)
- Level: 3 (RWLock)
- Contract:
  - Name/UUID lookups acquire read; DDL operations acquire write

6) Statistics and counters
- Level: 5 (Lock-Free atomics)
- Contract:
  - All counters use std::atomic with relaxed or acquire/release semantics as appropriate

7) Error context (thread-local)
- Level: 2 (Thread-Local)
- Contract:
  - Error stacks are not shared across threads; propagate by value or reconstruct

## Lock Ordering (to prevent deadlocks)

Global lock acquisition order (lowest to highest):
1. Statistics counters (Level 5) — atomic, non-blocking
2. Read locks on caches (catalog, TIP cache) — Level 3 (shared)
3. Buffer pool page locks — per-page lock (if enabled)
4. TIP write lock — Level 3 (exclusive) or Level 4 for I/O commit sequence
5. File I/O mutex — Level 4 (per-database file)

RULES:
- Never acquire a lower-order lock after holding a higher-order lock
- Convert read→write by releasing read and reacquiring write
- Do not hold locks across blocking I/O when not strictly necessary

## Critical Sections (Alpha)

Commit sequence (no write-after log (WAL) in Alpha; optional post-gold only):
1) Acquire TIP exclusive lock
2) Update TIP state to Committed
3) Release TIP lock

Flush sequence (optional in Alpha):
1) Acquire file I/O mutex
2) Write modified pages
3) Release file I/O mutex

## Performance Considerations

- Favor read locks for hot paths (catalog lookups, page pins)
- Keep critical sections short and avoid nested exclusive locks
- Use atomics for frequently updated counters; avoid false sharing

## Documentation Requirement

Every structure MUST declare its thread safety level and locking requirements:

```c
typedef struct Database {
    // THREAD SAFETY: Level 3 (RWLock) for metadata; Level 4 for I/O
    pthread_rwlock_t    meta_lock;   // schema/catalog views
    pthread_mutex_t     io_mutex;    // file I/O serialization
} Database;
```
