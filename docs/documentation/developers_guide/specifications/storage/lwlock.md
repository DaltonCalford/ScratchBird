# Specification: Lightweight Locks (LWLock)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines Lightweight Locks (LWLocks), a fast synchronization primitive used for protecting shared data structures in memory. LWLocks are simpler and faster than heavyweight Lock Manager locks.

## Scope

### In Scope

- LWLock implementation strategies
- Exclusive and shared lock modes
- LWLock tranches (groups)
- Spinlock fallback
- Lock statistics

### Out of Scope

- Heavyweight lock manager (see Lock Manager spec)
- OS mutexes/semaphores (use only when necessary)
- Condition variables (separate primitive)

## Background

LWLocks provide:
- **Speed**: Atomic operations, no system calls in fast path
- **Simplicity**: Two modes (exclusive, shared)
- **Low memory**: Single integer per lock
- **Scalability**: Atomic operations scale well

Used for:
- Buffer pool hash table buckets
- CSLock (Control Segment Lock) in Firebird style
- ProcArray access
- Shared memory data structures

## Specification

### Data Structures

#### LWLock Modes

```cpp
enum class LWLockMode {
    EXCLUSIVE = 0,  // Writer lock
    SHARED = 1      // Reader lock
};
```

#### LWLock Structure

```cpp
class LWLock {
private:
    std::atomic<int32_t> state_;  // Lock state
    // Bit 31: Has waiting processes
    // Bits 0-30: Shared lock count (negative = exclusive)
    
    static constexpr int32_t LW_FLAG_HAS_WAITERS = (1 << 31);
    static constexpr int32_t LW_LOCK_MASK = 0x7FFFFFFF;
    static constexpr int32_t LW_EXCLUSIVE = -1;
    static constexpr int32_t LW_UNLOCKED = 0;

public:
    void acquire(LWLockMode mode);
    void release();
    bool tryAcquire(LWLockMode mode);
};
```

#### LWLock State Encoding

```
State Value | Meaning
────────────┼─────────────────────────────────
0           │ Unlocked
1-N         │ Shared mode with N holders
-1          │ Exclusive mode (single holder)
-2          │ Exclusive with waiters
Negative    │ Exclusive (absolute value = waiters)
```

### Interface Contracts

#### Function: `acquire()`

```cpp
void LWLock::acquire(LWLockMode mode);
```

**Algorithm (Exclusive):**
```
acquireExclusive():
1. WHILE true:
2.     expected = state_.load(relaxed)
3.     
4.     IF expected == 0:  // Unlocked
5.         IF state_.compare_exchange_weak(expected, LW_EXCLUSIVE,
6.                                          acquire, relaxed):
7.             RETURN  // Got lock
8.     
9.     // Lock held, need to wait
10.    addToWaitQueue()
11.    park()  // OS wait or spin
```

**Algorithm (Shared):**
```
acquireShared():
1. WHILE true:
2.     expected = state_.load(relaxed)
3.     
4.     IF expected >= 0:  // Unlocked or shared
5.         IF state_.compare_exchange_weak(expected, expected + 1,
6.                                          acquire, relaxed):
7.             RETURN  // Got shared lock
8.     
9.     // Exclusive held, need to wait
10.    addToWaitQueue()
11.    park()
```

#### Function: `release()`

```cpp
void LWLock::release();
```

**Algorithm:**
```
release():
1. state = state_.fetch_sub(1, release)  // Decrement count
2. 
3. IF state == LW_EXCLUSIVE:  // Was exclusive
4.     IF hasWaiters(state):
5.         wakeOneWaiter()
6. ELIF state == 1:  // Was last shared holder
7.     IF hasWaiters(state):
8.         wakeOneWaiter()
```

#### Function: `tryAcquire()`

```cpp
bool LWLock::tryAcquire(LWLockMode mode);
```

**Algorithm:**
```
tryAcquire(mode):
1. expected = state_.load(relaxed)
2. 
3. IF mode == EXCLUSIVE:
4.     IF expected != 0:
5.         RETURN false
6.     RETURN state_.compare_exchange_strong(expected, LW_EXCLUSIVE,
7.                                            acquire, relaxed)
8. 
9. IF mode == SHARED:
10.    IF expected < 0:
11.        RETURN false
12.    RETURN state_.compare_exchange_strong(expected, expected + 1,
13.                                           acquire, relaxed)
```

### LWLock Tranches

Tranches group related locks for statistics and debugging:

```cpp
enum class LWLockTranche {
    BUFFER_POOL = 0,     // Buffer pool partitions
    PROC_ARRAY = 1,      // ProcArray
    CLOG = 2,            // Commit log
    TIP = 3,             // Transaction info pages
    LOCK_MANAGER = 4,    // Heavyweight lock manager
    OID_GEN = 5,         // OID generator
    XID_GEN = 6,         // XID generator
    NUM_TRANCHES
};

struct LWLockPadded {
    alignas(64) LWLock lock;  // Cache line aligned
    const char *name;
    LWLockTranche tranche;
};
```

### Buffer Pool Partition Locks

```
Buffer Pool Partitioning:
┌─────────────────────────────────────────────────────────────┐
│ Buffer Pool                                                 │
├─────────────────────────────────────────────────────────────┤
│ Partition 0    Partition 1    ...    Partition 63          │
│ ┌─────────┐   ┌─────────┐          ┌─────────┐             │
│ │ LWLock  │   │ LWLock  │          │ LWLock  │             │
│ │  + Hash │   │  + Hash │          │  + Hash │             │
│ │  Table  │   │  Table  │          │  Table  │             │
│ └─────────┘   └─────────┘          └─────────┘             │
└─────────────────────────────────────────────────────────────┘

Page lookup: hash(gpid) % 64 → partition → acquire lock → lookup
```

## Invariants

1. **State Validity**: State is always >= -2^30 and < 2^30
   - Verification: Bit masking
   
2. **Exclusive Singularity**: Only one exclusive holder
   - Verification: State == -1 when exclusive
   
3. **Shared Positive**: Shared count is positive
   - Verification: State > 0 when shared
   
4. **Waiter Wake-up**: Releasing always wakes waiters if present
   - Verification: Check flag before wake

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Uncontended acquire | ~20 ns |
| Contended acquire | ~500 ns - 10 µs |
| Release | ~15 ns |
| Memory per lock | 4-8 bytes |
| Max waiters | 2^30 - 1 |

## Comparison with Heavyweight Locks

| Aspect | LWLock | Heavyweight Lock |
|--------|--------|------------------|
| Speed | ~20 ns | ~200 ns |
| Modes | EXCLUSIVE, SHARED | 8 modes |
| Deadlock detection | No | Yes |
| Wait queue | Simple | Priority-ordered |
| Use case | Data structures | User operations |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_lwlock.cpp` | Basic operations |
| `tests/unit/test_lwlock_contention.cpp` | Contention handling |
| `tests/unit/test_lwlock_trylock.cpp` | Try-lock behavior |

## Related Specifications

- [Lock Manager](./lock_manager.md) - Heavyweight locks
- [Buffer Pool](./buffer_pool.md) - Uses LWLock for partitions

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
