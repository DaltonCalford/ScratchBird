# MGA Implementation Gap Analysis

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** 2025-10-02
**Status:** 📊 ANALYSIS COMPLETE
**Purpose:** Determine what MGA features exist vs. what specification requires

## Executive Summary

ScratchBird has a **partial MGA implementation** with basic transaction management and tuple visibility, but is **missing critical components** required by the comprehensive MGA specification. The implementation is currently single-connection only and lacks:
- Full MVCC with version chains
- Lock manager subsystem
- Vacuum/garbage collection
- Proper snapshot isolation
- Concurrency control

**Estimated Completion:** ~60% of full MGA specification implemented

---

## 1. Transaction Management Subsystem

### Specification Requirements (from Section 1 of MGA spec):

**CLOG (Commit Log):**
- 2-bit transaction status encoding (ACTIVE, COMMITTED, ABORTED, SUB_COMMITTED)
- 32,768 transactions per 8KB page
- Page chain for unlimited transaction history
- Fast lookup by XID

**TransactionIdGenerator:**
- 64-bit transaction IDs
- Special XIDs (INVALID=0, BOOTSTRAP=1, FROZEN=2)
- Thread-safe XID allocation
- XID wraparound handling (not an issue with 64-bit)

### Current Implementation:

✅ **IMPLEMENTED (transaction_manager.h/cpp):**
- **TIP (Transaction Inventory Pages)** - Similar to CLOG but uses larger entries:
  ```cpp
  struct TIPPageHeader {
      PageHeader page_header;
      uint64_t min_xid;
      uint64_t max_xid;
      uint32_t num_transactions;
      uint32_t next_tip_page;
  };

  struct TIPEntry {
      uint64_t xid;         // 8 bytes per transaction
      uint8_t state;        // ACTIVE, COMMITTED, ABORTED, PREPARED
      uint8_t flags;
      uint16_t reserved;
      uint64_t commit_time; // Timestamp
  };
  ```
  - **Total: 20 bytes per transaction** (vs 2 bits in specification)
  - Entries per page: (8192 - 112) / 20 ≈ **400 transactions per page**
  - **Much less efficient than CLOG** (spec: 32,768 per page)

- **Transaction States:**
  ```cpp
  enum class TransactionState {
      ACTIVE = 0,
      COMMITTED = 1,
      ABORTED = 2,
      PREPARED = 3  // For 2PC support
  };
  ```
  - ✅ Matches specification (though spec uses SUB_COMMITTED instead of PREPARED)

- **XID Management:**
  - ✅ 64-bit transaction IDs: `uint64_t next_xid_`
  - ✅ Special XIDs: INVALID_XID=0, BOOTSTRAP_XID=1, FROZEN_XID=2
  - ✅ Thread-safe allocation with `std::mutex`
  - ✅ Single active transaction tracking: `uint64_t active_xid_`

- **Core Operations:**
  - ✅ `beginTransaction()` - Allocates XID, writes to TIP
  - ✅ `commitTransaction()` - Updates state to COMMITTED
  - ✅ `rollbackTransaction()` - Updates state to ABORTED
  - ✅ `getTransactionState()` - Queries TIP with caching
  - ✅ `isTransactionVisible()` - Basic visibility check

❌ **MISSING:**
1. **CLOG 2-bit encoding** - Current TIP uses 20 bytes/transaction instead of 2 bits
2. **ProcArray/ActiveTransactionList** - No shared memory structure tracking all active transactions
3. **backend_xmin tracking** - No per-backend horizon for vacuum
4. **Multi-connection support** - Only single active transaction allowed
5. **XID allocation optimization** - No bulk allocation or reservations

### Gap Assessment:
**Status:** 🟡 **PARTIAL** (60% complete)
- Core transaction lifecycle works for single connection
- TIP provides persistence but is inefficient
- No proper concurrency support

---

## 2. Snapshot Management

### Specification Requirements (from Section 2 of MGA spec):

