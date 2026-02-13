# ScratchBird Transaction Management - Lock Manager (Firebird Model)


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](FIREBIRD_CONSTANTS_REFERENCE.md)

## Part 2 of Transaction and Lock Management Specification

## Purpose
This specification defines the lock manager model ScratchBird must follow.
It is a Firebird-derived lock manager, not PostgreSQL. It exists to coordinate
engine-level resources, not to provide MVCC read locks.

Authoritative Firebird sources:
- Lock manager core: `firebird/src/lock/lock.cpp`, `firebird/src/lock/lock_proto.h`
- Engine lock usage: `firebird/src/jrd/lck.cpp`, `firebird/src/jrd/lck.h`

---

## 1. Firebird Lock Manager Architecture

### 1.1 Shared Lock Table
- The lock manager is implemented as a shared memory lock table.
- The lock table is stored in a shared file and mapped into each process.
- Core structures:
  - `lhb` (lock header block)
  - `lbl` (lock block)
  - `lrq` (lock request)
  - `own` (owner blocks)
  - hash tables for locks and requests

This is defined in `firebird/src/lock/lock_proto.h` and implemented in
`firebird/src/lock/lock.cpp`.

### 1.2 Owners and Requests
- Each attachment (and the database itself) is an owner in the lock manager.
- Locks are acquired by submitting lock requests (`lrq`) against lock blocks (`lbl`).
- Owners and requests are linked in queues for efficient management and deadlock detection.

### 1.3 Lock Levels
Firebird defines these lock levels:
- `LCK_null`
- `LCK_SR` (shared read)
- `LCK_PR` (protected read)
- `LCK_SW` (shared write)
- `LCK_PW` (protected write)
- `LCK_EX` (exclusive)

Compatibility is defined in the Firebird lock manager (see `lock.cpp`).
ScratchBird must use Firebird’s compatibility semantics.

### 1.4 Lock Types
Firebird lock types are defined in `firebird/src/jrd/lck.h` and include:
- database lock (`LCK_database`)
- relation lock (`LCK_relation`)
- buffer/page lock (`LCK_bdb`)
- transaction lock (`LCK_tra`)
- existence locks for relations, indexes, procedures, functions
- sweep lock (`LCK_sweep`)
- attachment lock (`LCK_attachment`)
- record-level GC lock (`LCK_record_gc`)
- other internal coordination locks

ScratchBird should adopt these types (or equivalents) and maintain the same intent.

---

## 2. Lock Requests, Conversion, and Blocking AST

### 2.1 Requests and Conversion
- A request is submitted at a desired level.
- If incompatible, it is queued as pending.
- Conversions (e.g., SR -> EX) are explicitly supported.
- Lock compatibility and grant decisions are performed by the lock manager.

### 2.2 Blocking AST (Asynchronous Trap)
- When a request blocks others, the lock manager triggers a blocking AST.
- The AST signals the owner to downgrade or release the lock.
- This is a core part of Firebird deadlock avoidance.

Blocking AST is implemented in `firebird/src/lock/lock.cpp` and used by the engine in
`firebird/src/jrd/lck.cpp`.

---

## 3. Deadlock Detection

- Firebird lock manager performs deadlock detection by scanning request graphs.
- Deadlock scan interval is configurable.
- Deadlock resolution is handled centrally by the lock manager.

See `deadlock_scan` and related functions in `firebird/src/lock/lock.cpp`.

---

## 4. Transaction Locks and GC Integration

Firebird uses transaction locks (`LCK_tra`) as a communication channel for GC:
- Each transaction lock stores a data value representing its oldest active snapshot.
- The lock manager can return the minimum `LCK_tra` data across all active transactions.
- This value becomes the effective GC horizon (oldest active snapshot).

This is visible in `firebird/src/jrd/tra.cpp` and `firebird/src/jrd/lck.cpp`.

ScratchBird must implement the same coupling between transaction locks and GC.

---

## 5. What the Lock Manager Is Not

