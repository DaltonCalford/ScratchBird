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
3. ✅ Index MGA audit - **COMPLETE** - See below
4. [ ] TOAST MGA audit - Verify large object versioning

**Report Date**: November 1, 2025
**Audit Status**: Phase 1.1 and 1.2 Complete
**Next Phase**: Heap storage and TOAST MGA verification

---

# INDEX MGA COMPLIANCE AUDIT

**Audit Date**: November 1, 2025
**Scope**: All 7 Index Types (B-tree, Hash, Bitmap, GIN, BRIN, HNSW, R-tree)
**Status**: **CRITICAL NON-COMPLIANCE ACROSS ALL INDEXES**

---

## EXECUTIVE SUMMARY

**CRITICAL FINDING**: ALL 7 index types in ScratchBird are **NON-COMPLIANT** with Firebird MGA architecture. The codebase systematically implements **PostgreSQL MVCC** patterns instead of Firebird MGA.

### Compliance Status Overview

| Index Type | Status | Severity | Violations |
|------------|--------|----------|------------|
| **B-tree** | ❌ NON-COMPLIANT | **CRITICAL** | 5 major violations |
| **Hash** | ❌ NON-COMPLIANT | **CRITICAL** | 4 major violations |
| **Bitmap** | ❌ NON-COMPLIANT | **CRITICAL** | 4 major violations |
| **GIN** | ❌ NON-COMPLIANT | **CRITICAL** | 5 major violations |
| **BRIN** | ⚠️ PARTIALLY COMPLIANT | **HIGH** | 2 major violations |
| **HNSW** | ⚠️ PARTIALLY COMPLIANT | **HIGH** | 2 major violations |
| **R-tree** | ⚠️ PARTIALLY COMPLIANT | **HIGH** | 2 major violations |

---

## DETAILED FINDINGS BY INDEX TYPE

---

## 1. B-TREE INDEX

**Status**: ❌ **NON-COMPLIANT**
**Severity**: **CRITICAL**

### Evidence Files
- `include/scratchbird/core/btree.h`
- `src/core/btree.cpp`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/btree.h
// Lines: 183, 214-219

Status search(const std::vector<uint8_t> &key,
              struct Snapshot *snapshot,  // ❌ WRONG - PostgreSQL MVCC
              std::vector<TID> *tids_out,
              ErrorContext *ctx = nullptr);

std::unique_ptr<BTreeIterator>
rangeScan(const std::vector<uint8_t> *start_key,
          const std::vector<uint8_t> *end_key,
          struct Snapshot *snapshot,  // ❌ WRONG - PostgreSQL MVCC
          bool start_inclusive = true, bool end_inclusive = false,
          ErrorContext *ctx = nullptr);
```

**What Should Be Used**:
```cpp
// ✅ CORRECT - Firebird MGA
Status search(const std::vector<uint8_t> &key,
              TransactionId current_xid,  // Current transaction ID
              std::vector<TID> *tids_out,
              ErrorContext *ctx = nullptr);
```

---

### VIOLATION 2: Calls isSnapshotVisible() (Rule 3)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: src/core/btree.cpp
// Lines: 1103, 1125-1137

bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, struct Snapshot *snapshot) const
{
    // ...
    auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);
    if (!txn_mgr->isSnapshotVisible(xmin, txn_snapshot))  // ❌ WRONG
    {
        return false;
    }

    if (xmax != 0)
    {
        if (txn_mgr->isSnapshotVisible(xmax, txn_snapshot))  // ❌ WRONG
        {
            return false;
        }
    }
    return true;
}
```

**What Should Be Used**:
```cpp
// ✅ CORRECT - Firebird MGA
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, TransactionId reader_xid) const
{
    // Own changes always visible
    if (xmin == reader_xid) {
        return true;
    }

    // Look up transaction state in TIP
    TransactionState state;
    txn_mgr->getTransactionState(xmin, state, nullptr);

    // Only committed transactions older than reader are visible
    if (state == TransactionState::COMMITTED && xmin < reader_xid) {
        return true;
    }

    return false;
}
```

---

