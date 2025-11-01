# Index Types: Correct Firebird MGA Compliance Analysis

**Date**: November 1, 2025
**Status**: CORRECTED ANALYSIS
**Replaces**: INDEX_TYPES_MGA_COMPLIANCE_ANALYSIS.md (INVALID - MVCC contamination)

---

## Executive Summary

This document provides a **corrected** analysis of all 7 index types in ScratchBird for Firebird MGA (Multi-Generational Architecture) compliance. The previous analysis was based on PostgreSQL MVCC concepts (snapshots) rather than Firebird MGA concepts (TIP).

### Critical Finding

**ALL index implementations are contaminated with PostgreSQL MVCC concepts**. None properly implement Firebird MGA with Transaction Inventory Pages (TIP).

### Correct Evaluation Criteria

✓ **Firebird MGA Compliance Requires**:
1. Uses TIP (Transaction Inventory Page) for visibility checks
2. Does NOT use Snapshot structures
3. Checks transaction state (TX_COMMITTED, TX_ACTIVE, TX_ABORTED) via TIP
4. Uses OIT/OAT/OST markers for garbage collection
5. Back-versioning (not forward-versioning) for update-heavy indexes

❌ **PostgreSQL MVCC Contamination Indicators**:
1. Uses `Snapshot` parameter
2. Calls `isSnapshotVisible()` or similar snapshot-based APIs
3. Snapshot arrays (`active_xids[]`, `xmin`, `xmax`)
4. Forward-versioning
5. No TIP references

---

## Corrected Comparison Table

| Index Type | TIP Usage | Snapshot Usage | xmin/xmax | Visibility Check | MGA Compliance | Contamination |
|------------|-----------|----------------|-----------|------------------|----------------|---------------|
| **B-tree** | ❌ No | ❌ Yes | ✅ Yes | ❌ Snapshot-based | ❌ INVALID | **CRITICAL** |
| **Bitmap** | ❌ No | ❌ Yes (heap) | ❌ No (by design) | ❌ Snapshot-based | ⚠️ PARTIAL | **HIGH** |
| **Hash** | ❌ No | ❌ Yes (heap) | ❌ No | ❌ Snapshot-based | ⚠️ PARTIAL | **HIGH** |
| **GIN** | ❌ No | ❌ Yes | ❌ No | ❌ Snapshot-based | ❌ INVALID | **HIGH** |
| **BRIN** | ❌ No | ❌ Yes (stub) | ✅ Yes (unused) | ❌ Stub | ❌ INVALID | **MEDIUM** |
| **HNSW** | ❌ No | ❌ Yes (stub) | ✅ Yes (unused) | ❌ Stub | ❌ INVALID | **MEDIUM** |
| **R-tree** | ❌ No | ❌ Yes | ✅ Yes (partial) | ❌ Partial | ❌ INVALID | **MEDIUM** |

**Status**: 0 of 7 indexes correctly implement Firebird MGA

---

## Detailed Corrected Analysis

### 1. B-tree Index

**Previous Analysis**: ✅ "Full MGA compliance"
**Corrected Analysis**: ❌ **CRITICALLY CONTAMINATED**

#### What Was Incorrectly Implemented

**File**: `src/core/btree.cpp`

```cpp
// WRONG: Uses PostgreSQL MVCC Snapshot
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, struct Snapshot *snapshot) const
{
    if (snapshot == nullptr) return true;

    auto *txn_mgr = db_->transaction_manager();
    auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);

    // ❌ PostgreSQL MVCC API, not Firebird MGA
    if (!txn_mgr->isSnapshotVisible(xmin, txn_snapshot)) {
        return false;
    }

    if (xmax != 0) {
        if (txn_mgr->isSnapshotVisible(xmax, txn_snapshot)) {
            return false;
        }
    }

    return true;
}
```

