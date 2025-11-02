# MGA Compliance Audit Report

**Date**: November 1, 2025
**Auditor**: Comprehensive Code Audit
**Scope**: Firebird MGA vs PostgreSQL MVCC compliance
**Status**: PARTIALLY COMPLIANT (80%)

---

## Executive Summary

**Finding**: ScratchBird implements **hybrid architecture** - correct Firebird MGA back-versioning for UPDATE operations, but includes PostgreSQL MVCC snapshot structures for visibility checking.

**Compliance**: 80% (4 of 5 core requirements met)

**Critical Issue**: Snapshot structures with `active_xids[]` arrays violate MGA_RULES.md Rule 1

---

## MGA Requirements (from specifications)

Based on comprehensive review of:
- `/MGA_RULES.md` (15 absolute rules)
- `/docs/specifications/MGA_IMPLEMENTATION.md`
- `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `/docs/specifications/TRANSACTION_MGA_CORE.md`

### TRUE Firebird MGA Requires:

1. ✅ **TIP (Transaction Inventory Pages)** - 2-bit transaction state storage
2. ✅ **Back-Versioning** - UPDATE creates back version (old data) + in-place primary (new data)
3. ✅ **Stable TIDs** - TIDs never change on UPDATE
4. ✅ **Transaction Markers** - OIT/OAT/OST for garbage collection
5. ❌ **Pure TIP Visibility** - NO snapshot structures with active_xids arrays

---

## Audit Results

### ✅ COMPLIANT: TIP Implementation

**Files Audited**:
- `src/core/transaction_manager.cpp`
- `include/scratchbird/core/transaction_manager.h`

**Evidence of Compliance**:

1. **TIP Structure Exists**:
   - `src/core/transaction_manager.cpp:52-54` - TIP page usage
   - `src/core/transaction_manager.cpp:80` - `TransactionState` enum (ACTIVE/COMMITTED/ABORTED/PREPARED)

2. **TIP Write Operations**:
   - `src/core/transaction_manager.cpp:127-169` - `writeTipEntry()` writes transaction state to TIP

3. **TIP Read Operations**:
   - `src/core/transaction_manager.cpp:549-598` - `getTransactionState()` looks up TIP

**Verdict**: ✅ **COMPLIANT** - TIP properly implemented

---

### ✅ COMPLIANT: Back-Versioning for UPDATE

**Files Audited**:
- `src/core/storage_engine.cpp`
- `src/core/heap_page.cpp`

**Evidence of Compliance**:

#### Cross-Page UPDATE Implementation

**File**: `src/core/storage_engine.cpp:880-1035`

**Algorithm**:
```
Line 893:  "Step 1: Get OLD tuple data (preserve in back version)"
Line 931:  "Step 2: Allocate page for BACK version (OLD data)"
Line 978:  "Step 3: Re-pin PRIMARY page and overwrite IN-PLACE"
Line 1018: "Step 4: Return ORIGINAL TID (STABLE!)"
Line 1029: "Step 5: NO INDEX UPDATES NEEDED!"
```

**Key Code**:
```cpp
// src/core/storage_engine.cpp:931-960
// Step 2: Allocate page for BACK version (OLD data)
TID back_tid;
status = allocateTuple(back_data_size, &back_tid.page_id, &back_tid.item_id, ctx);

// src/core/storage_engine.cpp:978-1000
// Step 3: Re-pin PRIMARY page and overwrite IN-PLACE
status = HeapPage::overwriteTuple(
    primary_page_data, page_size_, item_id,
    new_data, new_data_size, back_tid, ctx);

// src/core/storage_engine.cpp:1018-1028
// Step 4: Return ORIGINAL TID (STABLE!)
if (new_page_id_out != nullptr) {
    *new_page_id_out = page_id;  // SAME page!
}
if (new_item_id_out != nullptr) {
    *new_item_id_out = item_id;  // SAME item!
}
```

#### Heap Page UPDATE Implementation

**File**: `src/core/heap_page.cpp:562-585`

**Header Comments**:
```cpp
// FIREBIRD MGA BACK VERSIONING ALGORITHM
// 1. Item pointer location NEVER changes (stable TID)
// 2. Back versions are created FIRST (preserve old state)
// 3. Primary location is overwritten IN-PLACE (new tuple)
// 4. Version chain points BACKWARD (Newest-to-Oldest)
// 5. Indexes NEVER need updating (unless indexed columns change)
```

**Key Code**:
```cpp
// src/core/heap_page.cpp:624-633
// Update item pointer to point to BACK version
item_ptr->offset = back_tid.item_id;
item_ptr->flags |= ITEM_HAS_BACK_VERSION;

