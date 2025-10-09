# Isolation Levels Implementation Design

**Date:** October 9, 2025
**Author:** Phase 3, Task 3.2
**Status:** Design Document
**Related:** FIREBIRD_TRANSACTION_MODEL_SPEC.md, ALPHA_1_2_IMPLEMENTATION_PLAN.md

---

## Executive Summary

This document specifies the design for implementing four Firebird-style isolation levels in ScratchBird:
1. **READ COMMITTED** - Each statement sees latest committed data
2. **READ COMMITTED READ CONSISTENCY** - Statement-level snapshot (Firebird 4.0+)
3. **SNAPSHOT** - Point-in-time snapshot at transaction start (default)
4. **SNAPSHOT TABLE STABILITY** - Snapshot with table-level locking

The implementation builds on existing infrastructure (ConnectionContext, TransactionManager, Snapshot) and adds isolation-level-aware visibility checking.

---

## Architecture Overview

### Current State (Phase 3.1 Complete)

```
ConnectionContext
  ├─ isolation_level_: IsolationLevel (enum)
  ├─ snapshot_: unique_ptr<Snapshot> (created for SNAPSHOT isolation)
  └─ current_xid_: uint64_t

TransactionManager
  ├─ Snapshot { xmin, xmax, active_xids[] }
  ├─ getSnapshot() - creates snapshot
  └─ isTransactionVisible(xid, snapshot_xid) - simple visibility

StorageEngine
  └─ isVisible(xmin, xmax, current_xid) - delegates to TransactionManager
```

### Target Architecture (Phase 3.2)

```
ConnectionContext
  ├─ isolation_level_: IsolationLevel
  ├─ snapshot_: unique_ptr<Snapshot> (for SNAPSHOT/SNAPSHOT_TABLE_STABILITY)
  ├─ statement_snapshot_: unique_ptr<Snapshot> (for READ_COMMITTED_READ_CONSISTENCY)
  └─ table_reservations_: vector<TableReservation>

TransactionManager
  ├─ isTransactionVisible(xid, snapshot_xid) - simple READ COMMITTED
  ├─ isSnapshotVisible(xid, Snapshot*) - SNAPSHOT isolation NEW
  └─ getSnapshot() - unchanged

StorageEngine
  └─ isVisible(xmin, xmax, current_xid) - isolation-level-aware NEW
```

---

## Isolation Level Semantics

### 1. READ COMMITTED (IsolationLevel::READ_COMMITTED)

**Firebird Semantics:**
- Each statement sees the latest committed version of data
- Non-repeatable reads are possible (data can change between statements)
- Phantom reads are possible (new rows can appear)
- Lowest isolation level, highest concurrency

**Visibility Rules:**
```cpp
// Tuple is visible if:
1. xmin is committed AND xmin < current transaction's XID
2. xmax is 0 (not deleted) OR xmax is not committed OR xmax >= current XID
3. Tuple creator (xmin) != current transaction AND tuple is committed
4. OR tuple creator == current transaction (see own changes)
```

**Implementation:**
- No snapshot needed
- Use existing `TransactionManager::isTransactionVisible(xid, current_xid)`
- Check transaction state (COMMITTED) from CLOG/TIP
- This is essentially what we have now

**Advantages:**
- Lowest overhead (no snapshot storage)
- Maximum concurrency
- Sees latest data

**Disadvantages:**
- Non-repeatable reads
- Phantom reads
- May see inconsistent state across statements

---

### 2. READ COMMITTED READ CONSISTENCY (IsolationLevel::READ_COMMITTED_READ_CONSISTENCY)

**Firebird Semantics (Firebird 4.0+):**
- Each **statement** gets a point-in-time snapshot
- Within a statement, reads are consistent
- Between statements, reads see latest committed data
- Prevents lost updates while maintaining high concurrency

**Visibility Rules:**
```cpp
// Per-statement snapshot:
1. Create snapshot at statement start
2. Use snapshot-based visibility for duration of statement
3. Discard snapshot at statement end
4. Next statement gets new snapshot
```

**Implementation:**
- `ConnectionContext::statement_snapshot_` - created at statement start
- Use snapshot-based visibility during statement execution
- Clear snapshot after statement completes
- Requires executor integration to detect statement boundaries

**Advantages:**
- Consistent reads within a statement
- Sees latest data between statements
- Prevents lost updates

**Disadvantages:**
- Requires statement boundary detection
- More overhead than pure READ COMMITTED
- Still allows phantom reads between statements

