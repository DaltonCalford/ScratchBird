# Specification: Predicate Locks (Serializable Isolation)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha (Planned) |
| **Authors** | Dalton Calford |

## Synopsis

This specification defines Serializable Snapshot Isolation (SSI) using predicate locks. SSI provides true serializable isolation without the performance penalty of two-phase locking.

## Scope

### In Scope

- Serializable Snapshot Isolation theory
- Predicate lock representation
- SSI conflict detection
- Serialization failure handling
- Safe retry rules

### Out of Scope

- Basic snapshot isolation (see Visibility Computation)
- Lock-based serializable isolation
- Optimistic concurrency control

## Background

### Isolation Levels Comparison

| Level | Implementation | Anomalies Allowed |
|-------|---------------|-------------------|
| READ COMMITTED | MGA visibility | Non-repeatable read, phantom |
| SNAPSHOT | Snapshot + MGA | Write skew, phantom |
| SERIALIZABLE | SSI | None |

### SSI Theory

SSI detects potential serializability violations by tracking:
1. **rw-conflicts**: Transaction A reads, Transaction B writes
2. **ww-conflicts**: Both transactions write
3. **Dangerous structure**: Two consecutive rw-conflicts

When a dangerous structure forms, one transaction is aborted.

## Specification

### Data Structures

#### Predicate Lock

```cpp
struct PredicateLock {
    uint32_t proc_id;           // Owner transaction
    LockTag tag;                // What is locked
    
    // For range locks
    std::vector<uint8_t> lower_bound;  // Inclusive
    std::vector<uint8_t> upper_bound;  // Inclusive
    
    // SSI metadata
    uint64_t start_time;
    bool is_read;               // rw-conflict source?
};
```

#### SSI Conflict

```cpp
struct SSIConflict {
    uint32_t proc_a;            // Reader (in rw-conflict)
    uint32_t proc_b;            // Writer
    LockTag tag;                // Where conflict occurred
    ConflictType type;          // rw, ww, or wr
    uint64_t time;
};
```

#### SerializableTransaction

```cpp
struct SerializableTransaction {
    uint32_t proc_id;
    uint64_t xmin;              // Snapshot xmin
    uint64_t xmax;              // Snapshot xmax
    
    // Conflict tracking
    std::vector<SSIConflict> out_conflicts;   // We conflict with others
    std::vector<SSIConflict> in_conflicts;    // Others conflict with us
    
    // Dangerous flag
    bool dangerous;             // Has incoming rw-conflict?
    uint32_t rw_conflict_partner;  // Who gave us rw-conflict?
};
```

### Predicate Lock Types

#### Relation-Level Predicate Lock

```
LockTag:
- target_type: TABLE
- object_uuid: Table UUID
- page_num: 0
- offset_num: 0

Used for: SELECT * FROM table WHERE condition
```

#### Page-Level Predicate Lock

```
LockTag:
- target_type: PAGE
- object_uuid: Table UUID
- page_num: Page number
- offset_num: 0

Used for: Index scan touching specific page
```

#### Tuple-Level Predicate Lock

```
LockTag:
- target_type: TUPLE
- object_uuid: Table UUID
- page_num: Page number
- offset_num: Offset in page

Used for: Exact tuple match
```

#### Range Predicate Lock

```
For: SELECT * FROM table WHERE key BETWEEN 'A' AND 'C'

PredicateLock:
- tag: Index/table
- lower_bound: 'A'
- upper_bound: 'C'

Conflicts with any write in ['A', 'C'] range
```

### SSI Algorithms

#### Conflict Detection

```
Algorithm: checkForConflict(reader, writer, tag)

1. // Check for rw-conflict
2. IF reader has predicate lock on tag AND writer is writing:
3.     // Found rw-conflict
4.     conflict = {reader, writer, tag, RW, now()}
5.     
6.     // Record conflict
7.     reader.out_conflicts.push_back(conflict)
8.     writer.in_conflicts.push_back(conflict)
9.     
10.    // Check for dangerous structure
11.    IF writer.dangerous:
12.        // Both ends have rw-conflicts - serializable failure!
13.        victim = selectVictim(reader, writer)
14.        abortTransaction(victim)
15.        RETURN SERIALIZATION_FAILURE
16.    ELSE:
17.        // Mark writer as dangerous
18.        writer.dangerous = true
19.        writer.rw_conflict_partner = reader.proc_id

20. RETURN OK
```