// src/core/heap_page.cpp:642-663
// Write new tuple data IN-PLACE at PRIMARY location
tuple_header->xmin = current_xid;
tuple_header->back_page = back_tid.page_id;
tuple_header->back_item = back_tid.item_id;
std::memcpy(tuple_data, new_data, new_data_size);
```

**Verdict**: ✅ **COMPLIANT** - UPDATE creates back version (OLD data) and modifies primary in-place (NEW data)

---

### ✅ COMPLIANT: Stable TIDs

**Files Audited**:
- `src/core/storage_engine.cpp`

**Evidence of Compliance**:

**File**: `src/core/storage_engine.cpp:1018-1033`

```cpp
// Step 4: Return ORIGINAL TID (STABLE!)
// This is the key benefit: TID never changes, indexes remain valid!
if (new_page_id_out != nullptr)
{
    *new_page_id_out = page_id;  // SAME page!
}
if (new_item_id_out != nullptr)
{
    *new_item_id_out = item_id;  // SAME item!
}

// Step 5: NO INDEX UPDATES NEEDED!
// Because TID is stable, indexes do NOT need to be updated.
// They continue pointing to the same (page, item) which now has the new data.
```

**Index Impact**:
- TID remains `(page_id, item_id)` after UPDATE
- Indexes continue pointing to same location
- No index writes required (unless indexed column modified)
- Eliminates PostgreSQL-style index bloat

**Verdict**: ✅ **COMPLIANT** - TIDs are stable, indexes not updated

---

### ✅ COMPLIANT: Transaction Markers (OIT/OAT/OST)

**Files Audited**:
- `include/scratchbird/core/transaction_manager.h`
- `src/core/transaction_manager.cpp`

**Evidence of Compliance**:

#### Marker Getters

**File**: `include/scratchbird/core/transaction_manager.h:169-199`

```cpp
/**
 * Get the oldest interesting transaction (OIT).
 * This is the oldest transaction that is not yet garbage collected.
 */
auto getOldestXid() const -> uint64_t;

/**
 * Get the oldest active transaction (OAT).
 */
auto getOldestActiveXid() const -> uint64_t;

/**
 * Get the oldest snapshot transaction (OST).
 */
auto getOldestSnapshot() const -> uint64_t;
```

#### Marker Updates

**File**: `src/core/transaction_manager.cpp:693-790`

**Function**: `updateTransactionMarkers()`

**Algorithm**:
1. Scan TIP pages to find first non-committed transaction (OIT)
2. Find oldest active transaction from active set (OAT)
3. Find oldest snapshot transaction from snapshot list (OST)
4. Update header page atomically

**Key Code**:
```cpp
// src/core/transaction_manager.cpp:715-752
// Calculate OIT by scanning TIP
for (uint64_t xid = current_oit; xid < next_xid; xid++) {
    TransactionState state = getTransactionState(xid);
    if (state != TransactionState::COMMITTED) {
        new_oit = xid;
        break;
    }
}

// src/core/transaction_manager.cpp:754-767
// Calculate OAT from active transactions
uint64_t new_oat = next_xid;
for (const auto& txn : active_transactions_) {
    if (txn.second < new_oat) {
        new_oat = txn.second;
    }
}

// src/core/transaction_manager.cpp:769-782
// Calculate OST from snapshot transactions
uint64_t new_ost = next_xid;
for (const auto& snapshot : active_snapshots_) {
    if (snapshot.xmin < new_ost) {
        new_ost = snapshot.xmin;
    }
}
```

**Verdict**: ✅ **COMPLIANT** - All transaction markers (OIT/OAT/OST) properly tracked

---

### ❌ VIOLATION: PostgreSQL Snapshot Structure

**Files Audited**:
- `include/scratchbird/core/transaction_manager.h`
- `src/core/transaction_manager.cpp`

**Evidence of Violation**:

#### Violation 1: Snapshot Structure Definition

**File**: `include/scratchbird/core/transaction_manager.h:228-250`

```cpp
/**
 * Transaction snapshot for MVCC visibility checks.
 */