**Statement Boundary Detection:**
- Executor creates statement context
- `ConnectionContext::beginStatement()` - creates statement snapshot
- `ConnectionContext::endStatement()` - discards statement snapshot
- Read operations use statement snapshot if available

---

### 3. SNAPSHOT (IsolationLevel::SNAPSHOT)

**Firebird Semantics:**
- Point-in-time snapshot at **transaction start**
- All reads within transaction see consistent state
- Repeatable reads guaranteed
- No phantom reads
- Default isolation level in ScratchBird

**Visibility Rules:**
```cpp
// Using transaction snapshot (created at BEGIN):
1. If xid == current_xid: VISIBLE (see own changes)
2. If xid > snapshot.xmax: INVISIBLE (started after our snapshot)
3. If xid < snapshot.xmin: VISIBLE if COMMITTED (old transaction)
4. If xid in snapshot.active_xids[]: INVISIBLE (was active at snapshot time)
5. Otherwise: VISIBLE if COMMITTED
```

**Implementation:**
- `ConnectionContext::snapshot_` created in `beginNewTransaction()`
- Snapshot contains:
  ```cpp
  struct Snapshot {
      uint64_t xmin;                     // Oldest active XID at snapshot time
      uint64_t xmax;                     // Next XID to be assigned
      std::vector<uint64_t> active_xids; // All active XIDs at snapshot time
  }
  ```
- Use `TransactionManager::isSnapshotVisible(xid, snapshot)` for all reads
- Snapshot persists for entire transaction lifetime

**Advantages:**
- Repeatable reads
- No phantom reads
- Consistent view throughout transaction
- Excellent for read-heavy workloads

**Disadvantages:**
- May see "old" data if transaction runs long
- Write skew anomalies possible
- Snapshot storage overhead

---

### 4. SNAPSHOT TABLE STABILITY (IsolationLevel::SNAPSHOT_TABLE_STABILITY)

**Firebird Semantics:**
- Snapshot isolation PLUS table-level locking
- Tables are reserved (locked) at transaction start
- Provides serializable isolation
- Prevents write skew and ensures true isolation

**Visibility Rules:**
```cpp
// Same as SNAPSHOT, plus:
1. Use snapshot-based visibility (same as SNAPSHOT)
2. Table locks prevent concurrent modifications
3. Lock modes:
   - SHARED READ: Allows concurrent SHARED READ, blocks PROTECTED
   - PROTECTED READ/WRITE: Exclusive access
```

**Implementation:**
- Same snapshot as SNAPSHOT isolation
- Additional table locks via `ConnectionContext::reserveTables()`
- Lock acquisition in `beginNewTransaction()` after snapshot creation
- Locks held until transaction end (commit/rollback)
- RESERVING clause specifies which tables and lock modes:
  ```sql
  START TRANSACTION
    ISOLATION LEVEL SNAPSHOT TABLE STABILITY
    RESERVING users FOR SHARED READ,
              orders FOR PROTECTED WRITE;
  ```

**Advantages:**
- Serializable isolation
- Prevents write skew
- Prevents concurrent modifications

**Disadvantages:**
- Lowest concurrency
- Can cause deadlocks
- Requires explicit table reservation

---

## Visibility Checking Algorithm

### Current Implementation (Simple)

```cpp
// TransactionManager::isTransactionVisible(xid, snapshot_xid)
bool TransactionManager::isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) {
    // Validate XID
    if (!isXidInRange(xid)) return false;

    // See own changes
    if (xid == snapshot_xid) return true;

    // Future transaction
    if (xid > snapshot_xid) return false;

    // Frozen tuples always visible
    if (xid <= FROZEN_XID) return true;

    // Check if committed
    TransactionState state;
    getTransactionState(xid, state, nullptr);
    return state == TransactionState::COMMITTED;
}
```

**Problem:** This is READ COMMITTED semantics, not snapshot isolation!

### New Implementation (Snapshot-Based)