### VIOLATION 3: No TIP Lookups (Rule 2)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- B-tree NEVER calls `getTransactionState()` for TIP lookups
- Instead relies on PostgreSQL-style snapshot active transaction array
- Grep search confirmed: 0 occurrences of `getTransactionState` in `src/core/btree.cpp`

**Impact**:
- No 2-bit per transaction state tracking
- No efficient TIP-based visibility checks
- Violates core Firebird MGA architecture

---

### VIOLATION 4: xmin/xmax as Snapshot Markers (Rule 1)
**Severity**: HIGH
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/btree.h
// Lines: 122-123

// Multi-version support
uint64_t btn_xmin; // Node creation transaction
uint64_t btn_xmax; // Node deletion transaction
```

**Analysis**:
- While xmin/xmax fields exist (which is correct for MGA), they are being used in conjunction with PostgreSQL snapshot visibility checking
- The fields themselves are correct, but the visibility logic using them is wrong

---

### VIOLATION 5: Comments Reference "MVCC" Instead of "MGA"
**Severity**: MEDIUM
**MGA Rule Violated**: Rule 0 - The Fundamental Distinction

**Evidence**:
```cpp
// File: include/scratchbird/core/btree.h
// Lines: 178-180

// PHASE 1 TASK 1.1.1: Added Snapshot parameter for MVCC visibility filtering
// For Firebird MGA: Snapshot is used to filter returned TIDs via heap visibility checks
// Pass nullptr to return ALL matching TIDs (used by VACUUM/internal operations)
```

**Analysis**:
- Documentation conflates MVCC with MGA
- Comments acknowledge "Firebird MGA" but implement PostgreSQL MVCC

---

## 2. HASH INDEX

**Status**: ❌ **NON-COMPLIANT**
**Severity**: **CRITICAL**

### Evidence Files
- `include/scratchbird/core/hash_index.h`
- `src/core/hash_index.cpp`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/hash_index.h
// Lines: 114-120

// PHASE 1 TASK 1.1.2: Added Snapshot parameter for MVCC visibility filtering
// Returns a vector of TIDs (may be empty if key not found)
// Pass nullptr for snapshot to return ALL matching TIDs (used by VACUUM)
std::vector<TID> find(const void *key_data, size_t key_len,
                      struct Snapshot *snapshot,  // ❌ WRONG
                      ErrorContext *ctx = nullptr);
```

---

### VIOLATION 2: No Visibility Checking at Index Level (Rule 3)
**Severity**: HIGH
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: src/core/hash_index.cpp
// Lines: 720-723

// MVCC filtering: For hash indexes in Firebird MGA, visibility filtering
// is done at the storage layer when fetching tuples
(void)snapshot;  // ❌ Snapshot parameter ignored!
```

**Analysis**:
- Hash index completely ignores snapshot parameter
- Defers all visibility checking to heap layer
- No TIP lookups at index level
- This violates MGA principle of early filtering

---

### VIOLATION 3: No TIP Lookups (Rule 2)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- Hash index NEVER calls `getTransactionState()` for TIP lookups
- Grep search confirmed: 0 occurrences in `src/core/hash_index.cpp`

---

### VIOLATION 4: Stores Legacy TID Format Without xmin/xmax (Rule 6)
**Severity**: HIGH
**MGA Rule Violated**: Rule 6 - In-Place Updates with Stable TIDs

**Evidence**:
```cpp
// File: include/scratchbird/core/hash_index.h
// Lines: 58-63

struct HashEntry
{
    uint64_t he_key_hash;  // Full 64-bit hash of the key
    uint64_t he_tuple_id;  // TupleId (page_id << 32 | item_id)
                          // Special value: 0 means deleted entry
    // ❌ NO xmin/xmax fields!
} __attribute__((packed));
```

**Impact**:
- Hash index cannot perform visibility checks
- No transaction tracking at index level
- Completely relies on heap for MVCC

---

## 3. BITMAP INDEX

**Status**: ❌ **NON-COMPLIANT**
**Severity**: **CRITICAL**

### Evidence Files
- `include/scratchbird/core/bitmap_index.h`
- `src/core/bitmap_index.cpp`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/bitmap_index.h
// Lines: 160-166

// PHASE 1 TASK 1.1.4: Added Snapshot parameter for MVCC visibility filtering
std::vector<TID> find(
    const void *value_data,
    size_t value_len,
    struct Snapshot *snapshot,  // ❌ WRONG
    ErrorContext *ctx = nullptr);
```

