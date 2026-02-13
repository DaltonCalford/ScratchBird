# Architecture Clarification: Firebird MGA + WAL

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Core Architecture Decision

This project implements **Firebird's Multi-Generational Architecture (MGA)** as the PRIMARY mechanism for ACID compliance and transaction isolation, with Write-Ahead Logging (WAL) as a SECONDARY process for durability and recovery.

## MGA as Primary (Phases 6-7)

### Multi-Generational Architecture
- **Primary ACID mechanism**: All ACID properties achieved through versioning
- **No locks for readers**: Readers never block writers, writers never block readers
- **Version chains**: Each row update creates a new version, old versions retained
- **Transaction Inventory Pages (TIP)**: Track transaction states
- **Natural isolation**: Isolation levels achieved through snapshot visibility rules

### Key MGA Components
```cpp
// Every tuple has version information
struct TupleHeader {
    uint64_t created_xid;    // Transaction that created this version
    uint64_t deleted_xid;    // Transaction that deleted this version (0 if live)
    uint64_t backptr_rid;    // Previous version location
    // ... data follows
};

// TIP tracks transaction states without WAL
enum TxnState {
    Active = 1,
    Committed = 2,
    Aborted = 3,
    Limbo = 4  // Two-phase commit
};
```

### MGA Benefits
- **Natural MVCC**: No additional overhead for snapshots
- **Consistent reads**: Snapshots see consistent database state
- **No read locks**: Massive concurrency for read workloads
- **Rollback is free**: Just mark transaction as aborted

## WAL as Secondary (Phase 16)

### Write-Ahead Logging Role
- **Durability only**: Ensures committed transactions survive crashes
- **Not for ACID**: ACID already provided by MGA
- **Not for undo**: MGA handles rollback naturally
- **Careful write**: Only after MGA structures updated

### WAL Integration with MGA
```cpp
// WAL records MGA operations, doesn't drive them
struct WALRecord {
    uint64_t lsn;
    uint64_t xid;
    WalOpType op;  // Records what MGA already did
    // No undo information needed - MGA handles that
};

// Transaction commit sequence
1. Update TIP to mark transaction committed (MGA)
2. Write WAL record of commit (Durability)
3. Return success to client
```

## Corrected Phase Specifications

### Phase 6: MGA Transactions (Primary)
- Implement TIP pages for transaction state
- Version chains in tuples
- Transaction visibility without locks
- Garbage collection of old versions
- NO dependency on WAL for correctness

### Phase 7: MVCC via MGA (Primary)
- Snapshot isolation through version visibility
- Read Committed: New snapshot each statement
- Repeatable Read: Snapshot at transaction start
- Serializable: Via MGA + additional checks
- NO locks for read operations

### Phase 16: WAL for Durability (Secondary)
- Write-after-commit logging
- Minimal WAL records (no undo)
- Recovery replays only committed transactions
- Checkpoints write MGA state to disk
- WAL can be disabled (lose durability, keep ACID)

## Architecture Comparison

| Feature | Traditional WAL-based | Firebird MGA + WAL |
|---------|----------------------|-------------------|
| ACID Provider | WAL | MGA |
| Read Locks | Required | Never |
| Rollback | Undo via WAL | Mark aborted in TIP |
| Versions | Created for MVCC | Natural part of updates |
| WAL Size | Large (undo+redo) | Small (redo only) |
| Reader Impact | Can block | Never blocks |
| Garbage Collection | Not needed | Required for old versions |

## Implementation Priority

1. **MGA First**: Phases 6-7 implement complete MGA
2. **Test MGA**: Verify ACID without WAL
3. **Add WAL**: Phase 16 adds durability layer
4. **Test Recovery**: Verify WAL recovery of MGA state

## Critical Differences

### What MGA Provides (Without WAL)
- ✅ Atomicity (via TIP states)
- ✅ Consistency (via versioning)
- ✅ Isolation (via snapshots)
- ❌ Durability (lost on crash)

### What WAL Adds
- ✅ Durability (survive crashes)
- ✅ Point-in-time recovery
- ✅ Replication support

## Testing Requirements

### MGA Tests (No WAL)
```cpp
// Test ACID without WAL
disable_wal();
begin_transaction();
insert("data");
// Kill without commit - data not visible (Atomicity via MGA)

begin_transaction();
insert("data");
commit();  // Updates TIP
// Kill here - data lost (no Durability without WAL)
```

### MGA + WAL Tests
```cpp
// Test durability with WAL
enable_wal();
begin_transaction();
insert("data");
commit();  // TIP updated + WAL written
// Kill here - data recovered (Durability via WAL)
```

## Summary

The reorganized phases should emphasize:
1. **MGA is primary**: Provides ACID minus durability
2. **WAL is secondary**: Adds durability and recovery
3. **No read locks ever**: Core Firebird advantage
4. **Garbage collection required**: Old versions accumulate
5. **WAL optional**: System works without it (in-memory mode)

This architecture is fundamentally different from PostgreSQL/MySQL and must be clearly specified in the phase documents.