struct Snapshot
{
    uint64_t xmin;                     // Oldest active XID at snapshot time
    uint64_t xmax;                     // Next XID to be assigned at snapshot time
    std::vector<uint64_t> active_xids; // Active XIDs at snapshot time
    uint64_t snapshot_xid;             // XID that took this snapshot
    std::chrono::system_clock::time_point timestamp; // When snapshot was taken
};
```

**MGA_RULES.md Rule 1 States**:
> ❌ FORBIDDEN (PostgreSQL MVCC):
> ```cpp
> struct Snapshot {
>     TransactionId xmin;
>     TransactionId xmax;
>     TransactionId *active_xids;
>     uint32_t xcnt;
> };
> ```
>
> **If you see `Snapshot` anywhere in transaction-related code, it's WRONG.**

**Severity**: CRITICAL
**Reason**: Violates fundamental MGA architecture principle

#### Violation 2: Snapshot-Based Visibility

**File**: `src/core/transaction_manager.cpp:856-934`

**Function**: `isSnapshotVisible()`

```cpp
auto TransactionManager::isSnapshotVisible(uint64_t xid, const Snapshot *snapshot) -> bool
{
    // Quick checks
    if (snapshot == nullptr)
        return true;
    if (xid >= snapshot->xmax)
        return false; // Transaction started after snapshot

    // Check if transaction was active at snapshot time
    if (std::binary_search(snapshot->active_xids.begin(),
                           snapshot->active_xids.end(), xid))
    {
        return false;  // Transaction was in-progress at snapshot time
    }

    // Transaction committed before snapshot
    return true;
}
```

**MGA_RULES.md Rule 3 States**:
> ✅ CORRECT (Firebird MGA):
> ```cpp
> bool is_visible(TransactionId version_xid, TransactionId reader_xid) {
>     // Look up transaction state in TIP
>     TxState state = get_transaction_state(version_xid);
>     if (state == TX_COMMITTED && version_xid < reader_xid) {
>         return true;
>     }
>     return false;
> }
> ```

**Severity**: CRITICAL
**Reason**: Uses PostgreSQL snapshot array lookups instead of TIP lookups

#### Violation 3: Snapshot Creation

**File**: `src/core/transaction_manager.cpp:944-1038`

**Function**: `getSnapshot()`

```cpp
auto TransactionManager::getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(transaction_mutex_);

    snapshot_out.xmax = next_xid_.load(std::memory_order_acquire);
    snapshot_out.active_xids.clear();

    // Collect all active transaction IDs
    for (const auto& [xid, state] : active_transactions_)
    {
        snapshot_out.active_xids.push_back(xid);
    }

    // Sort for binary search
    std::sort(snapshot_out.active_xids.begin(), snapshot_out.active_xids.end());

    snapshot_out.snapshot_xid = snapshot_out.xmax;
    snapshot_out.timestamp = std::chrono::system_clock::now();

    return Status::OK;
}
```

**Severity**: CRITICAL
**Reason**: Creates PostgreSQL-style snapshot with active_xids array

**Verdict**: ❌ **NON-COMPLIANT** - Snapshot structures violate Firebird MGA

---

## Summary of Violations

### CRITICAL Violations

| # | Issue | File | Lines | Severity |
|---|-------|------|-------|----------|
| 1 | Snapshot structure definition | `transaction_manager.h` | 228-250 | CRITICAL |
| 2 | Snapshot-based visibility | `transaction_manager.cpp` | 856-934 | CRITICAL |
| 3 | Snapshot creation API | `transaction_manager.cpp` | 944-1038 | CRITICAL |
| 4 | Snapshot API in header | `transaction_manager.h` | 258-262 | HIGH |

---

## Impact Analysis

### What Works Correctly (✅)

1. **UPDATE Operations**: Back-versioning algorithm is correct
2. **TID Stability**: TIDs never change, indexes remain valid
3. **TIP Storage**: Transaction state properly stored in TIP
4. **Transaction Markers**: OIT/OAT/OST properly tracked
5. **Performance**: Update-heavy workloads will perform well (stable TIDs)

### What's Architecturally Impure (❌)

1. **Visibility Checking**: Uses PostgreSQL snapshot arrays instead of pure TIP lookups
2. **API Contract**: Functions accept `Snapshot*` instead of `TransactionId reader_xid`
3. **Memory Overhead**: Snapshot structures consume memory for active_xids arrays
4. **Complexity**: Mixing two architectures instead of pure Firebird MGA

### Why This Matters

According to MGA_RULES.md:
> "PostgreSQL MVCC: 'Is this XID in the snapshot's active transaction array?'
> Firebird MGA: 'Is this XID committed and older than me?' (TIP lookup)
>
> ONE uses snapshots, ONE uses TIP. ScratchBird uses TIP (Firebird MGA)."

**Current Status**: ScratchBird uses BOTH (hybrid), which violates architectural purity.

---

## Recommended Corrections

### Priority 1: Remove Snapshot Structures (CRITICAL)

**Effort**: 20-30 hours

**Files to Modify**:
1. `include/scratchbird/core/transaction_manager.h`
   - Remove `Snapshot` struct (lines 228-250)
   - Remove `getSnapshot()` method (line 254)
   - Remove `isSnapshotVisible()` method (line 258)
   - Add `isVersionVisible(uint64_t version_xid, uint64_t reader_xid)` method

2. `src/core/transaction_manager.cpp`
   - Remove `getSnapshot()` implementation (lines 944-1038)
   - Remove `isSnapshotVisible()` implementation (lines 856-934)
   - Add `isVersionVisible()` implementation using TIP lookups

**Example Replacement**:
```cpp
// NEW: Pure Firebird MGA visibility
auto TransactionManager::isVersionVisible(uint64_t version_xid,
                                         uint64_t reader_xid) -> bool
{
    // Own changes always visible
    if (version_xid == reader_xid) {
        return true;
    }

    // Look up transaction state in TIP (NOT snapshot!)
    TransactionState state = getTransactionState(version_xid);

    // Only committed transactions older than reader are visible
    if (state == TransactionState::COMMITTED && version_xid < reader_xid) {
        return true;
    }

    // Active or aborted = not visible
    return false;
}
```

### Priority 2: Update All Callers (HIGH)

**Effort**: 30-40 hours

**Files to Audit and Update**:
- All index implementations (B-tree, Hash, Bitmap, GIN, BRIN, HNSW, R-tree)
- Heap page scanning
- Query executor visibility checks
- Any code calling `isSnapshotVisible()` or `getSnapshot()`

**Search for**:
```bash
grep -r "isSnapshotVisible" src/ include/
grep -r "getSnapshot" src/ include/
grep -r "Snapshot\*" src/ include/
```

**Replace**:
- `isSnapshotVisible(xid, snapshot)` → `isVersionVisible(version_xid, current_xid)`
- `Snapshot* snapshot` parameters → `TransactionId current_xid` parameters

---

## Compliance Scorecard

| Component | Status | Score |
|-----------|--------|-------|
| TIP Implementation | ✅ Compliant | 100% |
| Back-Versioning | ✅ Compliant | 100% |
| Stable TIDs | ✅ Compliant | 100% |
| Transaction Markers | ✅ Compliant | 100% |
| Visibility API | ❌ Non-Compliant | 0% |
| **Overall** | ⚠️ **Partial** | **80%** |

---

## Conclusion

**Finding**: ScratchBird implements a **hybrid architecture**:
- ✅ **Correct**: Firebird MGA back-versioning for UPDATE operations
- ❌ **Incorrect**: PostgreSQL MVCC snapshot structures for visibility

**Recommendation**: Remove Snapshot structures to achieve 100% Firebird MGA compliance per MGA_RULES.md.

**Estimated Effort**: 50-70 hours to achieve pure Firebird MGA.

**Risk**: MEDIUM - Update logic works correctly, only visibility API needs change.

---

## Next Audit Steps

1. ✅ TransactionManager MGA audit - **COMPLETE**
2. [ ] Heap storage MGA audit - Verify back version storage
3. [ ] Index MGA audit - Check all 7 index types for snapshot usage
4. [ ] TOAST MGA audit - Verify large object versioning

**Report Date**: November 1, 2025
**Audit Status**: Phase 1.1 Complete
**Next Phase**: Heap storage and index MGA verification