---

### VIOLATION 2: Calls isSnapshotVisible() (Rule 3)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: src/core/bitmap_index.cpp
// Lines: 474-479

// Cast snapshot pointer to actual TransactionManager::Snapshot type
auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);

bool xmin_visible = txn_manager->isSnapshotVisible(tuple_header->xmin, txn_snapshot);  // ❌ WRONG
bool xmax_visible = (tuple_header->xmax != 0) &&
                    txn_manager->isSnapshotVisible(tuple_header->xmax, txn_snapshot);  // ❌ WRONG
```

---

### VIOLATION 3: Post-Filtering Instead of Index-Level Visibility (Rule 3)
**Severity**: HIGH
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: src/core/bitmap_index.cpp
// Lines: 412-418

// PHASE 1 TASK 1.5: Visibility filter for bitmap index (post-filtering)
// This is a post-filter that checks heap tuple visibility for each TID returned by bitmap operations
// NOTE: This is less efficient than B-Tree/Hash visibility (20-40% overhead) because:
//       - Bitmap returns TIDs directly, not pointers to heap tuples
//       - We must access heap pages separately to check visibility
//       - Full MVCC redesign would require storing xmin/xmax in bitmap entries (deferred to Beta)
```

**Analysis**:
- Bitmap index performs visibility checks AFTER returning TIDs
- Requires heap page access for each TID (20-40% overhead)
- Should filter at index level using TIP lookups

---

### VIOLATION 4: No TIP Lookups (Rule 2)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- Bitmap index NEVER calls `getTransactionState()` for TIP lookups
- Instead accesses heap pages and calls `isSnapshotVisible()`
- Grep search confirmed: 0 occurrences in `src/core/bitmap_index.cpp`

---

## 4. GIN INDEX

**Status**: ❌ **NON-COMPLIANT**
**Severity**: **CRITICAL**

### Evidence Files
- `include/scratchbird/core/gin_index.h`
- `src/core/gin_index.cpp`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/gin_index.h
// Lines: 247-251

// PHASE 1 TASK 1.1.3: Added Snapshot parameter for MVCC visibility filtering
std::vector<TID> find(const void *key_data, size_t key_len,
                      struct Snapshot *snapshot,  // ❌ WRONG
                      ErrorContext *ctx = nullptr);
```

---

### VIOLATION 2: Calls isSnapshotVisible() (Rule 3)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: src/core/gin_index.cpp
// Lines: 2217, 2303-2305

return db_->transaction_manager()->isSnapshotVisible(xmin,
    reinterpret_cast<const TransactionManager::Snapshot *>(snapshot));  // ❌ WRONG

// And later:
bool xmin_visible = txn_manager->isSnapshotVisible(tuple_header->xmin, txn_snapshot);  // ❌ WRONG
bool xmax_visible = (tuple_header->xmax != 0) &&
                    txn_manager->isSnapshotVisible(tuple_header->xmax, txn_snapshot);  // ❌ WRONG
```

---

### VIOLATION 3: Mixed Use of TIP and Snapshots (Rule 3)
**Severity**: CRITICAL
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: src/core/gin_index.cpp
// Lines: 2211

Status status = db_->transaction_manager()->getTransactionState(xmin, state, ctx);
```

**Analysis**:
- GIN index calls BOTH `getTransactionState()` AND `isSnapshotVisible()`
- This is architectural confusion - mixing Firebird MGA with PostgreSQL MVCC
- Should use ONLY TIP-based visibility checks

---

### VIOLATION 4: Pending List Has xmin But Uses Snapshots (Rule 3)
**Severity**: HIGH
**MGA Rule Violated**: Rule 3 - Visibility Check Uses TIP, Not Snapshots

**Evidence**:
```cpp
// File: include/scratchbird/core/gin_index.h
// Lines: 50-56