- It is not a PostgreSQL-style predicate lock system.
- It is not a read locking system for MVCC visibility.
- Readers generally do not acquire locks for MVCC reads; they use version chains.

ScratchBird must not introduce PG-style lock modes (ACCESS SHARE, ROW EXCLUSIVE, etc).

---

## 6. Required ScratchBird Behavior

ScratchBird must implement Firebird-equivalent semantics:
- Shared lock table with owner/request queues.
- Firebird lock levels and compatibility.
- Blocking AST callbacks for lock conflicts.
- Deadlock scan and resolution consistent with Firebird.
- Transaction lock data used for GC horizon (OAT/OST control).
- Lock types aligned with Firebird’s internal lock taxonomy.

---

## 7. Implementation Pointers (FirebirdSQL)

- Lock manager core: `firebird/src/lock/lock.cpp`
- Shared lock table definitions: `firebird/src/lock/lock_proto.h`
- Engine lock usage: `firebird/src/jrd/lck.cpp`, `firebird/src/jrd/lck.h`

---

## 8. Firebird Lock Manager Structures (Detailed)

The shared lock table is a structured shared-memory layout:
- **LHB (lock header block)**: global metadata, hash buckets, queues of owners,
  processes, free lists, counters, scan settings.
- **LBL (lock block)**: represents a lockable object. Holds lock key, state,
  granted counts by level, queues of requests, and lock data.
- **LRQ (lock request)**: a request record linked into owner and lock queues.
  Contains requested level, state, blocking AST callback, and owner reference.
- **OWN (owner block)**: represents a database or attachment owner in the lock
  manager. Tracks owner queues and pending requests.

See `firebird/src/lock/lock_proto.h` for field-level structure definitions.

---

## 9. Lock Manager Core Operations (Detailed)

### 9.1 Enqueue
`enqueue()` allocates or finds an LBL for the lock key, creates an LRQ, and then:
1. Checks compatibility against granted requests.
2. If compatible, grants immediately and links LRQ to granted list.
3. If incompatible, enqueues LRQ to the pending list and may wait.
4. If wait time is exceeded, cancels and returns timeout/denied.

### 9.2 Convert
`convert()` upgrades/downgrades an existing request:
- Performs compatibility checks against other granted requests.
- If conflicting, queues the conversion request.
- On success, updates physical and logical lock levels and wakes waiters.

### 9.3 Dequeue
`dequeue()` removes a request and updates the LBL state and compatibility queue.
If removing a blocking request allows others to proceed, waiters are reposted.

### 9.4 Blocking AST
Firebird uses blocking AST callbacks when a granted request blocks another.
The lock manager invokes the AST to prompt the owner to downgrade or release.
This is a core part of Firebird’s non-spinning conflict resolution.

### 9.5 Repost / Wakeup
When a blocking request is removed or downgraded, the lock manager attempts to
grant compatible queued requests, then reposts any waiters to re-evaluate.

Implementation: `firebird/src/lock/lock.cpp` (enqueue/convert/dequeue/repost).

---

## 10. Deadlock Detection (Detailed)

Firebird performs deadlock scans by walking the wait-for graph:
- Each LRQ in waiting state contributes an edge from waiting owner to blocking owner.
- The lock manager walks these edges for cycles.
- If a cycle is found, one request is selected for deadlock resolution and
  returned with `isc_deadlock`.

See `deadlock_scan` / `deadlock_walk` in `lock.cpp`.

---

## 11. Compatibility and Grant Algorithm (Explicit)

Firebird compatibility is defined by lock level and current granted state.
The effective algorithm is:
1. For a given LBL, determine its highest granted level and counts by level.
2. For each pending/granted request, evaluate whether the requested level is
   compatible with the aggregate of granted levels.
3. If compatible, grant and update LBL state and counts.
4. If incompatible, enqueue as pending and mark as blocking if applicable.

ScratchBird must mirror Firebird’s compatibility table and grant semantics.
Compatibility is level-based (LCK_SR, LCK_PR, LCK_SW, LCK_PW, LCK_EX) rather than
PostgreSQL-style mode matrices.

---