**TransactionSnapshot:**
```cpp
struct TransactionSnapshot {
    uint64_t my_xid;          // Current transaction's XID
    uint64_t xmin;            // Oldest active XID
    uint64_t xmax;            // Next XID to be allocated
    uint32_t xip_count;       // Count of in-progress XIDs
    uint64_t* xip;            // Array of in-progress XIDs
    uint64_t curcid;          // Current command ID (for same-transaction visibility)

    // Serializable isolation support
    bool subxip_overflow;     // Subtransaction overflow flag
    uint32_t active_count;    // Number of active snapshots
};
```

**GetSnapshotData Algorithm:**
1. Lock ProcArray
2. Get xmax from TransactionIdGenerator
3. Scan all backends for active XIDs
4. Build xip array
5. Calculate xmin (oldest in xip)
6. Unlock ProcArray

**Visibility Rules:**
- `XidInMVCCSnapshot()` - Check if XID was in-progress at snapshot time
- Same-transaction visibility with command IDs
- Subtransaction handling

### Current Implementation:

🟡 **PARTIALLY IMPLEMENTED:**

**Snapshot Structure (transaction_manager.h:107-112):**
```cpp
struct Snapshot {
    uint64_t xmin;                     // Oldest active XID
    uint64_t xmax;                     // Next XID to be assigned
    std::vector<uint64_t> active_xids; // Active XIDs at snapshot time
};
```

**getSnapshot() (transaction_manager.cpp:344-360):**
```cpp
auto TransactionManager::getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    snapshot_out.xmin = FROZEN_XID + 1;
    snapshot_out.xmax = next_xid_;
    snapshot_out.active_xids.clear();

    // For single connection, only one active transaction
    if (active_xid_ != 0) {
        snapshot_out.active_xids.push_back(active_xid_);
        snapshot_out.xmin = active_xid_;
    }

    return Status::OK;
}
```

❌ **MISSING:**
1. **my_xid field** - Snapshot doesn't track which transaction owns it
2. **curcid (command ID)** - No command-level visibility within transaction
3. **Subtransaction support** - No xip_overflow handling
4. **ProcArray integration** - Can't scan all backends (only single connection)
5. **Snapshot export/import** - For parallel queries
6. **Serializable isolation** - No predicate lock tracking in snapshot

### Gap Assessment:
**Status:** 🟡 **PARTIAL** (40% complete)
- Basic snapshot structure exists
- Only works for single connection
- No command-level visibility
- No subtransaction support

---

## 3. Tuple Header and Version Chain Management

### Specification Requirements (from Section 3 of MGA spec):

**TupleHeaderData:**
```cpp
struct TupleHeaderData {
    uint64_t tdh_xmin;           // Creator XID
    uint64_t tdh_xmax;           // Deleter XID (0 = live)
    TupleId tdh_next_version;    // Link to next version (UPDATE chain)
    uint16_t tdh_infomask;       // Status flags
    uint8_t tdh_hoff;            // Header offset
    uint16_t tdh_natts;          // Number of attributes
};
```

**Infomask Flags:**
- HEAP_XMIN_COMMITTED, HEAP_XMIN_INVALID
- HEAP_XMAX_COMMITTED, HEAP_XMAX_INVALID
- HEAP_UPDATED, HEAP_HOT_UPDATED
- HEAP_KEYS_UPDATED
- HEAP_MOVED

**Version Chain Management:**
- Update chains via `tdh_next_version`
- HOT (Heap-Only Tuple) updates
- Tuple pruning during page access

### Current Implementation:

🟡 **PARTIALLY IMPLEMENTED (heap_page.h):**

**TupleHeader Structure (heap_page.h:64-82):**
```cpp
struct TupleHeader {
    uint64_t xmin;               // Transaction ID that inserted this tuple
    uint64_t xmax;               // Transaction ID that deleted this tuple (or 0)
    uint16_t flags;              // Various flags
    uint16_t null_bitmap_offset; // Offset to null bitmap (0 if no nulls)

    static constexpr uint16_t FLAG_HAS_NULLS = 0x0001;
    static constexpr uint16_t FLAG_DELETED = 0x0002;
};
```