struct GinPendingEntry
{
    uint64_t tid;         // TupleId (page_id << 32 | item_id)
    uint64_t xmin;        // Transaction ID that inserted this entry (for MVCC)  // ✅ Correct field
    uint16_t key_len;     // Key length in bytes
    uint8_t key_data[54]; // Key data (inline for small keys)
} __attribute__((packed));
```

**Analysis**:
- Pending entries correctly store xmin
- But visibility checks use `isSnapshotVisible()` instead of `getTransactionState()`

---

### VIOLATION 5: No TIP Lookups for Most Operations (Rule 2)
**Severity**: HIGH
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- GIN index calls `getTransactionState()` in only 1 location (line 2211)
- All other visibility checks use `isSnapshotVisible()`
- Inconsistent architecture

---

## 5. BRIN INDEX

**Status**: ⚠️ **PARTIALLY COMPLIANT**
**Severity**: **HIGH**

### Evidence Files
- `include/scratchbird/core/brin_index.h`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: HIGH
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/brin_index.h
// Lines: 239-243

Status scan(const std::vector<uint8_t> *min_value,
            const std::vector<uint8_t> *max_value,
            struct Snapshot *snapshot,  // ❌ WRONG
            std::vector<uint32_t> *block_numbers_out,
            ErrorContext *ctx = nullptr);
```

---

### VIOLATION 2: No Implementation Found (Rule 2)
**Severity**: MEDIUM
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- BRIN implementation file `src/core/brin_index.cpp` does not exist
- Cannot verify TIP usage or snapshot usage in actual code
- Header declares snapshot parameters but no implementation to audit

---

### POSITIVE FINDINGS:

✅ **COMPLIANT:** BRIN header shows xmin/xmax tracking at range level:
```cpp
// File: include/scratchbird/core/brin_index.h
// Lines: 150-152

// MGA compliance (Phase 4A.1)
uint64_t brn_xmin; // Transaction that created this range
uint64_t brn_xmax; // Transaction that deleted this range (0 if active)
```

✅ **COMPLIANT:** Documentation explicitly mentions Firebird MGA:
```cpp
// Lines: 58-64
## MGA Compliance (Phase 4A.1 - October 2025)

- **xmin/xmax tracking**: Each range summary has xmin/xmax
- **Snapshot isolation**: Snapshot parameter in scan API
- **Visibility filtering**: Via heap layer (Firebird MGA)
- **Garbage collection**: removeDeadEntries() for dead ranges
- **Stable TIDs**: Ranges reference stable block numbers
```

---

## 6. HNSW INDEX

**Status**: ⚠️ **PARTIALLY COMPLIANT**
**Severity**: **HIGH**

### Evidence Files
- `include/scratchbird/core/hnsw_index.h`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: HIGH
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/hnsw_index.h
// Lines: 284-288

Status search(const VectorValue &query_vector,
              uint32_t k,
              struct Snapshot *snapshot,  // ❌ WRONG
              std::vector<HnswSearchResult> *results_out,
              ErrorContext *ctx = nullptr);
```

---

### VIOLATION 2: No Implementation Found (Rule 2)
**Severity**: MEDIUM
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- HNSW implementation file `src/core/hnsw_index.cpp` does not exist
- Cannot verify TIP usage or snapshot usage in actual code
- Header declares snapshot parameters but no implementation to audit

---

### POSITIVE FINDINGS:

✅ **COMPLIANT:** HNSW header shows xmin/xmax tracking at node level:
```cpp
// File: include/scratchbird/core/hnsw_index.h
// Lines: 147-149

// MGA compliance (Phase 4A.2)
uint64_t node_xmin; // Transaction that created this node
uint64_t node_xmax; // Transaction that deleted this node (0 if active)
```

✅ **COMPLIANT:** Documentation explicitly mentions Firebird MGA:
```cpp
// Lines: 55-60
## MGA Compliance (Phase 4A.2 - October 2025)