**Problems**:
1. ❌ Uses `Snapshot *snapshot` parameter (PostgreSQL concept)
2. ❌ Uses `isSnapshotVisible()` API (PostgreSQL API)
3. ❌ No TIP (Transaction Inventory Page) lookups
4. ❌ No transaction state checking (TX_COMMITTED, TX_ACTIVE, TX_ABORTED)
5. ❌ No OIT/OAT/OST marker usage

#### What Should Have Been Implemented

```cpp
// CORRECT: Uses Firebird MGA TIP
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, TransactionId current_xid) const
{
    // Legacy entries always visible
    if (xmin == 0) return true;

    auto *txn_mgr = db_->transaction_manager();
    if (txn_mgr == nullptr) return true;

    // ✓ CORRECT: Look up transaction state in TIP
    TxState xmin_state = txn_mgr->getTransactionState(xmin);  // TIP lookup

    // Own changes always visible
    if (xmin == current_xid) {
        return true;
    }

    // Check if creating transaction is committed and older
    if (xmin_state == TX_COMMITTED && xmin < current_xid) {
        // Check deletion
        if (xmax != 0) {
            TxState xmax_state = txn_mgr->getTransactionState(xmax);  // TIP lookup
            if (xmax_state == TX_COMMITTED && xmax < current_xid) {
                return false;  // Deleted before our transaction
            }
        }
        return true;  // Visible
    }

    return false;  // Not committed or too new
}
```

#### Correction Required

**Estimated Effort**: 40-60 hours

**Tasks**:
1. Remove all `Snapshot` parameters from B-tree API
2. Replace with `TransactionId current_xid` parameter
3. Rewrite `isEntryVisible()` to use TIP lookups
4. Update all call sites (executor, storage_engine)
5. Rewrite test suite (no snapshot-based tests)
6. Add TIP cache integration

**Impact**: Core index type - affects all other index implementations

---

### 2. Bitmap Index

**Previous Analysis**: ✅ "Heap-layer MGA compliance (production-ready)"
**Corrected Analysis**: ⚠️ **PARTIAL - Heap contamination**

#### Current Implementation

**File**: `src/core/bitmap_index.cpp`

```cpp
// From bitmap_index.cpp:477 (heap-layer visibility check)
std::vector<TID> filterTidsByVisibility(
    const std::vector<TID>& tids,
    TransactionManager::Snapshot* snapshot)  // ❌ Snapshot parameter
{
    std::vector<TID> visible_tids;

    for (const TID& tid : tids) {
        // Pin heap page
        tuple_header = fetch_tuple_header(tid);

        // ❌ Snapshot-based visibility check (PostgreSQL MVCC)
        bool xmin_visible = txn_manager->isSnapshotVisible(
            tuple_header->xmin, snapshot);
        bool xmax_visible = txn_manager->isSnapshotVisible(
            tuple_header->xmax, snapshot);

        if (xmin_visible && !xmax_visible) {
            visible_tids.push_back(tid);
        }
    }

    return visible_tids;
}
```

#### Analysis

