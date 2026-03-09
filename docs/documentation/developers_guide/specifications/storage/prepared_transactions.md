# Specification: Prepared Transactions (Two-Phase Commit)

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/transaction_manager.h:161`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:588`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_2pc.cpp`

## Synopsis

This specification defines prepared transactions for distributed transaction support using the Two-Phase Commit (2PC) protocol. Prepared transactions survive crashes and must be explicitly committed or rolled back.

## Scope

### In Scope

- Two-phase commit protocol
- PREPARE TRANSACTION operation
- COMMIT PREPARED operation
- ROLLBACK PREPARED operation
- Prepared transaction recovery
- Global transaction ID (GID) management

### Out of Scope

- Distributed transaction coordinator implementation
- XA protocol specifics
- Heuristic completion
- Transaction manager migration

## Background

Two-Phase Commit ensures atomicity across multiple databases:

1. **Phase 1 (Prepare)**: Each participant prepares to commit
   - Write changes to disk
   - Respond "prepared" or "abort"
   - Enter "limbo" state

2. **Phase 2 (Commit/Rollback)**: Coordinator decides
   - If all prepared: Send commit
   - If any aborted: Send rollback
   - Participants complete

## Specification

### Data Structures

#### PreparedTransaction

```cpp
struct PreparedTransaction {
    // Identification
    std::string gid;               // Global transaction ID
    uint64_t local_xid;            // Local XID
    ID owner_id;                   // Owner identifier
    
    // State
    TransactionState state;        // PREPARED
    uint64_t prepare_time;         // When prepared
    
    // Resource state (for recovery)
    std::vector<GPID> modified_pages;  // Pages modified
    std::vector<ID> lock_holders;      // Locks held
    
    // For crash recovery
    uint64_t prepare_lsn;          // WAL LSN (future)
    bool in_doubt;                 // Uncertain outcome?
};
```

#### PreparedTransactionMap

```cpp
class TransactionManager {
    // In-memory map of prepared transactions
    std::unordered_map<std::string, PreparedTransaction> prepared_txns_;
    
    // XID -> GID mapping for quick lookup
    std::unordered_map<uint64_t, std::string> xid_to_gid_;
    
    // For OAT computation - prepared XIDs must be considered "active"
    std::unordered_set<uint64_t> prepared_xids_;
};
```

### Global Transaction ID (GID)

**Format**: Free-form string, typically:
- `hostname.transaction_id.timestamp`
- `xa.{xid_format_id}.{xid_gtrid_length}.{xid_bqual_length}.{xid_data}`
- Maximum length: 200 characters

### Interface Contracts

#### Function: `prepareTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp:588
Status TransactionManager::prepareTransaction(
    uint32_t proc_id,
    uint64_t xid,
    const std::string& gid,
    const ID& owner_id,
    ErrorContext *ctx
);
```

**Preconditions:**
- Transaction state is ACTIVE
- GID not already in use
- All changes flushed to disk (WAL or direct)

**Postconditions:**
- State set to PREPARED
- Transaction data persisted
- XID kept in ProcArray (visible for conflict detection)

**Algorithm:**
```
1. Verify transaction is ACTIVE
2. Verify GID not already in prepared_txns_

3. // Ensure durability
4. flushAllDirtyPages()
5. fsync()  // Critical: Changes must survive crash

6. // Create prepared transaction record
7. PreparedTransaction pt;
8. pt.gid = gid;
9. pt.local_xid = xid;
10. pt.owner_id = owner_id;
11. pt.state = TransactionState::PREPARED;
12. pt.prepare_time = now();
13. pt.modified_pages = getModifiedPages(xid);
14. pt.lock_holders = getLockHolders(xid);

15. // Persist to TIP
16. writeTipEntry(xid, TransactionState::PREPARED, ctx)
17. 
18. // Add to in-memory maps
19. prepared_txns_[gid] = pt;
20. xid_to_gid_[xid] = gid;
21. prepared_xids_.insert(xid);

22. // Keep XID in ProcArray (still "active" for visibility)
23. // Don't clear transaction ID from backend

24. RETURN OK
```

#### Function: `commitPreparedTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp
Status TransactionManager::commitPreparedTransaction(
    const std::string& gid,
    ErrorContext *ctx
);
```

**Preconditions:**
- Prepared transaction with GID exists
- Caller has appropriate privileges

**Postconditions:**
- State set to COMMITTED
- Locks released
- ProcArray cleared
- Prepared transaction record removed

**Algorithm:**
```
1. Find prepared transaction by GID
2. IF not found: RETURN NOT_FOUND

3. xid = pt.local_xid

4. // Update state
5. clog_->setStatus(xid, ClogStatus::COMMITTED)
6. writeTipEntry(xid, TransactionState::COMMITTED, ctx)

7. // Release resources
8. releaseLocks(xid)
9. clearProcArrayEntry(xid)