- **xmin/xmax tracking**: Each node has xmin/xmax
- **Snapshot isolation**: Snapshot parameter in search/insert APIs
- **Visibility filtering**: During graph traversal, skip deleted nodes
- **Garbage collection**: removeDeadEntries() for dead node removal
- **Stable TIDs**: Nodes reference stable tuple IDs (heap TIDs)
```

---

## 7. R-TREE INDEX

**Status**: ⚠️ **PARTIALLY COMPLIANT**
**Severity**: **HIGH**

### Evidence Files
- `include/scratchbird/core/rtree.h`
- `src/core/rtree.cpp`

### VIOLATION 1: Uses PostgreSQL MVCC Snapshot (Rule 1)
**Severity**: HIGH
**MGA Rule Violated**: Rule 1 - NO SNAPSHOTS

**Evidence**:
```cpp
// File: include/scratchbird/core/rtree.h
// Lines: 269-272, 283-286

Status insert(const BoundingBox& bbox,
             const TID& tid,
             struct Snapshot* snapshot,  // ❌ WRONG
             ErrorContext* ctx = nullptr);

Status search(const BoundingBox& bbox,
             struct Snapshot* snapshot,  // ❌ WRONG
             std::vector<TID>* tids_out,
             ErrorContext* ctx = nullptr);
```

---

### VIOLATION 2: No TIP Lookups in Implementation (Rule 2)
**Severity**: HIGH
**MGA Rule Violated**: Rule 2 - Transaction Inventory Pages (TIP) Required

**Evidence**:
- R-tree implementation `src/core/rtree.cpp` exists
- Grep search confirmed: 0 occurrences of `isSnapshotVisible` or `getTransactionState`
- **This suggests visibility checking is not yet implemented**

---

### POSITIVE FINDINGS:

✅ **COMPLIANT:** R-tree header shows xmin/xmax tracking at entry level:
```cpp
// File: include/scratchbird/core/rtree.h
// Lines: 166-168

// MGA compliance
uint64_t entry_xmin; // Transaction that created this entry
uint64_t entry_xmax; // Transaction that deleted this entry
```

✅ **COMPLIANT:** Documentation explicitly mentions Firebird MGA:
```cpp
// Lines: 66-72
## MGA Compliance