**What's Correct** (relative to other indexes):
- ✓ Bitmap structure itself has no xmin/xmax (correct for this index type)
- ✓ Returns all matching TIDs first (doesn't filter at index level)
- ✓ Defers visibility to heap layer (appropriate for bitmap indexes)

**What's Contaminated**:
- ❌ Heap visibility check uses snapshots (PostgreSQL MVCC)
- ❌ Uses `isSnapshotVisible()` API
- ❌ No TIP usage

#### Correction Required

**Estimated Effort**: 10-15 hours

**Tasks**:
1. Replace `Snapshot *snapshot` parameter with `TransactionId current_xid`
2. Rewrite heap visibility check to use TIP lookups
3. Change `isSnapshotVisible()` calls to `getTransactionState()` calls
4. Update TransactionManager API if needed

**Note**: This index is the easiest to fix since it already has the right structure (no index-level xmin/xmax).

---

### 3. Hash Index

**Previous Analysis**: ✅ "Heap-layer MGA compliance (snapshot unused)"
**Corrected Analysis**: ⚠️ **PARTIAL - Heap contamination**

#### Current Implementation

**File**: `src/core/hash_index.cpp`

```cpp
// Hash index does minimal visibility checking
std::vector<TID> HashIndex::search(
    const std::vector<uint8_t>& key,
    TransactionManager::Snapshot* snapshot,  // ❌ Snapshot parameter (unused!)
    ErrorContext* ctx)
{
    // Find bucket
    uint32_t bucket = hash_function(key);

    // Return all TIDs in bucket (no filtering)
    return bucket_tids[bucket];

    // Executor filters by visibility using heap tuple headers
}
```

#### Analysis

**What's Correct**:
- ✓ Snapshot parameter unused (doesn't filter at index level)
- ✓ Returns all TIDs, defers to executor
- ✓ Uses tombstone (tid=0) for deletion marking

**What's Contaminated**:
- ❌ Snapshot parameter in API (even if unused)
- ❌ Executor uses snapshot-based heap visibility check
- ❌ No TIP usage

#### Correction Required

**Estimated Effort**: 5-10 hours

**Tasks**:
1. Remove unused `Snapshot *snapshot` parameter
2. Fix executor heap visibility checks to use TIP
3. API cleanup

**Note**: Simplest to fix - snapshot already unused.

---

### 4. GIN Index (Generalized Inverted Index)

**Previous Analysis**: ✅ "Hybrid approach (production-ready)"
**Corrected Analysis**: ❌ **CONTAMINATED - Snapshot usage**

#### Current Implementation

**File**: `src/core/gin_index.cpp`

```cpp
// GIN has pending list for fast inserts
std::vector<TID> GINIndex::search(
    const std::vector<uint8_t>& key,
    TransactionManager::Snapshot* snapshot,  // ❌ Snapshot parameter
    ErrorContext* ctx)
{
    // Search main index
    std::vector<TID> main_tids = search_btree(key);

    // Search pending list
    std::vector<TID> pending_tids = search_pending_list(key, snapshot);  // ❌

    // Merge results
    std::vector<TID> all_tids = merge(main_tids, pending_tids);

    // Filter by visibility
    return filterByVisibility(all_tids, snapshot);  // ❌ Snapshot-based
}
```

#### Analysis

**What's Correct**:
- ✓ Pending list concept (optimization for GIN)
- ✓ Two-phase search (main + pending)

**What's Contaminated**:
- ❌ Snapshot-based visibility in pending list
- ❌ `filterByVisibility()` uses PostgreSQL MVCC
- ❌ No TIP usage

#### Correction Required

**Estimated Effort**: 20-30 hours

**Tasks**:
1. Replace snapshot parameters with transaction ID
2. Rewrite pending list visibility checks (TIP-based)
3. Rewrite `filterByVisibility()` to use TIP
4. Update pending list vacuum logic

---

### 5. BRIN Index (Block Range Index)

**Previous Analysis**: ⚠️ "MGA-ready structures, stub implementation"
**Corrected Analysis**: ❌ **CONTAMINATED - Wrong structures**

#### Current Implementation

**File**: `src/core/brin_index.cpp`

```cpp
// BRIN entry structure
struct BRINEntry {
    PageRange range;
    uint64_t brn_xmin;  // ✅ Has xmin field
    uint64_t brn_xmax;  // ✅ Has xmax field
    uint32_t flags;
    // ... min/max values for columns
};

// Visibility check (STUB)
bool BRINIndex::isEntryVisible(
    const BRINEntry* entry,
    TransactionManager::Snapshot* snapshot) const  // ❌ Snapshot
{
    // ❌ STUB: Always returns true
    return true;
}
```

#### Analysis

**What's Correct**:
- ✓ Has `brn_xmin` and `brn_xmax` fields (structure ready)

**What's Contaminated**:
- ❌ Snapshot parameter (wrong API)
- ❌ Stub implementation (doesn't check anything)
- ❌ No TIP usage

#### Correction Required

**Estimated Effort**: 15-20 hours

**Tasks**:
1. Replace snapshot parameter with transaction ID
2. Implement `isEntryVisible()` with TIP lookups
3. Add soft deletion support
4. Test with multi-transaction workloads

---

### 6. HNSW Index (Hierarchical Navigable Small World)

**Previous Analysis**: ⚠️ "MGA-ready structures, stub implementation"
**Corrected Analysis**: ❌ **CONTAMINATED - Wrong structures**

#### Current Implementation

**File**: `src/core/hnsw_index.cpp`

```cpp
// HNSW node structure
struct HNSWNode {
    UUID node_uuid;
    uint64_t node_xmin;  // ✅ Has xmin field
    uint64_t node_xmax;  // ✅ Has xmax field
    uint32_t flags;
    std::vector<UUID> neighbors;
    // ... vector data
};

// Visibility check (STUB)
bool HNSWIndex::isNodeVisible(
    const HNSWNode* node,
    TransactionManager::Snapshot* snapshot) const  // ❌ Snapshot
{
    // ❌ STUB: Always returns true
    return true;
}
```

#### Analysis

**What's Correct**:
- ✓ Has `node_xmin` and `node_xmax` fields
- ✓ UUID-based node references

**What's Contaminated**:
- ❌ Snapshot parameter (wrong API)
- ❌ Stub implementation
- ❌ No TIP usage
- ❌ Graph traversal doesn't check visibility

#### Correction Required

**Estimated Effort**: 25-35 hours

**Tasks**:
1. Replace snapshot parameter with transaction ID
2. Implement `isNodeVisible()` with TIP lookups
3. Add visibility checks to graph traversal
4. Handle concurrent graph modifications
5. Implement soft deletion for nodes

---

### 7. R-tree Index (Spatial Index)

**Previous Analysis**: ⚠️ "Partial MGA implementation (simplified visibility)"
**Corrected Analysis**: ❌ **CONTAMINATED - Wrong API**

#### Current Implementation

**File**: `src/core/rtree_index.cpp`

```cpp
// R-tree entry structure
struct RTreeEntry {
    BoundingBox bbox;
    TID tid;
    uint64_t entry_xmin;  // ✅ Has xmin field
    uint64_t entry_xmax;  // ✅ Has xmax field
    uint32_t flags;
};

// Visibility check (PARTIAL)
bool RTreeIndex::isEntryVisible(
    const RTreeEntry* entry,
    TransactionManager::Snapshot* snapshot) const  // ❌ Snapshot
{
    // ⚠️ PARTIAL: Only checks xmax
    if (entry->entry_xmax != 0) {
        // ❌ Snapshot-based check
        return !txn_manager->isSnapshotVisible(entry->entry_xmax, snapshot);
    }
    return true;  // ❌ Doesn't check xmin!
}
```

#### Analysis

**What's Correct**:
- ✓ Has `entry_xmin` and `entry_xmax` fields
- ✓ Attempts visibility checking (not just stub)

**What's Contaminated**:
- ❌ Snapshot parameter (wrong API)
- ❌ Only checks `xmax`, ignores `xmin` (incomplete)
- ❌ Uses `isSnapshotVisible()` (PostgreSQL MVCC)
- ❌ No TIP usage

#### Correction Required

**Estimated Effort**: 15-20 hours

**Tasks**:
1. Replace snapshot parameter with transaction ID
2. Implement complete visibility check (xmin AND xmax)
3. Use TIP lookups instead of snapshot visibility
4. Add to range search algorithm
5. Test with concurrent spatial queries

---

## Part 3: Prerequisites for Correction

### 3.1 Missing Infrastructure

All index corrections depend on these prerequisites:

#### 1. Transaction Inventory Page (TIP) Implementation

**Status**: ❌ **NOT IMPLEMENTED**

**Required Files**:
- `include/scratchbird/core/tip.h`
- `src/core/tip.cpp`

**Required API**:
```cpp
class TransactionInventoryPage {
public:
    // Get transaction state from TIP
    static TxState getTransactionState(Database* db, TransactionId xid);

    // Set transaction state in TIP
    static void setTransactionState(Database* db, TransactionId xid, TxState state);

    // Allocate new TIP page when needed
    static uint32_t allocateTipPage(Database* db);

private:
    // TIP cache for performance (temporal locality)
    struct TIPCache {
        LRUCache<TransactionId, TxState> cache;
        std::mutex mutex;
    };
};
```

**Estimated Effort**: 30-40 hours

#### 2. Transaction Markers (OIT/OAT/OST)

**Status**: ❌ **NOT IMPLEMENTED**

**Required Changes**:
- Modify `DatabaseHeader` to include OIT/OAT/OST
- Implement marker update logic
- Implement sweep trigger: `(OST - OIT) > sweep_interval`

**Estimated Effort**: 15-20 hours

#### 3. TransactionManager API Redesign

**Status**: ❌ **CONTAMINATED** (Currently PostgreSQL MVCC)

**Current API** (WRONG):
```cpp
class TransactionManager {
    struct Snapshot {
        TransactionId xmin;
        TransactionId xmax;
        TransactionId *active_xids;
        uint32_t xcnt;
        // ... PostgreSQL fields
    };

    Status getSnapshot(Snapshot& snapshot, ErrorContext* ctx);
    bool isSnapshotVisible(TransactionId xid, const Snapshot* snapshot);
};
```

**Required API** (CORRECT):
```cpp
class TransactionManager {
    // NO Snapshot structure!

    // Begin transaction (returns transaction ID)
    TransactionId beginTransaction(IsolationLevel level, bool read_only);

    // Commit/rollback
    void commitTransaction(TransactionId xid);
    void rollbackTransaction(TransactionId xid);

    // TIP-based visibility
    TxState getTransactionState(TransactionId xid);
    bool isVersionVisible(TransactionId version_xid, TransactionId reader_xid);

    // Transaction markers
    void getTransactionMarkers(TransactionId& oit, TransactionId& oat,
                               TransactionId& ost, TransactionId& next);
    void updateTransactionMarkers();
};
```

**Estimated Effort**: 40-50 hours

---

## Part 4: Correction Priority

### 4.1 Phase 1: Infrastructure (Must Be Done First)

**Estimated Effort**: 85-110 hours

1. **Implement TIP** (30-40h)
   - TIP page structure
   - TIP cache
   - Transaction state lookups

2. **Implement Transaction Markers** (15-20h)
   - OIT/OAT/OST in database header
   - Marker update logic
   - Sweep trigger logic

3. **Redesign TransactionManager** (40-50h)
   - Remove Snapshot structure
   - Add TIP-based APIs
   - Update all existing callers

### 4.2 Phase 2: Core Indexes

**Estimated Effort**: 55-80 hours

1. **B-tree** (40-60h) - Most critical, most contaminated
2. **Hash** (5-10h) - Simplest to fix
3. **Bitmap** (10-15h) - Heap-layer only

### 4.3 Phase 3: Advanced Indexes

**Estimated Effort**: 55-85 hours

1. **GIN** (20-30h) - Pending list complexity
2. **BRIN** (15-20h) - Range index
3. **R-tree** (15-20h) - Spatial index
4. **HNSW** (25-35h) - Most complex (graph traversal)

### 4.4 Total Correction Effort

**Total**: 195-275 hours (5-7 weeks)

---

## Part 5: Testing Strategy

### 5.1 TIP Testing

**New Test File**: `tests/unit/test_tip.cpp`

**Test Cases**:
- TIP page allocation
- Transaction state storage/retrieval
- TIP cache effectiveness
- Concurrent TIP access
- TIP wraparound handling

### 5.2 Index Visibility Testing

**Updated Test Files**:
- `tests/unit/test_btree_mga_compliance.cpp`
- `tests/unit/test_hash_index.cpp`
- `tests/unit/test_bitmap_index.cpp`
- `tests/unit/test_gin_index.cpp`
- `tests/unit/test_brin_index.cpp`
- `tests/unit/test_hnsw_index.cpp`
- `tests/unit/test_rtree_index.cpp`

**Test Patterns** (NO SNAPSHOTS):
```cpp
TEST_F(IndexTest, VisibilityWithTIP)
{
    // Transaction 100: Insert entry
    uint64_t xid_100 = txn_mgr->beginTransaction();
    index->insert(key, tid, xid_100);

    // Transaction 99 (started before insert)
    uint64_t xid_99 = 99;
    std::vector<TID> tids;

    // Should NOT see entry (created by txn 100 > 99)
    index->search(key, xid_99, &tids);  // Pass xid, not snapshot!
    EXPECT_EQ(tids.size(), 0);

    // Commit transaction 100
    txn_mgr->commitTransaction(xid_100);

    // Transaction 101 (started after commit)
    uint64_t xid_101 = 101;

    // Should see entry (created by committed txn 100 < 101)
    index->search(key, xid_101, &tids);
    EXPECT_EQ(tids.size(), 1);
}
```

---

## Part 6: Migration Strategy

### 6.1 Cannot Reuse Existing Work

**Critical Realization**: Almost none of the Task 17 code can be salvaged.

**Why?**
- API signatures wrong (Snapshot instead of TransactionId)
- Visibility logic wrong (snapshot-based instead of TIP-based)
- Test cases wrong (test PostgreSQL behavior, not Firebird)
- Documentation wrong (describes MVCC, not MGA)

**Salvageable Components** (< 10%):
- `btn_xmin`/`btn_xmax` field additions (structure correct)
- `rhd_b_page`/`rhd_b_line` concepts (back-versioning structure)
- markDeleted() concept (soft deletion valid)

**Must Be Rewritten** (> 90%):
- All visibility checking code
- All API signatures
- All test cases
- All documentation

### 6.2 Recommended Approach

**Option 1: Complete Rewrite** (Recommended)
- Implement TIP infrastructure first
- Rewrite all indexes from scratch with correct API
- Cleaner, less technical debt

**Option 2: Incremental Fix**
- Fix TransactionManager API first
- Fix each index one by one
- Higher risk of missing contamination

**Recommendation**: Complete rewrite (Option 1) - cleaner and faster in long run

---

## Conclusion

### Summary

**Critical Finding**: All 7 index types are contaminated with PostgreSQL MVCC concepts (snapshots) instead of implementing Firebird MGA (Transaction Inventory Pages).

**Root Cause**: Fundamental misunderstanding of the difference between:
- PostgreSQL MVCC (snapshot-based visibility)
- Firebird MGA (TIP-based visibility)

**Impact**:
- 0 of 7 indexes correctly implement Firebird MGA
- All Task 17 work (2,500+ lines) invalid
- Previous index analysis completely wrong
- Test suite tests wrong behavior

**Correction Effort**: 195-275 hours (5-7 weeks)

**Critical Prerequisites**:
1. Implement TIP (Transaction Inventory Page)
2. Implement OIT/OAT/OST markers
3. Redesign TransactionManager API (remove Snapshot)

**Path Forward**:
1. Halt all work based on Task 17
2. Implement TIP infrastructure
3. Redesign TransactionManager API
4. Rewrite all index visibility logic
5. Rewrite all test cases
6. Update all documentation

### The Irony

The indexes that DON'T use snapshots (Bitmap, Hash) are actually **closer** to correct Firebird MGA than the B-tree that DOES use snapshots!

**Lesson**: Firebird MGA does NOT use snapshots. It uses TIP (Transaction Inventory Page) lookups. Any code using `TransactionManager::Snapshot` is PostgreSQL MVCC, not Firebird MGA.

---

**Document Status**: CORRECTED ANALYSIS
**Replaces**: INDEX_TYPES_MGA_COMPLIANCE_ANALYSIS.md (INVALID)
**Date**: November 1, 2025
**Next Steps**: Implement TIP infrastructure before any index work