## 12. Lock Data Operations (Explicit)

Firebird locks can carry 64-bit data values (e.g., for transaction locks).
The lock manager supports:
- `readData` / `readData2`: read lock data values for a lock series.
- `writeData`: update the data field of a granted request.
- `queryData`: compute min/max/sum/count/avg over data for a lock series.

ScratchBird must implement these data operations, as they are used by GC
and transaction coordination (e.g., `LCK_tra` min for GC horizon).

---

## 13. Deadlock Victim Selection (Firebird Behavior)

Firebird’s deadlock detection is request-driven:
1. A waiting request triggers `deadlock_scan(owner, request)`.
2. `deadlock_walk` performs a DFS over the wait-for graph starting from the
   current request.
3. If the walk encounters a request already marked `LRQ_deadlock`, it has
   found a cycle and returns that request as the victim.
4. If a blocking owner is still processing ASTs or has just been granted a
   lock, the walk marks `maybe_deadlock` and defers resolution.
5. Requests waiting with a timeout are not pursued in the deadlock walk (their
   timeout will break the cycle).

Victim action:
- The returned victim request is marked `LRQ_rejected` and removed from its
  owner pending queue, then the owner is awakened.
- It is possible for the caller’s own request to be chosen and rejected.

This behavior is implemented in `lock.cpp` (`deadlock_scan`, `deadlock_walk`,
and the wait loop in `wait_for_request`).

---

## 14. ScratchBird Implementation Requirements (Expanded)

ScratchBird must implement the following Firebird-equivalent behaviors:
1. Shared lock table with LHB/LBL/LRQ/OWN data structures.
2. Lock request queues and compatibility checks based on Firebird lock levels.
3. Blocking AST callbacks invoked by the lock manager.
4. Deadlock scans across request graphs, with deterministic resolution.
5. Transaction lock data updates and GC horizon queries as in Firebird.
6. Engine-level lock types aligned with Firebird’s `lck.h` taxonomy.

This is non-negotiable; PostgreSQL-style lock models are not compatible.

---

## Appendix A. Firebird Lock Compatibility Table

This appendix preserves Firebird’s compatibility matrix logic in explicit tabular form.
Levels are ordered as: `none`, `null`, `SR`, `PR`, `SW`, `PW`, `EX`.
`true` means compatible; `false` means conflict.

```
               none  null   SR    PR    SW    PW    EX
none           true  true  true  true  true  true  true
null           true  true  true  true  true  true  true
SR             true  true  true  true  true  true  false
PR             true  true  true  true  false false false
SW             true  true  true  false true  false false
PW             true  true  true  false false false false
EX             true  true  false false false false false
```

This corresponds to `compatibility[LCK_max][LCK_max]` in `firebird/src/lock/lock.cpp`.
ScratchBird must implement identical compatibility semantics.

**Terminology note:** ScratchBird uses Firebird MGA. Any MGA references in this file are legacy shorthand and must be interpreted as MGA per the authoritative references above.

## ScratchBird Extensions Impact (Lock Manager)

### 1. UUID-Based Lock Keys

ScratchBird replaces page/slot identifiers with UUIDs for logical lock keys:
- Relation, index, and record-level GC locks use UUIDs as the primary key.
- Lock manager compatibility and deadlock behavior remain Firebird-compatible.
- Physical page/buffer locks still exist but are scoped within a file space and are
  not used for logical identity.

### 2. Large Page Sizes

Larger pages increase contention scope:
- Page/buffer lock hold times must be minimized.
- GC and sweep should use chunked iteration to avoid long lock hold times on
  large pages.

### 3. File Spaces

Lock namespaces must be stable across file spaces:
- Logical locks should be keyed by object UUID (not file-space-local IDs).
- Physical locks may include file-space identifiers in the lock key only when
  protecting the physical page itself.

### 4. Distributed Shard Repositories

- Logical locks for shared/global objects must be shard-aware (namespace prefix
  or control-plane arbitration).
- Physical locks remain local to the shard.
- Deadlock detection remains local per shard unless global locking is introduced.