- **xmin/xmax tracking**: Each entry has xmin/xmax
- **Snapshot isolation**: Snapshot parameter in search/insert APIs
- **Visibility filtering**: During tree traversal, skip deleted entries
- **Garbage collection**: removeDeadEntries() for dead entry removal
- **Stable TIDs**: Entries reference stable tuple IDs (heap TIDs)
```

✅ **COMPLIANT:** R-tree has visibility checking method signature:
```cpp
// Lines: 480
bool isEntryVisible(const RTreeEntry& entry, struct Snapshot* snapshot) const;
```

**However:** Since `isSnapshotVisible()` and `getTransactionState()` are not found in the implementation, this method may not be implemented yet, or uses a different pattern.

---

## CROSS-CUTTING ANALYSIS

### Pattern: Systematic PostgreSQL MVCC Implementation

All implemented indexes (B-tree, Hash, Bitmap, GIN) follow the same incorrect pattern:

1. **API Level:** Accept `struct Snapshot *snapshot` parameter
2. **Visibility Logic:** Call `TransactionManager::isSnapshotVisible(xid, snapshot)`
3. **No TIP Lookups:** Do NOT call `getTransactionState()` for TIP-based visibility
4. **PostgreSQL Architecture:** Check if transaction is in snapshot's active transaction array

### Pattern: Newer Indexes Show Awareness But Lack Implementation

The newer indexes (BRIN, HNSW, R-tree) show evidence of MGA awareness:

1. **Documentation:** Explicitly mention "Firebird MGA"
2. **Data Structures:** Include xmin/xmax fields
3. **API Design:** Have snapshot parameters (incorrect, but consistent)
4. **Implementation:** Either missing or doesn't perform visibility checks yet

This suggests these indexes were designed with MGA in mind but haven't been fully implemented.

---

## SEVERITY CLASSIFICATION

### CRITICAL Violations (Immediate Action Required)

1. **All indexes use `struct Snapshot *` parameters** (Rule 1 violation)
   - Affects: B-tree, Hash, Bitmap, GIN, BRIN, HNSW, R-tree
   - Impact: Architectural incompatibility with Firebird MGA

2. **Implemented indexes call `isSnapshotVisible()`** (Rule 3 violation)
   - Affects: B-tree, Bitmap, GIN
   - Impact: Wrong visibility algorithm (PostgreSQL vs Firebird)

3. **No consistent TIP usage** (Rule 2 violation)
   - Affects: B-tree, Hash, Bitmap, GIN
   - Impact: Missing core MGA mechanism

### HIGH Violations (Architectural Redesign Needed)

1. **Hash index has no xmin/xmax in entries** (Rule 6 violation)
   - Impact: Cannot perform visibility checks at index level

2. **Bitmap index does post-filtering (20-40% overhead)** (Rule 3 violation)
   - Impact: Performance degradation

3. **GIN index mixes TIP and snapshots** (Rule 3 violation)
   - Impact: Architectural confusion

### MEDIUM Violations (Documentation/Consistency)

1. **Comments conflate MVCC and MGA** (Rule 0 violation)
   - Affects: All indexes
   - Impact: Developer confusion, maintenance issues

---

## RECOMMENDATIONS

### Immediate Actions (Phase 1 - Critical)

1. **Replace All Snapshot Parameters with TransactionId**
   ```cpp
   // BEFORE (WRONG):
   Status search(..., struct Snapshot *snapshot, ...)

   // AFTER (CORRECT):
   Status search(..., TransactionId current_xid, ...)
   ```

2. **Replace All isSnapshotVisible() Calls with getTransactionState()**
   ```cpp
   // BEFORE (WRONG):
   bool visible = txn_mgr->isSnapshotVisible(xmin, snapshot);

   // AFTER (CORRECT):
   TransactionState state;
   Status s = txn_mgr->getTransactionState(xmin, state, ctx);
   bool visible = (state == TransactionState::COMMITTED && xmin < current_xid);
   ```

3. **Implement TIP-Based Visibility Checks in All Indexes**
   - Add TIP lookup infrastructure
   - Update all visibility checking code
   - Remove snapshot-based logic

### Medium-Term Actions (Phase 2 - Architectural)

4. **Add xmin/xmax to Hash Index Entries**
   ```cpp
   struct HashEntry {
       uint64_t he_key_hash;
       uint64_t he_tuple_id;
       uint64_t he_xmin;  // ADD THIS
       uint64_t he_xmax;  // ADD THIS
   };
   ```

5. **Implement Index-Level Visibility in Bitmap**
   - Store xmin/xmax in bitmap structure (space trade-off)
   - OR accept post-filtering overhead for low-cardinality use case

6. **Complete Implementation of BRIN, HNSW, R-tree**
   - These indexes show MGA awareness but need full implementation
   - Ensure visibility logic uses TIP, not snapshots

### Long-Term Actions (Phase 3 - Documentation)

7. **Update All Comments to Use "MGA" Not "MVCC"**
8. **Add MGA Architecture Documentation**
9. **Create Migration Guide from Current State to MGA-Compliant State**

---

## COMPLIANCE CHECKLIST (Per MGA_RULES.md Rule 13)

For each index, checking against MGA Compliance Indicators:

| Indicator | B-tree | Hash | Bitmap | GIN | BRIN | HNSW | R-tree |
|-----------|--------|------|--------|-----|------|------|--------|
| ✅ TIP implementation | ❌ | ❌ | ❌ | ⚠️ | ❓ | ❓ | ❓ |
| ✅ `getTransactionState()` calls | ❌ | ❌ | ❌ | ⚠️ | ❓ | ❓ | ❌ |
| ✅ `TxState` enum usage | ❌ | ❌ | ❌ | ⚠️ | ❓ | ❓ | ❌ |
| ✅ OIT/OAT/OST markers | N/A | N/A | N/A | N/A | N/A | N/A | N/A |
| ✅ Back pointers (new→old) | N/A | N/A | N/A | N/A | N/A | N/A | N/A |
| ✅ In-place updates | N/A | N/A | N/A | N/A | N/A | N/A | N/A |
| ✅ Stable TIDs | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ✅ "Back version" terminology | N/A | N/A | N/A | N/A | N/A | N/A | N/A |

Legend:
- ✅ = Compliant
- ❌ = Non-compliant
- ⚠️ = Partially compliant (mixed usage)
- ❓ = Cannot verify (no implementation)
- N/A = Not applicable to indexes (heap-only feature)

**Note:** OIT/OAT/OST markers and back-versioning are heap-level features, not index features. Indexes only need to respect TIP-based visibility.

---

## ARCHITECTURAL ROOT CAUSE

The root cause of all violations is a **systematic PostgreSQL MVCC design** instead of Firebird MGA:

### Current Architecture (PostgreSQL MVCC):
```
Transaction Start → Create Snapshot (xmin, xmax, active_xids[])
                   ↓