```cpp
// TransactionManager::isSnapshotVisible(xid, snapshot*)
bool TransactionManager::isSnapshotVisible(uint64_t xid, const Snapshot* snapshot) {
    // 1. Validate XID
    if (!isXidInRange(xid)) return false;

    // 2. See own changes (xid == current transaction's XID)
    //    Note: Caller must check this separately with current_xid

    // 3. Future transaction (started after snapshot)
    if (xid >= snapshot->xmax) return false;

    // 4. Frozen tuples always visible
    if (xid <= FROZEN_XID) return true;

    // 5. Transaction was active at snapshot time - INVISIBLE
    if (std::binary_search(snapshot->active_xids.begin(),
                          snapshot->active_xids.end(), xid)) {
        return false;
    }

    // 6. Old transaction (started before snapshot.xmin)
    //    Must be committed to be visible
    if (xid < snapshot->xmin) {
        TransactionState state;
        getTransactionState(xid, state, nullptr);
        return state == TransactionState::COMMITTED;
    }

    // 7. Transaction started after xmin but before xmax,
    //    and was NOT in active list - must have committed before snapshot
    //    Check CLOG to confirm
    TransactionState state;
    getTransactionState(xid, state, nullptr);
    return state == TransactionState::COMMITTED;
}
```

**Key Differences:**
- Checks `active_xids` array - transactions active at snapshot time are invisible
- Uses `snapshot->xmax` as boundary (not current_xid)
- Provides repeatable reads

### Isolation-Level-Aware Visibility

```cpp
// StorageEngine::isVisible(xmin, xmax, current_xid) - UPDATED
bool StorageEngine::isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) {
    TransactionManager* tm = db_->transaction_manager();

    // Get current connection context to determine isolation level
    ConnectionContext* ctx = ConnectionContext::getCurrent();

    // Validate XIDs
    if (!tm->isXidInRange(xmin)) return false;
    if (xmax != 0 && !tm->isXidInRange(xmax)) xmax = 0;

    // Determine visibility based on isolation level
    IsolationLevel isolation = (ctx != nullptr)
        ? ctx->getIsolationLevel()
        : IsolationLevel::READ_COMMITTED;

    bool xmin_visible = false;
    bool xmax_visible = false;

    switch (isolation) {
        case IsolationLevel::READ_COMMITTED:
            // Use simple visibility (existing implementation)
            xmin_visible = tm->isTransactionVisible(xmin, current_xid);
            if (xmax != 0) {
                xmax_visible = tm->isTransactionVisible(xmax, current_xid);
            }
            break;

        case IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
            // Use statement snapshot if available
            if (ctx != nullptr && ctx->getStatementSnapshot() != nullptr) {
                // Check if we see our own changes first
                if (xmin == current_xid) {
                    xmin_visible = true;
                } else {
                    xmin_visible = tm->isSnapshotVisible(xmin, ctx->getStatementSnapshot());
                }

                if (xmax != 0) {
                    if (xmax == current_xid) {
                        xmax_visible = true;
                    } else {
                        xmax_visible = tm->isSnapshotVisible(xmax, ctx->getStatementSnapshot());
                    }
                }
            } else {
                // Fallback to READ COMMITTED
                xmin_visible = tm->isTransactionVisible(xmin, current_xid);
                if (xmax != 0) {
                    xmax_visible = tm->isTransactionVisible(xmax, current_xid);
                }
            }
            break;

        case IsolationLevel::SNAPSHOT:
        case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
            // Use transaction snapshot
            if (ctx != nullptr && ctx->getSnapshot() != nullptr) {
                // Check if we see our own changes first
                if (xmin == current_xid) {
                    xmin_visible = true;
                } else {
                    xmin_visible = tm->isSnapshotVisible(xmin, ctx->getSnapshot());
                }

                if (xmax != 0) {
                    if (xmax == current_xid) {
                        xmax_visible = true;
                    } else {
                        xmax_visible = tm->isSnapshotVisible(xmax, ctx->getSnapshot());
                    }
                }
            } else {
                // Fallback to READ COMMITTED (should not happen)
                xmin_visible = tm->isTransactionVisible(xmin, current_xid);
                if (xmax != 0) {
                    xmax_visible = tm->isTransactionVisible(xmax, current_xid);
                }
            }
            break;
    }

    // Tuple is visible if:
    // - Creating transaction (xmin) is visible
    // - AND deleting transaction (xmax) is NOT visible (or doesn't exist)
    return xmin_visible && !xmax_visible;
}
```

---

## Implementation Steps

### Step 1: Add Snapshot-Based Visibility to TransactionManager (2 days)

**Files to modify:**
- `include/scratchbird/core/transaction_manager.h`
- `src/core/transaction_manager.cpp`

**Changes:**
1. Add new method:
   ```cpp
   bool isSnapshotVisible(uint64_t xid, const Snapshot* snapshot) const;
   ```

2. Ensure `active_xids` in snapshot is sorted (for binary_search)
   - Update `getSnapshot()` to sort the active_xids vector