#### Dangerous Structure Detection

```
A dangerous structure exists when:

T1 ──rw──► T2 ──rw──► T3

Or equivalently (cycles):
T1 ──rw──► T2
▲──────────┘ rw

Where rw means: T1 reads, T2 later writes (overlapping data)

This pattern can lead to non-serializable execution.
```

#### Victim Selection

```
Algorithm: selectVictim(t1, t2)

1. // Prefer to abort:
2. // 1. Read-only transactions (can retry safely)
3. // 2. Younger transactions (less work lost)
4. // 3. Transactions with fewer conflicts

5. IF t1.is_read_only AND NOT t2.is_read_only:
6.     RETURN t1
7. IF t2.is_read_only AND NOT t1.is_read_only:
8.     RETURN t2

9. IF t1.start_time > t2.start_time:
10.    RETURN t1  // t1 is younger
11. ELSE:
12.    RETURN t2
```

### Interface Contracts

#### Function: `acquirePredicateLock()`

```cpp
Status acquirePredicateLock(
    uint32_t proc_id,
    const LockTag &tag,
    const std::vector<uint8_t> &lower_bound,
    const std::vector<uint8_t> &upper_bound,
    bool for_read,          // true = rw-conflict source
    ErrorContext *ctx
);
```

**Postconditions:**
- Predicate lock acquired
- Any conflicts detected and recorded
- Serialization failure may be raised

#### Function: `checkWriteConflict()`

```cpp
Status checkWriteConflict(
    uint32_t writer_proc_id,
    const LockTag &tag,
    ErrorContext *ctx
);
```

**Called before**: Any INSERT, UPDATE, DELETE

**Postconditions:**
- rw-conflicts checked
- Dangerous structures detected
- May return SERIALIZATION_FAILURE

### Serialization Failure

```
When SSI detects dangerous structure:

1. Select victim transaction
2. Abort victim with SQLSTATE 40001
3. Message: "could not serialize access due to read/write dependencies"

Application must:
1. Roll back current transaction
2. Retry from beginning
```

## Invariants

1. **Conflict Symmetry**: If A conflicts with B, B conflicts with A
   - Verification: Record in both directions
   
2. **Dangerous Structure Detection**: Any dangerous pattern causes abort
   - Verification: Check on every rw-conflict
   
3. **Read-Only Safety**: Read-only transactions never need abort
   - Verification: They don't create ww-conflicts

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `SERIALIZATION_FAILURE` | Dangerous structure detected | Retry transaction |

## Performance Considerations

### Predicate Lock Granularity
- **Fine (tuple)**: Precise, but many locks
- **Coarse (page/table)**: Fewer locks, more false positives
- **Trade-off**: Accuracy vs overhead

### Conflict Tracking Overhead
- **Per transaction**: O(number of conflicts)
- **Memory**: ~100 bytes per conflict
- **Cleanup**: On transaction end

### SSI vs 2PL Performance
- **Read-heavy**: SSI much faster (no blocking)
- **Write-heavy**: Comparable
- **Contention**: SSI may have more aborts

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_ssi_basic.cpp` | Basic SSI |
| `tests/unit/test_ssi_write_skew.cpp` | Write skew detection |
| `tests/unit/test_ssi_phantom.cpp` | Phantom detection |
| `tests/unit/test_ssi_retry.cpp` | Retry rules |

## Related Specifications

- [Lock Manager](./lock_manager.md) - Heavyweight locks
- [Visibility Computation](./visibility_computation.md) - Base visibility

## References

- "Serializable Snapshot Isolation" - Cahill et al., SIGMOD 2009
- PostgreSQL SSI implementation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