Index Search → Pass Snapshot → Check if XID in active_xids[]
                   ↓
Return TIDs → Filter by snapshot visibility
```

### Required Architecture (Firebird MGA):
```
Transaction Start → Assign TransactionId (sequential counter)
                   ↓
Index Search → Pass TransactionId → Look up TIP for each XID
                   ↓                  (2-bit state per transaction)
Return TIDs → Filter by: TX_COMMITTED && XID < current_xid
```

---

## IMPACT ASSESSMENT

### Performance Impact
- **Bitmap Index:** 20-40% overhead from post-filtering (acknowledged in code comments)
- **All Indexes:** Snapshot active transaction array search is O(N) where N = active transactions
- **MGA Alternative:** TIP lookup is O(1) with 2-bit state

### Correctness Impact
- **Isolation Violations:** PostgreSQL MVCC snapshots don't match Firebird MGA semantics
- **Visibility Anomalies:** "Oldest Interesting Transaction" (OIT) concept not implemented
- **Garbage Collection Issues:** Sweep manager needs TIP, but indexes use snapshots

### Maintenance Impact
- **Code Confusion:** Comments say "Firebird MGA" but code implements PostgreSQL MVCC
- **Architecture Drift:** Newer indexes (BRIN, HNSW, R-tree) designed for MGA but not implemented
- **Technical Debt:** Complete rewrite of visibility checking needed across all 7 indexes

---

## INDEX AUDIT CONCLUSION

**ALL 7 INDEX TYPES ARE NON-COMPLIANT OR PARTIALLY COMPLIANT** with Firebird MGA architecture.

The codebase shows a **systematic implementation of PostgreSQL MVCC** patterns despite documentation claims of "Firebird MGA." This represents a critical architectural mismatch that affects:

1. **Correctness** - Wrong isolation semantics
2. **Performance** - Inefficient visibility checks
3. **Maintainability** - Conflicting documentation and code
4. **Future Development** - Newer indexes designed for MGA but awaiting implementation

**Recommendation:** Prioritize a systematic rewrite of visibility checking across all index types to use TIP-based lookups (`getTransactionState()`) instead of snapshot-based checks (`isSnapshotVisible()`). This is foundational work that must be completed before ScratchBird can claim Firebird MGA compliance.

---

## UPDATED MGA COMPLIANCE SCORECARD

| Component | Status | Score |
|-----------|--------|-------|
| TIP Implementation | ✅ Compliant | 100% |
| Back-Versioning | ✅ Compliant | 100% |
| Stable TIDs | ✅ Compliant | 100% |
| Transaction Markers | ✅ Compliant | 100% |
| Visibility API (TransactionManager) | ❌ Non-Compliant | 0% |
| **Index Visibility (All 7 Types)** | ❌ **Non-Compliant** | **0%** |
| **Overall MGA Compliance** | ⚠️ **CRITICAL PARTIAL** | **50%** |

**Revised Status**: Due to systematic PostgreSQL MVCC implementation across all indexes, overall MGA compliance is now assessed at 50% (was 80% before index audit).

**Correction Effort**: 150-220 hours estimated to fix TransactionManager API + all 7 index types

**Risk**: HIGH - Requires complete visibility checking rewrite across entire codebase