3. Implement snapshot-based visibility algorithm (see above)

4. Keep existing `isTransactionVisible()` for READ COMMITTED

**Testing:**
- Unit test with mock snapshots
- Verify active transaction detection
- Verify xmin/xmax boundary checking

### Step 2: Update StorageEngine with Isolation-Aware Visibility (1 day)

**Files to modify:**
- `src/core/storage_engine.cpp`

**Changes:**
1. Update `isVisible()` to get ConnectionContext
2. Switch on isolation level
3. Call appropriate visibility function
4. Handle "see own changes" logic

**Testing:**
- Test with each isolation level
- Verify ConnectionContext integration

### Step 3: Add Statement Snapshot Support (2 days)

**Files to modify:**
- `include/scratchbird/core/connection_context.h`
- `src/core/connection_context.cpp`
- `include/scratchbird/sblr/executor.h` (for statement boundaries)
- `src/sblr/executor.cpp`

**Changes:**
1. Add `statement_snapshot_` member to ConnectionContext
2. Add methods:
   ```cpp
   Status beginStatement(ErrorContext* ctx);
   void endStatement();
   const Snapshot* getStatementSnapshot() const;
   ```

3. Executor integration:
   - Call `beginStatement()` at start of each SQL statement
   - Call `endStatement()` after statement completion
   - Only for READ_COMMITTED_READ_CONSISTENCY isolation

**Testing:**
- Verify statement snapshots created/destroyed
- Test visibility within vs. between statements

### Step 4: Implement Table Locking for SNAPSHOT TABLE STABILITY (2 days)

**Files to modify:**
- `src/core/connection_context.cpp` (enable TODO for table locking)
- `src/core/lock_manager.cpp` (if needed)

**Changes:**
1. Enable table lock acquisition in `beginNewTransaction()`
2. Implement lock acquisition for reserved tables:
   ```cpp
   for (auto& reservation : table_reservations_) {
       Status s = acquireTableLock(reservation.table_name,
                                    reservation.lock_mode,
                                    reservation.for_write,
                                    ctx);
       if (s != Status::OK) return s;
   }
   ```

3. Locks released automatically at transaction end (via LockManager)

**Testing:**
- Test SHARED READ locks (concurrent)
- Test PROTECTED locks (exclusive)
- Test lock conflicts
- Test deadlock detection

### Step 5: Testing & Validation (2 days)

**Test Cases:**

**READ COMMITTED:**
- [ ] Non-repeatable reads work correctly
- [ ] Phantom reads work correctly
- [ ] Sees latest committed data

**READ COMMITTED READ CONSISTENCY:**
- [ ] Statement-level repeatable reads
- [ ] Different data between statements
- [ ] No phantom reads within statement

**SNAPSHOT:**
- [ ] Repeatable reads across entire transaction
- [ ] No phantom reads
- [ ] Don't see concurrent commits
- [ ] See own changes

**SNAPSHOT TABLE STABILITY:**
- [ ] Same as SNAPSHOT
- [ ] Table locks prevent concurrent access
- [ ] SHARED READ allows concurrent SHARED READ
- [ ] PROTECTED blocks all concurrent access

**Cross-Isolation Testing:**
- [ ] Multiple connections with different isolation levels
- [ ] Verify each sees appropriate data

---

## Performance Considerations

### Memory Overhead

| Isolation Level | Snapshot Storage | Active XIDs Array |
|----------------|------------------|-------------------|
| READ COMMITTED | None | None |
| READ_COMMITTED_READ_CONSISTENCY | Per statement | Yes (~100-1000 XIDs) |
| SNAPSHOT | Per transaction | Yes (~100-1000 XIDs) |
| SNAPSHOT_TABLE_STABILITY | Per transaction | Yes (~100-1000 XIDs) |

**Memory Usage:**
- Snapshot: ~32 bytes + (8 bytes × number of active XIDs)
- Typical: ~1 KB per snapshot with 100 active transactions

### CPU Overhead

| Isolation Level | Visibility Check Cost |
|----------------|----------------------|
| READ COMMITTED | Low (CLOG lookup) |
| READ_COMMITTED_READ_CONSISTENCY | Medium (snapshot + binary search) |
| SNAPSHOT | Medium (snapshot + binary search) |
| SNAPSHOT_TABLE_STABILITY | Medium (same as SNAPSHOT) |