10. // Remove from prepared maps
11. prepared_txns_.erase(gid)
12. xid_to_gid_.erase(xid)
13. prepared_xids_.erase(xid)

14. RETURN OK
```

#### Function: `rollbackPreparedTransaction()`

```cpp
// Source: src/core/transaction_manager.cpp
Status TransactionManager::rollbackPreparedTransaction(
    const std::string& gid,
    ErrorContext *ctx
);
```

**Preconditions:**
- Prepared transaction with GID exists

**Postconditions:**
- State set to ABORTED
- Changes undone (via back versions)
- Locks released
- Prepared transaction record removed

**Algorithm:**
```
1. Find prepared transaction by GID
2. IF not found: RETURN NOT_FOUND

3. xid = pt.local_xid

4. // Rollback changes
5. FOR each page IN pt.modified_pages:
6.     rollbackPageChanges(page, xid)

7. // Update state
8. clog_->setStatus(xid, ClogStatus::ABORTED)
9. writeTipEntry(xid, TransactionState::ABORTED, ctx)

10. // Release resources
11. releaseLocks(xid)
12. clearProcArrayEntry(xid)

13. // Remove from prepared maps
14. prepared_txns_.erase(gid)
15. xid_to_gid_.erase(xid)
16. prepared_xids_.erase(xid)

17. RETURN OK
```

### Recovery

```
Algorithm: recoverPreparedTransactions()

1. // Scan TIP for PREPARED transactions
2. prepared_list = []
3. FOR each TIP page:
4.     FOR each entry IN tip_page.entries:
5.         IF entry.state == PREPARED:
6.             prepared_list.push_back(entry.xid)

7. // Rebuild in-memory state
8. FOR each xid IN prepared_list:
9.     gid = readGIDFromStorage(xid)
10.    owner_id = readOwnerFromStorage(xid)
11.    
12.    PreparedTransaction pt;
13.    pt.gid = gid;
14.    pt.local_xid = xid;
15.    pt.owner_id = owner_id;
16.    pt.state = PREPARED;
17.    
18.    prepared_txns_[gid] = pt;
19.    xid_to_gid_[xid] = gid;
20.    prepared_xids_.insert(xid);

21. // Register in ProcArray (for conflict detection)
22. FOR each xid IN prepared_list:
23.    registerPreparedInProcArray(xid)

24. LOG info: "Recovered N prepared transactions"
```

### State Machine

```
┌─────────┐ prepare()  ┌─────────┐ commit_prepared() ┌─────────┐
│  ACTIVE │ ─────────► │ PREPARED│ ─────────────────►│COMMITTED│
└─────────┘            │ (limbo) │                   └─────────┘
                       └────┬────┘
                            │ rollback_prepared()
                            ▼
                       ┌─────────┐
                       │ ABORTED │
                       └─────────┘
```

## Invariants

1. **Durability**: PREPARED transactions survive crashes
   - Verification: fsync before acknowledging prepare
   
2. **GID Uniqueness**: No two prepared transactions share GID
   - Verification: Check before prepare
   
3. **OAT Consideration**: PREPARED XIDs included in OAT computation
   - Verification: OAT = min(OAT from ProcArray, min(prepared_xids_))
   
4. **Resource Retention**: Locks and resources held until commit/rollback
   - Verification: Released in commit/rollback functions

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `NOT_FOUND` | GID not in prepared_txns_ | Return error |
| `DUPLICATE_GID` | GID already exists | Return error |
| `ACTIVE_TRANSACTION` | Transaction not ready for prepare | Return error |
| `IO_ERROR` | fsync failure during prepare | Return error, transaction stays ACTIVE |

## Performance Considerations

### fsync Overhead
- **Prepare requires fsync**: Ensures durability
- **Cost**: ~1-5ms per fsync (SSD)
- **Optimization**: Batch prepares with group commit (future)

### Memory Usage
- **Per prepared transaction**: ~2KB metadata
- **Limit**: max_prepared_transactions configuration
- **Cleanup**: Automatic on commit/rollback

### Lock Retention
- **Prepared transactions hold locks**: Prevents conflicts
- **Duration**: Until explicit commit/rollback
- **Risk**: Long-prepared transactions block others

## Monitoring

```sql
-- View prepared transactions
SELECT * FROM sb_prepared_xacts;

-- Columns:
-- gid: Global transaction ID
-- local_xid: Local XID
-- owner_id: Owner
-- prepare_time: When prepared
-- in_doubt: Uncertain outcome?
```

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_2pc.cpp` | Basic 2PC |
| `tests/unit/test_2pc_recovery.cpp` | Crash recovery |
| `tests/unit/test_2pc_concurrent.cpp` | Concurrent prepares |

## Related Specifications

- [Transaction States](./transaction_states.md) - PREPARED state
- [Transaction Lifecycle](./transaction_lifecycle.md) - Transaction management
- [Lock Manager](./lock_manager.md) - Lock retention during prepare

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