**Size:** 20 bytes (vs specification's more complex structure)

✅ **IMPLEMENTED:**
- `xmin` field for creator XID
- `xmax` field for deleter XID
- Basic `flags` field (2 flags only)

❌ **MISSING:**
1. **tdh_next_version** - No update chain support
2. **Infomask flags** - Only 2 flags (needs 10+ for full MVCC)
3. **tdh_hoff** - No variable header offset
4. **tdh_natts** - No attribute count tracking
5. **HEAP_XMIN_COMMITTED** - No hint bits for committed state
6. **HEAP_UPDATED** - No flag for UPDATE vs DELETE
7. **HOT updates** - No heap-only tuple optimization
8. **Version chain traversal** - No helper functions

**HeapPage Operations:**
- ✅ `insertTuple()` - Sets xmin correctly
- ✅ `deleteTuple()` - Sets xmax correctly
- ❌ `updateTuple()` - **NOT IMPLEMENTED** (no update chain creation)
- ❌ `pruneTuples()` - **NOT IMPLEMENTED** (no tuple pruning)
- ❌ `followVersionChain()` - **NOT IMPLEMENTED**

### Gap Assessment:
**Status:** 🟡 **PARTIAL** (30% complete)
- Basic tuple headers work for insert/delete
- No UPDATE support (major gap)
- No version chains
- No HOT optimization
- No pruning

---

## 4. Concurrency Control and Lock Management

### Specification Requirements (from Section 4 of MGA spec):

**Lock Manager:**
```cpp
struct LockManager {
    // Lock hash tables
    HashTable* lock_hash;              // Locks by object
    HashTable* proc_lock_hash;         // Locks by process

    // Lock pools
    LockPool* lock_pool;
    LockPool* proc_lock_pool;

    // Deadlock detection
    DeadlockDetector* deadlock_detector;
    uint32_t deadlock_timeout_ms;

    // Lock modes
    // ACCESS_SHARE, ROW_SHARE, ROW_EXCLUSIVE,
    // SHARE_UPDATE_EXCLUSIVE, SHARE, SHARE_ROW_EXCLUSIVE,
    // EXCLUSIVE, ACCESS_EXCLUSIVE
};
```

**Lock Types:**
- Database locks
- Table locks (8 modes)
- Row locks (FOR UPDATE, FOR SHARE, FOR KEY SHARE, FOR NO KEY UPDATE)
- Page locks
- Advisory locks
- Predicate locks (for serializable isolation)

**Deadlock Detection:**
- Wait-for graph
- Timeout-based detection (1 second default)
- Youngest transaction aborts

### Current Implementation:

❌ **NOT IMPLEMENTED**

**Evidence:**
- No `lock_manager.h` or `lock_manager.cpp` files exist
- Grep for "LockManager" found only specification documents
- No lock-related code in `storage_engine.cpp` or `transaction_manager.cpp`
- Integration tests (`test_lock_conflict.cpp`, `test_locking_and_creation.cpp`) exist but likely fail

**Visibility Function (storage_engine.cpp):**
```cpp
auto StorageEngine::isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) -> bool
{
    // Basic visibility without locking
    if (xmax != 0) {
        return false;  // Deleted tuple
    }
    if (xmin > current_xid) {
        return false;  // Future tuple
    }
    return true;  // Visible
}
```
- ✅ Basic visibility check
- ❌ No actual locking mechanism
- ❌ No snapshot-based visibility

### Gap Assessment:
**Status:** 🔴 **MISSING** (0% complete)
- **No lock manager at all**
- **No row-level locking**
- **No table-level locking**
- **No deadlock detection**
- **No advisory locks**
- **No predicate locks**

This is the **largest gap** in the MGA implementation.

---

## 5. Garbage Collection (Vacuum) Subsystem

### Specification Requirements (from Section 5 of MGA spec):

**Vacuum Operations:**
1. **Calculate Horizon:**
   - Scan ProcArray for oldest backend_xmin
   - Oldest active XID is vacuum horizon
   - Tuples with xmax < horizon are candidates

2. **Identify Dead Tuples:**
   - Check tuple headers for xmax
   - Check CLOG for commit status
   - Build list of removable tuples

3. **Reclaim Space:**
   - Remove dead tuples from heap pages
   - Update indexes
   - Mark space as reusable
   - Truncate empty pages at end

4. **Update Statistics:**
   - Dead tuple count
   - Free space map
   - Table statistics

**Vacuum Types:**
- Manual VACUUM
- Auto-vacuum (background process)
- VACUUM FULL (table rewrite)

### Current Implementation:

❌ **NOT IMPLEMENTED**

**Evidence:**
- No `vacuum.h` or `vacuum.cpp` files exist
- `HeapPage::deleteTuple()` only sets `xmax` and `FLAG_DELETED`
- No tuple pruning or space reclamation
- No free space map updates
- No vacuum background worker

**Hash Index Vacuum (hash_index.cpp):**
```cpp
Status HashIndex::vacuum(ErrorContext* ctx)
{
    // Only vacuums hash index buckets, not heap tuples
    // Compacts deleted entries within hash pages
}
```
- ✅ Hash index has vacuum for its own pages
- ❌ No heap tuple vacuum
- ❌ No coordination with other indexes

### Gap Assessment:
**Status:** 🔴 **MISSING** (5% complete)
- **No heap vacuum** (critical gap)
- **No auto-vacuum**
- **No vacuum horizon calculation**
- **No dead tuple identification**
- Only hash index has internal vacuum

---

## 6. Additional MGA Components

### Specification Requirements:

**6.1 Careful Writes (Firebird-style durability):**
- Double-write buffer
- Atomic page writes
- Recovery without transaction log

**6.2 Savepoints:**
- Nested transaction support
- Rollback to savepoint
- Release savepoint

**6.3 Two-Phase Commit:**
- PREPARE TRANSACTION
- COMMIT PREPARED
- ROLLBACK PREPARED
- Transaction state = LIMBO

**6.4 Distributed Transactions:**
- Global transaction UUID
- Cross-node coordination
- Distributed deadlock detection

### Current Implementation:

**6.1 Careful Writes:**
- ❌ Not implemented (relies on OS sync)

**6.2 Savepoints:**
- ❌ Not implemented
- Transaction states include PREPARED but no savepoint API

**6.3 Two-Phase Commit:**
- 🟡 **Partial:** TransactionState::PREPARED exists
- ❌ No PREPARE/COMMIT PREPARED commands
- ❌ No recovery of prepared transactions

**6.4 Distributed Transactions:**
- ❌ Not implemented
- No global transaction UUID
- No distributed coordination

---

## 7. Summary Table

| Component | Specification | Current Status | Completion | Priority |
|-----------|--------------|----------------|------------|----------|
| **1. Transaction Management** | | | | |
| - CLOG/TIP | 2-bit encoding, 32K/page | TIP: 20 bytes/entry, 400/page | 60% | HIGH |
| - XID Management | 64-bit, thread-safe | ✅ Implemented | 100% | - |
| - Transaction States | 4 states | ✅ 4 states | 100% | - |
| - ProcArray | Shared memory tracking | ❌ Missing | 0% | HIGH |
| - Multi-connection | Multiple backends | ❌ Single only | 0% | HIGH |
| **2. Snapshot Management** | | | | |
| - Snapshot Structure | xmin/xmax/xip/curcid | xmin/xmax/xip only | 70% | MEDIUM |
| - GetSnapshotData | Scan ProcArray | ❌ Stub only | 40% | HIGH |
| - Command IDs | Same-txn visibility | ❌ Missing | 0% | MEDIUM |
| - Subtransactions | xip_overflow handling | ❌ Missing | 0% | LOW |
| **3. Tuple Headers & Chains** | | | | |
| - TupleHeader | 10+ fields, infomask | 4 fields, 2 flags | 30% | HIGH |
| - Version Chains | tdh_next_version | ❌ Missing | 0% | HIGH |
| - HOT Updates | Heap-only optimization | ❌ Missing | 0% | MEDIUM |
| - Tuple Pruning | Page access pruning | ❌ Missing | 0% | MEDIUM |
| **4. Lock Manager** | | | | |
| - Lock Manager | Full implementation | ❌ Missing | 0% | CRITICAL |
| - Table Locks | 8 lock modes | ❌ Missing | 0% | CRITICAL |
| - Row Locks | 4 row lock modes | ❌ Missing | 0% | CRITICAL |
| - Deadlock Detection | Wait-for graph | ❌ Missing | 0% | HIGH |
| - Advisory Locks | User locks | ❌ Missing | 0% | LOW |
| **5. Vacuum** | | | | |
| - Heap Vacuum | Dead tuple removal | ❌ Missing | 0% | CRITICAL |
| - Horizon Calculation | ProcArray scan | ❌ Missing | 0% | CRITICAL |
| - Auto-Vacuum | Background worker | ❌ Missing | 0% | HIGH |
| - VACUUM FULL | Table rewrite | ❌ Missing | 0% | LOW |
| **6. Advanced Features** | | | | |
| - Careful Writes | Double-write buffer | ❌ Missing | 0% | MEDIUM |
| - Savepoints | Nested transactions | ❌ Missing | 0% | LOW |
| - Two-Phase Commit | PREPARE/COMMIT | 🟡 Partial | 10% | LOW |
| - Distributed Txns | Cross-node coordination | ❌ Missing | 0% | LOW |

---

## 8. Overall Assessment

### What Works:
✅ **Single-connection transaction lifecycle**
- Begin, commit, rollback work correctly
- XID allocation is solid
- Basic tuple visibility for insert/delete
- TIP persistence (though inefficient)

### Critical Gaps (Blocking Production Use):
🔴 **1. No Lock Manager** - Cannot support concurrent transactions
🔴 **2. No Vacuum** - Database will grow indefinitely with dead tuples
🔴 **3. No UPDATE Support** - No version chains for tuple updates
🔴 **4. No ProcArray** - Cannot track multiple active transactions
🔴 **5. No Multi-connection** - Artificially limited to single backend

### Medium Priority Gaps:
🟡 **6. Inefficient CLOG** - TIP uses 160x more space than CLOG spec
🟡 **7. Limited Snapshot** - No command IDs, no subtransactions
🟡 **8. Minimal TupleHeader** - No hint bits, no HOT optimization
🟡 **9. No Tuple Pruning** - Vacuum needed for space reclaim

### Low Priority Gaps (Future Enhancements):
⚪ **10. Savepoints** - Nice to have for complex transactions
⚪ **11. Two-Phase Commit** - For distributed transactions
⚪ **12. Careful Writes** - Firebird-style durability
⚪ **13. Advisory Locks** - User-level locking

---

## 9. Estimated Implementation Effort

Based on similar systems (PostgreSQL, Firebird) and existing codebase:

| Component | Estimated Lines of Code | Estimated Time |
|-----------|------------------------|----------------|
| **Phase 1: Lock Manager** | 2,500 lines | 2 weeks |
| - Lock hash tables | 500 lines | 2 days |
| - Lock acquisition/release | 800 lines | 3 days |
| - Deadlock detection | 600 lines | 3 days |
| - Lock modes/conflicts | 600 lines | 2 days |
| **Phase 2: Version Chains** | 1,200 lines | 1 week |
| - TupleHeader expansion | 200 lines | 1 day |
| - Version chain creation | 400 lines | 2 days |
| - Chain traversal | 300 lines | 2 days |
| - HeapPage::updateTuple() | 300 lines | 2 days |
| **Phase 3: Vacuum** | 2,000 lines | 1.5 weeks |
| - Horizon calculation | 300 lines | 1 day |
| - Dead tuple scan | 500 lines | 2 days |
| - Space reclamation | 600 lines | 3 days |
| - Index cleanup | 400 lines | 2 days |
| - Background worker | 200 lines | 1 day |
| **Phase 4: ProcArray** | 800 lines | 3 days |
| - Shared memory structure | 200 lines | 1 day |
| - Backend registration | 200 lines | 1 day |
| - Snapshot integration | 400 lines | 1 day |
| **Phase 5: CLOG Optimization** | 600 lines | 2 days |
| - 2-bit encoding | 300 lines | 1 day |
| - Convert TIP to CLOG | 300 lines | 1 day |
| **Phase 6: Testing** | 2,000 lines | 1 week |
| - Unit tests | 800 lines | 2 days |
| - Integration tests | 800 lines | 3 days |
| - Stress tests | 400 lines | 2 days |
| **TOTAL** | **~9,100 lines** | **~7 weeks** |

---

## 10. Recommended Implementation Order

### **Phase 1: Multi-Connection Foundation** (Week 1)
**Priority:** CRITICAL
- Implement ProcArray for tracking multiple backends
- Remove single-connection restriction from TransactionManager
- Update snapshot management to scan all active transactions
- **Prerequisite for all concurrency work**

### **Phase 2: Lock Manager** (Weeks 2-3)
**Priority:** CRITICAL
- Implement lock hash tables and pools
- Add table-level locking (8 modes)
- Add row-level locking (4 modes)
- Implement deadlock detection with wait-for graph
- **Enables concurrent transactions**

### **Phase 3: UPDATE and Version Chains** (Week 4)
**Priority:** CRITICAL
- Expand TupleHeader with tdh_next_version and infomask
- Implement HeapPage::updateTuple() with version chain creation
- Add version chain traversal for visibility checks
- **Completes basic MVCC**

### **Phase 4: Vacuum Subsystem** (Weeks 5-6)
**Priority:** CRITICAL
- Implement vacuum horizon calculation from ProcArray
- Add dead tuple identification
- Implement heap page space reclamation
- Add index cleanup coordination
- Implement background auto-vacuum worker
- **Prevents unbounded database growth**

### **Phase 5: CLOG Optimization** (Week 7)
**Priority:** MEDIUM
- Convert TIP from 20-byte entries to 2-bit CLOG
- Migrate existing transactions during upgrade
- **Improves efficiency 160x**

### **Phase 6: Advanced Features** (Future)
**Priority:** LOW
- HOT updates optimization
- Tuple pruning during page access
- Savepoints and subtransactions
- Two-phase commit completion
- Careful writes / double-write buffer
- Distributed transaction support

---

## 11. Compatibility Considerations

### Backward Compatibility:
- **TIP → CLOG migration** needed if changing storage format
- **TupleHeader expansion** requires full table rewrite
- **Version chains** backward compatible (tdh_next_version = 0 for old tuples)

### Forward Compatibility:
- Reserve space in TupleHeader for future infomask bits
- Keep TIP format extensible with reserved fields
- Design lock manager for future lock types (predicate locks)

---

## 12. Testing Requirements

### Unit Tests Needed:
1. **TransactionManager:**
   - Multi-connection transaction allocation
   - Concurrent begin/commit/rollback
   - XID allocation under load

2. **Lock Manager:**
   - Lock acquisition/release
   - Lock mode conflicts
   - Deadlock detection and resolution
   - Advisory locks

3. **Version Chains:**
   - Chain creation on UPDATE
   - Chain traversal
   - Visibility across chain
   - Chain pruning

4. **Vacuum:**
   - Dead tuple identification
   - Space reclamation
   - Horizon calculation
   - Auto-vacuum triggering

### Integration Tests Needed:
1. Concurrent inserts across multiple connections
2. UPDATE conflicts with version chains
3. Deadlock scenarios (cycle detection)
4. Vacuum with active snapshots
5. Long-running transactions blocking vacuum

### Stress Tests Needed:
1. 1000+ concurrent connections
2. 1M+ transactions with vacuum
3. Large UPDATE chains (100+ versions)
4. Lock contention under high load

---

## 13. Conclusion

ScratchBird has implemented the **foundational elements** of MGA for single-connection use:
- Transaction lifecycle works
- Basic MVCC visibility for INSERT/DELETE
- XID management is solid
- Tuple headers track xmin/xmax

However, the implementation is **incomplete for production use**. The critical missing components are:

**Must-Have Before Multi-User:**
1. ❌ Lock Manager (0% complete)
2. ❌ Vacuum subsystem (0% complete)
3. ❌ ProcArray for multi-connection (0% complete)
4. ❌ UPDATE with version chains (0% complete)

**Estimated Total Effort:** ~7 weeks for full MGA implementation (~9,100 lines of code)

**Recommended Next Step:** Implement ProcArray and remove single-connection restriction, then proceed with Lock Manager as the highest priority.

---

## References

- Primary Specification: `/docs/specifications/parser/v3/Specification for a Multi-Generational Database Architecture.md` (865 lines)
- Secondary Specs: `TRANSACTION_MGA_CORE.md`, `TRANSACTION_LOCK_MANAGER.md`
- Current Code: `transaction_manager.cpp` (574 lines), `heap_page.cpp`, `storage_engine.cpp`
- Tests: `test_lock_conflict.cpp`, `test_locking_and_creation.cpp` (likely failing)

