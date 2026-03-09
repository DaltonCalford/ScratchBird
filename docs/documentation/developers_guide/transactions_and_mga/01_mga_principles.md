<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# MGA Principles

[Prev](../README.md) | [Next](./02_mga_vs_wal.md) | [Topic README](./README.md) | [Developers Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

Multi-Generational Architecture (MGA) is the storage foundation of ScratchBird. Unlike traditional databases that update rows in-place and use WAL for recovery, MGA creates new versions for every change and uses the database itself as the transaction log.

## Core Principle: Everything is an INSERT

```
Traditional Database (In-Place + WAL):
┌─────────────┐     UPDATE users SET name = 'Jane' WHERE id = 1;
│ Row: John   │ ──► ┌─────────────┐     ┌───────────────────────┐
│ ID: 1       │     │ Row: Jane   │     │ WAL: UPDATE id=1      │
└─────────────┘     │ ID: 1       │     │     name='Jane'       │
                    └─────────────┘     └───────────────────────┘
                    Overwrite in-place    Write ahead log

ScratchBird MGA (Version Chain):
┌─────────────┐     UPDATE users SET name = 'Jane' WHERE id = 1;
│ Version 1   │ ──► ┌─────────────┐     ┌───────────────────────┐
│ TXN: 100    │     │ Version 2   │ ──► │ Version 1             │
│ Row: John   │     │ TXN: 200    │     │ (unchanged, linked)   │
│ ID: 1       │     │ Row: Jane   │     │                       │
└─────────────┘     │ back-ptr ───┘     └───────────────────────┘
                    └─────────────┘
                    New version inserted    Old version preserved
                    No WAL needed           Database IS the log
```

## Version Chain Structure

```
Record Version Chain:
                         
Latest ◄── Newer ◄── Older ◄── Oldest
  │          │          │          │
┌────┐    ┌────┐    ┌────┐    ┌────┐
│V4  │    │V3  │    │V2  │    │V1  │
│TXN │    │TXN │    │TXN │    │TXN │
│500 │    │450 │    │420 │    │400 │
│State│   │State│   │State│   │State│
│ACTIVE│  │COMMIT│  │COMMIT│  │COMMIT│
└────┘    └────┘    └────┘    └────┘

Each version:
- Transaction ID that created it
- State: ACTIVE, COMMITTED, or ROLLED_BACK
- Back-pointer to previous version
- Full row data (not deltas)
```

## Transaction Operations

### INSERT

```
INSERT INTO users (id, name) VALUES (1, 'John');

Before:                    After:
(No row)                  ┌─────────────┐
                          │ Version 1   │
                          │ TXN: 100    │
                          │ State: ACTIVE│
                          │ ID: 1       │
                          │ Name: John  │
                          │ back-ptr: NULL
                          └─────────────┘
```

### UPDATE

```
UPDATE users SET name = 'Jane' WHERE id = 1;
                          
Before:                   After (during TXN 200):
┌─────────────┐           ┌─────────────┐    ┌─────────────┐
│ Version 1   │           │ Version 2   │───►│ Version 1   │
│ TXN: 100    │           │ TXN: 200    │    │ TXN: 100    │
│ State: COMMIT│          │ State: ACTIVE│   │ State: COMMIT│
│ ID: 1       │           │ ID: 1       │    │ ID: 1       │
│ Name: John  │           │ Name: Jane  │    │ Name: John  │
└─────────────┘           └─────────────┘    └─────────────┘

After COMMIT:             After ROLLBACK:
Version 2: COMMITTED      Version 2: ROLLED_BACK
```

### DELETE

```
DELETE FROM users WHERE id = 1;

Before:                   After (during TXN 300):
┌─────────────┐           ┌─────────────┐    ┌─────────────┐
│ Version 2   │           │ Version 3   │───►│ Version 2   │
│ TXN: 200    │           │ TXN: 300    │    │ TXN: 200    │
│ State: COMMIT│          │ State: ACTIVE│   │ State: COMMIT│
│ ID: 1       │           │ ID: 1       │    │ ID: 1       │
│ Name: Jane  │           │ deleted flag│    │ Name: Jane  │
└─────────────┘           └─────────────┘    └─────────────┘

After COMMIT:
Version 3: COMMITTED (deleted flag set)
```

## Visibility Rules

A transaction can see a version if:

```
1. Version's transaction is COMMITTED
   AND
2. Version's transaction ID < current transaction's snapshot ID
   OR
3. Version's transaction is the current transaction itself
```

### Example Scenario

```
Timeline:
T0: Transaction 100 begins
T1: Transaction 200 begins
T2: TXN 200 updates row → creates Version 2 (ACTIVE)
T3: Transaction 300 begins (snapshot = {100, 200})
T4: TXN 200 commits → Version 2: COMMITTED
T5: Transaction 400 begins (snapshot = {100, 200, 300})

Visibility from TXN 300:
- Version 1 (TXN 100): VISIBLE (100 < 300, committed)
- Version 2 (TXN 200): NOT VISIBLE (200 >= 300 in snapshot)

Visibility from TXN 400:
- Version 1 (TXN 100): VISIBLE
- Version 2 (TXN 200): VISIBLE (200 < 400, committed)
```

## Snapshots

A snapshot captures the set of active transactions at a point in time:

```
Snapshot Structure:
{
    snapshot_id: 500,
    xmin: 100,           // Oldest active transaction
    xmax: 501,           // Next transaction ID
    active_txns: [450, 480, 495]  // Currently active
}

Version visibility check:
IF version.txn_id == current_txn_id:
    VISIBLE  // My own changes
ELSE IF version.txn_id IN snapshot.active_txns:
    NOT VISIBLE  // Active (not committed)
ELSE IF version.state != COMMITTED:
    NOT VISIBLE  // Rolled back
ELSE IF version.txn_id >= snapshot.xmax:
    NOT VISIBLE  // Future transaction
ELSE:
    VISIBLE  // Committed before snapshot
```

## No Traditional WAL

### Why No WAL?

Traditional databases need WAL because:
- In-place updates lose old data immediately
- Need to recover to consistent state after crash

MGA doesn't need WAL because:
- Old versions are never overwritten
- Transaction state is in the data pages
- COMMIT is just a flag change

```
Traditional Commit:
1. Write WAL records (force to disk)
2. Update data pages (in memory)
3. Write commit WAL record (force to disk)
4. Return success to client
5. Background: flush data pages

MGA Commit:
1. Update version state: ACTIVE → COMMITTED (single flag)
2. Return success to client
3. Background: eventual flush to disk

The database IS the log.
```

## Index Behavior

### Indexes Point to ALL Versions

```
Index on users(email):

Traditional:              MGA:
┌──────────┬─────────┐    ┌──────────┬──────────────────────┐
│ email    │ row_id  │    │ email    │ version_ptr          │
├──────────┼─────────┤    ├──────────┼──────────────────────┤
│ john@... │ 1001    │    │ john@... │ ptr → Version 1      │
│ jane@... │ 1002    │    │ jane@... │ ptr → Version 2      │
└──────────┴─────────┘    │ jane@... │ ptr → Version 3      │
                          └──────────┴──────────────────────┘
                          Multiple entries for same key!
```

### Index Scan with Visibility Check

```sql
SELECT * FROM users WHERE email = 'jane@example.com';

Index scan:
1. Find index entry for 'jane@example.com'
2. Get version pointer
3. Check visibility against snapshot
4. If visible, return row
5. If not visible, follow back-pointer chain
6. Find first visible version

Result: Reader sees consistent snapshot without locking
```

## Garbage Collection

Old versions must eventually be reclaimed.

### Cooperative GC

```
Reader performs GC:
1. While traversing version chain
2. Find versions older than all active snapshots
3. Remove if state = COMMITTED or ROLLED_BACK
4. Update back-pointers to skip removed versions
```

### Background GC

```
Dedicated GC process:
1. Identify transactions older than oldest snapshot
2. Scan version chains
3. Remove unreachable versions
4. Compact index entries
```

### GC Thresholds

| Parameter | Default | Description |
|-----------|---------|-------------|
| `mga.gc_threshold_txns` | 100 | Min transactions before GC |
| `mga.gc_threshold_versions` | 1000 | Min versions before GC |
| `mga.gc_interval_ms` | 60000 | Background GC interval |

## Concurrency Benefits

### Readers Never Block Writers

```
Writer updating row:
- Creates new version
- Doesn't lock old version
- Reader continues using old version

Reader querying row:
- Uses snapshot
- Doesn't lock anything
- Writer proceeds unblocked
```

### Writers Never Block Readers

```
Writer committing:
- Just flips flag
- No lock on table
- Readers use their snapshot
```

### Only Writers Block Writers (on Same Row)

```
Two writers updating same row:
- Second writer waits for first to commit/rollback
- OR: Second writer gets conflict error
- Configurable: optimistic vs pessimistic
```

## Comparison: MGA vs MVCC vs Locking

| Aspect | MGA (ScratchBird) | MVCC (PostgreSQL) | Locking (MySQL MyISAM) |
|--------|-------------------|-------------------|------------------------|
| Reader-Writer | No blocking | No blocking | Readers block writers |
| Writer-Reader | No blocking | No blocking | Writers block readers |
| Storage growth | Versions accumulate | Tuples accumulate | Minimal |
| Recovery | Database is log | WAL required | WAL required |
| Point-in-time | Built-in | Requires WAL replay | Not supported |
| Index bloat | Versions in index | Dead tuples in index | Minimal |

## Best Practices

### For Application Developers

1. **Expect versions to exist** - Don't assume DELETE removes data immediately
2. **Long transactions** - Hold snapshots longer = more versions retained
3. **Batch updates** - Fewer large transactions vs many small ones
4. **Monitor GC** - Watch `mga.version_count` metric

### For DBAs

1. **Storage planning** - MGA uses more storage than in-place updates
2. **GC tuning** - Adjust thresholds for workload
3. **Partitioning** - GC works per-partition
4. **Vacuum equivalent** - Run `ANALYZE` for statistics, GC is automatic

## See Also

- [MGA vs WAL](02_mga_vs_wal.md)
- [Transaction Lifecycle](03_transaction_lifecycle.md)
- [Visibility and Versioning](04_visibility_and_versioning.md)
- [GC Sweep and Maintenance](06_gc_sweep_and_maintenance.md)