**Optimizations:**
- Keep `active_xids` sorted for O(log N) binary search
- Cache transaction states in TransactionManager
- Use vectorized comparisons where possible

### Concurrency

| Isolation Level | Concurrency | Use Case |
|----------------|-------------|----------|
| READ COMMITTED | Highest | High-throughput OLTP |
| READ_COMMITTED_READ_CONSISTENCY | High | OLTP with consistency needs |
| SNAPSHOT | Medium | Reporting, analytics |
| SNAPSHOT_TABLE_STABILITY | Lowest | Data migration, bulk operations |

---

## Migration Path

### Phase 1: Basic Snapshot Visibility (Week 1, Days 1-2)
- Implement `isSnapshotVisible()`
- Update `StorageEngine::isVisible()` for SNAPSHOT isolation only
- Test SNAPSHOT isolation thoroughly

### Phase 2: READ COMMITTED Support (Week 1, Days 3-4)
- Verify existing implementation works correctly
- Add tests for READ COMMITTED
- Ensure no regressions

### Phase 3: Statement Snapshots (Week 1, Day 5 - Week 2, Day 1)
- Add statement snapshot infrastructure
- Executor integration
- Test READ_COMMITTED_READ_CONSISTENCY

### Phase 4: Table Locking (Week 2, Days 2-3)
- Enable table lock acquisition
- Test SNAPSHOT_TABLE_STABILITY
- Deadlock testing

### Phase 5: Integration & Testing (Week 2, Days 4-5)
- Cross-isolation testing
- Performance benchmarks
- Documentation updates

---

## Testing Strategy

### Unit Tests

```cpp
TEST(IsolationLevels, SnapshotVisibility) {
    // Create snapshot with xmin=100, xmax=105, active=[101, 103]
    Snapshot snap;
    snap.xmin = 100;
    snap.xmax = 105;
    snap.active_xids = {101, 103};
    std::sort(snap.active_xids.begin(), snap.active_xids.end());

    TransactionManager tm(db);

    // XID 99: old committed - VISIBLE
    EXPECT_TRUE(tm.isSnapshotVisible(99, &snap));

    // XID 101: was active - INVISIBLE
    EXPECT_FALSE(tm.isSnapshotVisible(101, &snap));

    // XID 102: committed before snap - VISIBLE
    EXPECT_TRUE(tm.isSnapshotVisible(102, &snap));

    // XID 105: future - INVISIBLE
    EXPECT_FALSE(tm.isSnapshotVisible(105, &snap));
}
```

### Integration Tests

```cpp
TEST(IsolationLevels, RepeatableReads) {
    // Connection 1: SNAPSHOT isolation
    auto conn1 = db.connect();
    conn1->startTransaction(false, IsolationLevel::SNAPSHOT, true);

    // Connection 2: INSERT new row
    auto conn2 = db.connect();
    conn2->execute("INSERT INTO users VALUES (1, 'Alice')");
    conn2->commit();

    // Connection 1: Should NOT see Alice (repeatable read)
    auto result = conn1->execute("SELECT * FROM users");
    EXPECT_EQ(result.rowCount(), 0);

    conn1->commit();

    // After commit, new transaction sees Alice
    result = conn1->execute("SELECT * FROM users");
    EXPECT_EQ(result.rowCount(), 1);
}
```

### Performance Tests

```cpp
BENCHMARK(IsolationLevels, SnapshotOverhead) {
    // Compare READ COMMITTED vs SNAPSHOT performance
    // Measure visibility check latency
    // Measure memory usage with varying active transaction counts
}
```

---

## Success Criteria

Task 3.2 is complete when:

- [ ] All 4 isolation levels implemented
- [ ] Snapshot-based visibility working correctly
- [ ] Statement snapshots for READ_COMMITTED_READ_CONSISTENCY
- [ ] Table locking for SNAPSHOT_TABLE_STABILITY
- [ ] All unit tests passing
- [ ] All integration tests passing
- [ ] No performance regressions for READ COMMITTED
- [ ] Documentation updated
- [ ] Code compiles without warnings

---

## References

1. **Firebird Documentation**: Transaction Isolation Levels
2. **PostgreSQL MVCC**: Implementation details (similar snapshot approach)
3. **ALPHA_1_2_IMPLEMENTATION_PLAN.md**: Phase 3, Task 3.2
4. **FIREBIRD_TRANSACTION_MODEL_SPEC.md**: Firebird transaction semantics

---

**Design Version:** 1.0
**Date:** October 9, 2025
**Status:** Ready for Implementation
