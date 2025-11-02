# MGA Compliance Fix Plan - ScratchBird Database

**Created**: November 2, 2025
**Status**: ACTIVE - CRITICAL PRIORITY
**Goal**: Achieve 100% Firebird MGA compliance (currently 50%)

---

## 🔴 CRITICAL: WHY THIS WORK MUST BE DONE

### The Problem

**ScratchBird currently implements PostgreSQL MVCC (snapshot-based visibility) instead of Firebird MGA (TIP-based visibility).**

This is **NOT a design choice** - it is an **architectural violation** that must be corrected. The codebase has:
- ✅ **CORRECT**: Firebird MGA back-versioning (old data → back version, new data in-place)
- ✅ **CORRECT**: Stable TIDs (indexes never updated on UPDATE)
- ✅ **CORRECT**: TIP (Transaction Inventory Pages) implementation
- ❌ **WRONG**: PostgreSQL snapshot-based visibility in all indexes

### Why This Is Wrong

Per `/MGA_RULES.md` (mandatory reading):

> **Rule 0: The Fundamental Distinction**
>
> ScratchBird uses Firebird MGA, NOT PostgreSQL MVCC.
>
> **PostgreSQL MVCC**: "Is this XID in the snapshot's active transaction array?"
> **Firebird MGA**: "Is this XID committed and older than me?" (TIP lookup)
>
> **ONE uses snapshots, ONE uses TIP. ScratchBird uses TIP (Firebird MGA).**
>
> **If you see `Snapshot` anywhere in transaction-related code, it's WRONG.**

### Consequences of NOT Fixing This

1. **Architectural Impurity**: Mixing two incompatible concurrency models
2. **Performance Issues**: Snapshot array lookups are O(N), TIP lookups are O(1)
3. **Isolation Violations**: PostgreSQL snapshot semantics ≠ Firebird MGA semantics
4. **Garbage Collection Broken**: Sweep manager needs TIP, but indexes use snapshots
5. **Cannot Ship ALPHA**: This is a blocking architectural violation

---

## 📋 MANDATORY READING BEFORE ANY WORK

**CRITICAL**: Read these documents BEFORE starting any MGA compliance work:

### 1. MGA Rules (MUST READ FIRST)
**File**: `/MGA_RULES.md` (650 lines)
**Why**: Contains 15 absolute rules for Firebird MGA implementation
**Key Rules**:
- Rule 1: NO SNAPSHOTS (if you see `Snapshot`, it's wrong)
- Rule 2: TIP Required (2-bit transaction state storage)
- Rule 3: Visibility uses TIP, NOT snapshots
- Rule 11: API signatures use `TransactionId`, NOT `Snapshot*`

### 2. Comprehensive Audit Report
**File**: `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md` (1,389 lines)
**Why**: Documents ALL violations with exact file:line references
**Key Sections**:
- TransactionManager violations (lines 252-371)
- Index violations by type (lines 521-1388)
- Compliance scorecard (lines 1372-1388)

### 3. MGA Specifications
**Files**:
- `/docs/specifications/MGA_IMPLEMENTATION.md` (687 lines)
- `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md` (1,570 lines)
**Why**: Complete technical details of Firebird MGA architecture

### 4. Previous Contamination Analysis
**File**: `/docs/analysis/CRITICAL_MGA_MVCC_CONFUSION_ANALYSIS.md` (8,500 lines)
**Why**: Documents how PostgreSQL MVCC contamination happened and how to avoid it

---

## 🎯 PROJECT OVERVIEW

### Scope
Fix all PostgreSQL MVCC contamination to achieve pure Firebird MGA compliance.

### Components to Fix
1. TransactionManager API (remove Snapshot structure)
2. B-tree index visibility
3. Hash index visibility
4. Bitmap index visibility
5. GIN index visibility
6. BRIN index visibility (if implemented)
7. HNSW index visibility (if implemented)
8. R-tree index visibility

### Success Criteria
- ✅ Zero `Snapshot` structures in codebase
- ✅ Zero `isSnapshotVisible()` calls
- ✅ All visibility checks use `getTransactionState()` (TIP lookup)
- ✅ All index APIs use `TransactionId current_xid` parameter
- ✅ 100% MGA compliance (all tests pass)

### Timeline
**Total Effort**: 150-220 hours (4-6 weeks)
**Start Date**: TBD
**Target Completion**: TBD

---

## 📊 PROGRESS TRACKING

### Overall Status
- [ ] Phase 1: TransactionManager API redesign (20-30 hours)
- [ ] Phase 2: B-tree index fix (30-40 hours)
- [ ] Phase 3: Hash index fix (15-20 hours)
- [ ] Phase 4: Bitmap index fix (20-30 hours)
- [ ] Phase 5: GIN index fix (30-40 hours)
- [ ] Phase 6: Advanced indexes (BRIN, HNSW, R-tree) (35-60 hours)
- [ ] Phase 7: Testing & validation (20-30 hours)

**Current Phase**: NOT STARTED
**Hours Completed**: 0 / 220
**Completion**: 0%

---

## 🔧 PHASE 1: TransactionManager API Redesign

**Priority**: 🔴 CRITICAL (must complete first - all other phases depend on this)
**Effort**: 20-30 hours
**Status**: NOT STARTED

### Goals
1. Remove PostgreSQL `Snapshot` structure from TransactionManager
2. Replace `isSnapshotVisible()` with TIP-based `isVersionVisible()`
3. Update all callers to use new API

### Current Violations

**File**: `include/scratchbird/core/transaction_manager.h`

**Lines 228-250**: Snapshot structure (WRONG - PostgreSQL MVCC)
```cpp
struct Snapshot {
    uint64_t xmin;
    uint64_t xmax;
    std::vector<uint64_t> active_xids;  // ❌ WRONG
    uint64_t snapshot_xid;
    std::chrono::system_clock::time_point timestamp;
};
```

**Line 254**: `getSnapshot()` method (WRONG)
**Line 258**: `isSnapshotVisible()` method (WRONG)

**File**: `src/core/transaction_manager.cpp`

**Lines 856-934**: `isSnapshotVisible()` implementation (WRONG)
**Lines 944-1038**: `getSnapshot()` implementation (WRONG)

### Required Changes

#### Step 1.1: Remove Snapshot Structure
**File**: `include/scratchbird/core/transaction_manager.h:228-250`

**Action**: DELETE the entire `Snapshot` struct

**Rationale**: Per MGA_RULES.md Rule 1, if you see `Snapshot` in transaction code, it's WRONG.

#### Step 1.2: Remove Snapshot Methods
**File**: `include/scratchbird/core/transaction_manager.h:254,258`

**Action**: DELETE these method declarations:
```cpp
auto getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx) -> Status;
auto isSnapshotVisible(uint64_t xid, const Snapshot *snapshot) -> bool;
```

#### Step 1.3: Add TIP-Based Visibility Method
**File**: `include/scratchbird/core/transaction_manager.h`

**Action**: ADD this method declaration:
```cpp
/**
 * Check if a tuple version is visible to a transaction (Firebird MGA).
 * Uses TIP (Transaction Inventory Page) lookup, NOT snapshots.
 *
 * @param version_xid Transaction ID that created the version
 * @param reader_xid Transaction ID of the reader
 * @return true if version is visible to reader
 */
auto isVersionVisible(uint64_t version_xid, uint64_t reader_xid) -> bool;
```

**Reference**: MGA_RULES.md Rule 3 (lines 121-145)

#### Step 1.4: Implement isVersionVisible()
**File**: `src/core/transaction_manager.cpp`

**Action**: ADD this implementation (replace lines 856-934):
```cpp
auto TransactionManager::isVersionVisible(uint64_t version_xid, uint64_t reader_xid) -> bool
{
    // Own changes always visible
    if (version_xid == reader_xid) {
        return true;
    }

    // Look up transaction state in TIP (NOT snapshot!)
    TransactionState state;
    ErrorContext ctx;
    Status status = getTransactionState(version_xid, state, &ctx);

    if (status != Status::OK) {
        // If TIP lookup fails, assume not visible
        return false;
    }

    // Only committed transactions older than reader are visible
    // This is the CORE of Firebird MGA visibility
    if (state == TransactionState::COMMITTED && version_xid < reader_xid) {
        return true;
    }

    // Active, aborted, or limbo transactions are not visible
    return false;
}
```

**Reference**: MGA_RULES.md Rule 3 (lines 121-145)

#### Step 1.5: Remove Old Implementations
**File**: `src/core/transaction_manager.cpp:856-1038`

**Action**: DELETE both `isSnapshotVisible()` and `getSnapshot()` implementations

**Rationale**: These are PostgreSQL MVCC patterns that violate Firebird MGA.

### Testing Requirements

Create test file: `tests/core/transaction_manager_mga_test.cpp`

**Test Cases**:
1. Own transaction can see own changes
2. Committed transaction visible to later transactions
3. Active transaction not visible to concurrent transactions
4. Aborted transaction not visible to any transactions
5. Older transactions cannot see newer transactions
6. TIP state changes reflected in visibility

### Validation Checklist
- [ ] Snapshot structure removed
- [ ] getSnapshot() removed
- [ ] isSnapshotVisible() removed
- [ ] isVersionVisible() implemented with TIP lookup
- [ ] All tests pass
- [ ] No Snapshot references in transaction_manager.h
- [ ] No Snapshot references in transaction_manager.cpp

### Next Phase Dependencies
All index phases (2-6) depend on Phase 1 completion.

---

## 🔧 PHASE 2: B-tree Index Fix

**Priority**: 🔴 CRITICAL
**Effort**: 30-40 hours
**Status**: NOT STARTED
**Depends On**: Phase 1 complete

### Goals
1. Remove all Snapshot parameters from B-tree API
2. Replace with TransactionId current_xid parameter
3. Replace isSnapshotVisible() calls with TIP lookups
4. Ensure B-tree uses pure Firebird MGA visibility

### Current Violations

**Audit Reference**: `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md:551-700`

**File**: `include/scratchbird/core/btree.h`

**Violation 1 (Lines 183, 214-219)**: Snapshot parameters in search/rangeScan
```cpp
Status search(const std::vector<uint8_t> &key,
              struct Snapshot *snapshot,  // ❌ WRONG
              std::vector<TID> *tids_out,
              ErrorContext *ctx = nullptr);

std::unique_ptr<BTreeIterator>
rangeScan(const std::vector<uint8_t> *start_key,
          const std::vector<uint8_t> *end_key,
          struct Snapshot *snapshot,  // ❌ WRONG
          bool start_inclusive = true, bool end_inclusive = false,
          ErrorContext *ctx = nullptr);
```

**File**: `src/core/btree.cpp`

**Violation 2 (Lines 1103, 1125-1137)**: isSnapshotVisible() calls
```cpp
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, struct Snapshot *snapshot) const
{
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

**Violation 3**: No TIP lookups (grep confirms: 0 occurrences of `getTransactionState`)

### Required Changes

#### Step 2.1: Update B-tree Header API
**File**: `include/scratchbird/core/btree.h`

**Action**: Replace Snapshot parameters with TransactionId

**BEFORE**:
```cpp
Status search(const std::vector<uint8_t> &key,
              struct Snapshot *snapshot,
              std::vector<TID> *tids_out,
              ErrorContext *ctx = nullptr);
```

**AFTER**:
```cpp
Status search(const std::vector<uint8_t> &key,
              uint64_t current_xid,  // ✅ CORRECT - Firebird MGA
              std::vector<TID> *tids_out,
              ErrorContext *ctx = nullptr);
```

**Apply to**:
- `search()` method
- `rangeScan()` method
- `isEntryVisible()` method
- Any other methods with Snapshot parameters

#### Step 2.2: Update isEntryVisible() Implementation
**File**: `src/core/btree.cpp:1103-1137`

**Action**: Replace PostgreSQL visibility with Firebird MGA

**BEFORE (PostgreSQL MVCC)**:
```cpp
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, struct Snapshot *snapshot) const
{
    auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);
    if (!txn_mgr->isSnapshotVisible(xmin, txn_snapshot))
    {
        return false;
    }

    if (xmax != 0)
    {
        if (txn_mgr->isSnapshotVisible(xmax, txn_snapshot))
        {
            return false;
        }
    }
    return true;
}
```

**AFTER (Firebird MGA)**:
```cpp
bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t reader_xid) const
{
    // Own changes always visible
    if (xmin == reader_xid) {
        return true;
    }

    // Use TIP-based visibility (Firebird MGA)
    if (!txn_mgr->isVersionVisible(xmin, reader_xid))
    {
        return false;
    }

    // If tuple has been deleted
    if (xmax != 0)
    {
        // If deleting transaction is visible, tuple is deleted
        if (txn_mgr->isVersionVisible(xmax, reader_xid))
        {
            return false;
        }
    }

    return true;
}
```

**Reference**: MGA_RULES.md Rule 3 (lines 121-145)

#### Step 2.3: Update All search() Callers
**Action**: Find all callers of B-tree search/rangeScan methods

**Command**:
```bash
grep -rn "->search\|\.search\|->rangeScan\|\.rangeScan" --include="*.cpp" src/ | grep -i btree
```

**For Each Caller**:
- Remove Snapshot creation
- Pass current transaction ID instead
- Update error handling if needed

**Example Change**:

**BEFORE**:
```cpp
TransactionManager::Snapshot snapshot;
txn_mgr->getSnapshot(snapshot, ctx);
btree->search(key, &snapshot, &tids, ctx);
```

**AFTER**:
```cpp
uint64_t current_xid = connection->getCurrentTransactionId();
btree->search(key, current_xid, &tids, ctx);
```

### Testing Requirements

Create test file: `tests/core/btree_mga_test.cpp`

**Test Cases**:
1. Search finds tuples from committed transactions
2. Search doesn't find tuples from active transactions
3. Search doesn't find tuples from aborted transactions
4. Search doesn't find tuples from future transactions
5. RangeScan respects visibility correctly
6. Concurrent transactions see correct isolation
7. UPDATE operations maintain TID stability

### Validation Checklist
- [ ] No Snapshot parameters in btree.h
- [ ] No Snapshot parameters in btree.cpp
- [ ] No isSnapshotVisible() calls in B-tree code
- [ ] isEntryVisible() uses isVersionVisible()
- [ ] All search/rangeScan callers updated
- [ ] All tests pass
- [ ] grep confirms: 0 occurrences of "Snapshot" in btree files

### Documentation Updates
- [ ] Update B-tree header comments to reference Firebird MGA
- [ ] Remove any references to "MVCC" in B-tree code
- [ ] Add references to MGA_RULES.md in comments

---

## 🔧 PHASE 3: Hash Index Fix

**Priority**: 🔴 CRITICAL
**Effort**: 15-20 hours
**Status**: NOT STARTED
**Depends On**: Phase 1 complete

### Goals
1. Add xmin/xmax fields to HashEntry structure
2. Remove Snapshot parameters from Hash index API
3. Implement TIP-based visibility for hash indexes
4. Enable early filtering at index level

### Current Violations

**Audit Reference**: `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md:703-785`

**File**: `include/scratchbird/core/hash_index.h`

**Violation 1 (Lines 114-120)**: Snapshot parameter in find()
```cpp
std::vector<TID> find(const void *key_data, size_t key_len,
                      struct Snapshot *snapshot,  // ❌ WRONG
                      ErrorContext *ctx = nullptr);
```

**Violation 2 (Lines 58-63)**: HashEntry lacks xmin/xmax
```cpp
struct HashEntry
{
    uint64_t he_key_hash;
    uint64_t he_tuple_id;
    // ❌ NO xmin/xmax fields!
} __attribute__((packed));
```

**File**: `src/core/hash_index.cpp`

**Violation 3 (Lines 720-723)**: Snapshot parameter ignored
```cpp
// MVCC filtering: For hash indexes in Firebird MGA, visibility filtering
// is done at the storage layer when fetching tuples
(void)snapshot;  // ❌ Snapshot parameter ignored!
```

**Violation 4**: No TIP lookups (grep confirms: 0 occurrences)

### Required Changes

#### Step 3.1: Add xmin/xmax to HashEntry
**File**: `include/scratchbird/core/hash_index.h:58-63`

**Action**: Add transaction tracking fields

**BEFORE**:
```cpp
struct HashEntry
{
    uint64_t he_key_hash;
    uint64_t he_tuple_id;
} __attribute__((packed));
```

**AFTER**:
```cpp
struct HashEntry
{
    uint64_t he_key_hash;
    uint64_t he_tuple_id;
    uint64_t he_xmin;  // ✅ ADD: Transaction that created entry
    uint64_t he_xmax;  // ✅ ADD: Transaction that deleted entry (0 if active)
} __attribute__((packed));
```

**Impact**: This changes the on-disk format. Requires:
1. Database migration for existing hash indexes
2. Update hash page size calculations
3. Update all HashEntry read/write code

#### Step 3.2: Update Hash Index API
**File**: `include/scratchbird/core/hash_index.h:114-120`

**Action**: Replace Snapshot with TransactionId

**BEFORE**:
```cpp
std::vector<TID> find(const void *key_data, size_t key_len,
                      struct Snapshot *snapshot,
                      ErrorContext *ctx = nullptr);
```

**AFTER**:
```cpp
std::vector<TID> find(const void *key_data, size_t key_len,
                      uint64_t current_xid,  // ✅ CORRECT
                      ErrorContext *ctx = nullptr);
```

#### Step 3.3: Implement Visibility Filtering
**File**: `src/core/hash_index.cpp`

**Action**: Replace snapshot ignore with TIP-based filtering

**BEFORE**:
```cpp
// Line 720-723
(void)snapshot;  // Ignored
```

**AFTER**:
```cpp
// Filter hash entries by visibility
std::vector<TID> visible_tids;
for (const auto& entry : matching_entries) {
    // Check if entry is visible using TIP
    if (txn_mgr->isVersionVisible(entry.he_xmin, current_xid)) {
        // Check if entry has been deleted
        if (entry.he_xmax == 0 ||
            !txn_mgr->isVersionVisible(entry.he_xmax, current_xid)) {
            visible_tids.push_back(TID::fromPacked(entry.he_tuple_id));
        }
    }
}
return visible_tids;
```

#### Step 3.4: Update insert() to Set xmin
**File**: `src/core/hash_index.cpp`

**Action**: Set xmin when creating hash entries

**Add to insert()**:
```cpp
entry.he_xmin = xid;  // Transaction that created this entry
entry.he_xmax = 0;    // Not deleted
```

#### Step 3.5: Update remove() to Set xmax
**File**: `src/core/hash_index.cpp`

**Action**: Set xmax when deleting entries (MGA soft delete)

**Replace physical delete with**:
```cpp
entry.he_xmax = xid;  // Mark as deleted by this transaction
// Do NOT physically remove - let garbage collection handle it
```

### Testing Requirements

Create test file: `tests/core/hash_index_mga_test.cpp`

**Test Cases**:
1. find() respects transaction visibility
2. Concurrent transactions see correct isolation
3. Deleted entries not visible after deletion
4. xmin/xmax correctly set on insert/delete
5. Garbage collection cleans old entries
6. Hash index migration works correctly

### Migration Required

**Warning**: Changing HashEntry structure requires database migration.

Create migration script:
1. Scan all existing hash indexes
2. Read old format (without xmin/xmax)
3. Write new format (with xmin/xmax set to 0/0 for existing entries)
4. Update catalog metadata

### Validation Checklist
- [ ] HashEntry has xmin/xmax fields
- [ ] No Snapshot parameters in hash_index.h
- [ ] No Snapshot parameters in hash_index.cpp
- [ ] find() implements TIP-based filtering
- [ ] insert() sets xmin
- [ ] remove() sets xmax (soft delete)
- [ ] Migration script created and tested
- [ ] All tests pass

---

## 🔧 PHASE 4: Bitmap Index Fix

**Priority**: 🔴 CRITICAL
**Effort**: 20-30 hours
**Status**: NOT STARTED
**Depends On**: Phase 1 complete

### Goals
1. Remove Snapshot parameters from Bitmap index API
2. Replace post-filtering with index-level TIP filtering
3. Eliminate 20-40% performance overhead from heap access
4. Achieve pure Firebird MGA visibility

### Current Violations

**Audit Reference**: `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md:788-867`

**File**: `include/scratchbird/core/bitmap_index.h:160-166`

**Violation 1**: Snapshot parameter
```cpp
std::vector<TID> find(
    const void *value_data,
    size_t value_len,
    struct Snapshot *snapshot,  // ❌ WRONG
    ErrorContext *ctx = nullptr);
```

**File**: `src/core/bitmap_index.cpp`

**Violation 2 (Lines 474-479)**: isSnapshotVisible() calls
```cpp
auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);

bool xmin_visible = txn_manager->isSnapshotVisible(tuple_header->xmin, txn_snapshot);  // ❌ WRONG
bool xmax_visible = (tuple_header->xmax != 0) &&
                    txn_manager->isSnapshotVisible(tuple_header->xmax, txn_snapshot);  // ❌ WRONG
```

**Violation 3 (Lines 412-418)**: Post-filtering overhead
```cpp
// PHASE 1 TASK 1.5: Visibility filter for bitmap index (post-filtering)
// This is a post-filter that checks heap tuple visibility for each TID
// NOTE: This is less efficient than B-Tree/Hash visibility (20-40% overhead)
```

### Required Changes

#### Step 4.1: Update Bitmap Index API
**File**: `include/scratchbird/core/bitmap_index.h:160-166`

**Action**: Replace Snapshot with TransactionId

**BEFORE**:
```cpp
std::vector<TID> find(
    const void *value_data,
    size_t value_len,
    struct Snapshot *snapshot,
    ErrorContext *ctx = nullptr);
```

**AFTER**:
```cpp
std::vector<TID> find(
    const void *value_data,
    size_t value_len,
    uint64_t current_xid,  // ✅ CORRECT
    ErrorContext *ctx = nullptr);
```

#### Step 4.2: Replace Post-Filtering with TIP Filtering
**File**: `src/core/bitmap_index.cpp:412-479`

**Action**: Remove heap access, use TIP lookups instead

**Strategy Decision**:

**Option A**: Store xmin/xmax in bitmap entries (increases storage)
- Pros: No heap access needed, fast filtering
- Cons: Increases bitmap size

**Option B**: Accept post-filtering but use TIP (not snapshots)
- Pros: No storage increase
- Cons: Still has overhead (but correct architecture)

**Recommended**: Option B for now (correctness over performance)

**BEFORE** (PostgreSQL snapshot + heap access):
```cpp
// Access heap page to get tuple header
auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);
bool xmin_visible = txn_manager->isSnapshotVisible(tuple_header->xmin, txn_snapshot);
bool xmax_visible = (tuple_header->xmax != 0) &&
                    txn_manager->isSnapshotVisible(tuple_header->xmax, txn_snapshot);
```

**AFTER** (Firebird MGA TIP):
```cpp
// Access heap page to get tuple header (still needed)
// But use TIP-based visibility (Firebird MGA)
bool xmin_visible = txn_manager->isVersionVisible(tuple_header->xmin, current_xid);
bool xmax_visible = (tuple_header->xmax != 0) &&
                    txn_manager->isVersionVisible(tuple_header->xmax, current_xid);
```

**Note**: Post-filtering overhead remains, but architecture is now correct.

#### Step 4.3: Update Comments
**File**: `src/core/bitmap_index.cpp:412-418`

**Action**: Update comment to reflect MGA (not MVCC)

**BEFORE**:
```cpp
// NOTE: This is less efficient than B-Tree/Hash visibility (20-40% overhead) because:
//       - Full MVCC redesign would require storing xmin/xmax in bitmap entries (deferred to Beta)
```

**AFTER**:
```cpp
// NOTE: This is less efficient than B-Tree/Hash visibility (20-40% overhead) because:
//       - Bitmap returns TIDs directly, not pointers to heap tuples
//       - We must access heap pages to check visibility via TIP (Firebird MGA)
//       - Full optimization would require storing xmin/xmax in bitmap entries (future enhancement)
```

### Testing Requirements

Create test file: `tests/core/bitmap_index_mga_test.cpp`

**Test Cases**:
1. find() uses TIP-based visibility
2. No snapshot structures used
3. Visibility semantics match B-tree
4. Performance overhead measured and acceptable
5. Concurrent transactions isolated correctly

### Validation Checklist
- [ ] No Snapshot parameters in bitmap_index.h
- [ ] No Snapshot parameters in bitmap_index.cpp
- [ ] isSnapshotVisible() replaced with isVersionVisible()
- [ ] Comments updated to reference MGA (not MVCC)
- [ ] All tests pass
- [ ] Performance regression < 5% vs current (if any)

### Future Optimization (Post-ALPHA)
Consider Option A (store xmin/xmax in bitmap entries) for Beta release to eliminate post-filtering overhead.

---

## 🔧 PHASE 5: GIN Index Fix

**Priority**: 🔴 CRITICAL
**Effort**: 30-40 hours
**Status**: NOT STARTED
**Depends On**: Phase 1 complete

### Goals
1. Remove Snapshot parameters from GIN index API
2. Eliminate mixed TIP/snapshot usage (architectural confusion)
3. Ensure consistent Firebird MGA visibility
4. Fix pending list visibility

### Current Violations

**Audit Reference**: `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md:870-967`

**File**: `include/scratchbird/core/gin_index.h:247-251`

**Violation 1**: Snapshot parameter
```cpp
std::vector<TID> find(const void *key_data, size_t key_len,
                      struct Snapshot *snapshot,  // ❌ WRONG
                      ErrorContext *ctx = nullptr);
```

**File**: `src/core/gin_index.cpp`

**Violation 2 (Lines 2217, 2303-2305)**: isSnapshotVisible() calls
```cpp
return db_->transaction_manager()->isSnapshotVisible(xmin,
    reinterpret_cast<const TransactionManager::Snapshot *>(snapshot));  // ❌ WRONG
```

**Violation 3 (Line 2211)**: Mixed TIP and snapshot usage
```cpp
Status status = db_->transaction_manager()->getTransactionState(xmin, state, ctx);  // ✅ TIP
// BUT also calls isSnapshotVisible() elsewhere  // ❌ Snapshot
```

This is **architectural confusion** - using BOTH Firebird MGA and PostgreSQL MVCC in the same component.

### Required Changes

#### Step 5.1: Update GIN Index API
**File**: `include/scratchbird/core/gin_index.h:247-251`

**Action**: Replace Snapshot with TransactionId

**BEFORE**:
```cpp
std::vector<TID> find(const void *key_data, size_t key_len,
                      struct Snapshot *snapshot,
                      ErrorContext *ctx = nullptr);
```

**AFTER**:
```cpp
std::vector<TID> find(const void *key_data, size_t key_len,
                      uint64_t current_xid,  // ✅ CORRECT
                      ErrorContext *ctx = nullptr);
```

#### Step 5.2: Eliminate Mixed TIP/Snapshot Usage
**File**: `src/core/gin_index.cpp`

**Action**: Replace ALL isSnapshotVisible() with isVersionVisible()

**Find**:
```bash
grep -n "isSnapshotVisible" src/core/gin_index.cpp
```

**Replace Each Instance**:

**BEFORE**:
```cpp
bool visible = db_->transaction_manager()->isSnapshotVisible(xmin,
    reinterpret_cast<const TransactionManager::Snapshot *>(snapshot));
```

**AFTER**:
```cpp
bool visible = db_->transaction_manager()->isVersionVisible(xmin, current_xid);
```

#### Step 5.3: Fix Pending List Visibility
**File**: `include/scratchbird/core/gin_index.h:50-56`

**Current (Correct xmin field)**:
```cpp
struct GinPendingEntry
{
    uint64_t tid;
    uint64_t xmin;  // ✅ Correct field
    uint16_t key_len;
    uint8_t key_data[54];
} __attribute__((packed));
```

**Action**: Ensure pending list uses xmin with TIP lookups (not snapshots)

**In pending list scan code**:
```cpp
// When scanning pending list
for (const auto& entry : pending_entries) {
    // Use TIP-based visibility
    if (txn_mgr->isVersionVisible(entry.xmin, current_xid)) {
        // Entry is visible
    }
}
```

#### Step 5.4: Verify Consistent TIP Usage
**Action**: Ensure ALL visibility checks in GIN use TIP

**Checklist**:
- [ ] Entry tree traversal uses TIP
- [ ] Posting list scans use TIP
- [ ] Pending list scans use TIP
- [ ] No mixed TIP/snapshot code paths

### Testing Requirements

Create test file: `tests/core/gin_index_mga_test.cpp`

**Test Cases**:
1. find() uses TIP-based visibility
2. Posting list visibility correct
3. Pending list visibility correct
4. No architectural mixing (TIP only)
5. Full-text search respects transaction isolation
6. Concurrent GIN operations isolated correctly

### Validation Checklist
- [ ] No Snapshot parameters in gin_index.h
- [ ] No Snapshot parameters in gin_index.cpp
- [ ] Zero isSnapshotVisible() calls (grep confirms)
- [ ] All visibility uses isVersionVisible()
- [ ] Pending list uses TIP
- [ ] No mixed TIP/snapshot patterns
- [ ] All tests pass

---

## 🔧 PHASE 6: Advanced Indexes (BRIN, HNSW, R-tree)

**Priority**: 🟠 HIGH (if implemented) / 🟡 LOW (if stubs)
**Effort**: 35-60 hours
**Status**: NOT STARTED
**Depends On**: Phase 1 complete

### Goals
1. Fix BRIN index if implemented
2. Fix HNSW index if implemented
3. Fix R-tree index
4. Ensure all advanced indexes use Firebird MGA

### Current Status

**BRIN Index**:
- Header declares Snapshot parameters
- No implementation file (`src/core/brin_index.cpp` does not exist)
- Status: **PARTIAL** - API designed but not implemented

**HNSW Index**:
- Header declares Snapshot parameters
- Implementation is stubs only (returns NOT_IMPLEMENTED)
- Status: **STUB** - Not functional

**R-tree Index**:
- Header declares Snapshot parameters
- Implementation exists but no visibility calls found
- Status: **PARTIAL** - Visibility not implemented

### Decision Point

**Question**: Are these indexes required for ALPHA?

**If NO** (recommended):
- Fix only API signatures (remove Snapshot parameters)
- Update comments to reference Firebird MGA
- Defer full implementation to post-ALPHA
- **Effort**: 10-15 hours (API changes only)

**If YES**:
- Complete full implementation with TIP-based visibility
- **Effort**: 35-60 hours (full implementation)

### Required Changes (Minimal - API Only)

#### Step 6.1: BRIN Index API Update
**File**: `include/scratchbird/core/brin_index.h:239-243`

**BEFORE**:
```cpp
Status scan(const std::vector<uint8_t> *min_value,
            const std::vector<uint8_t> *max_value,
            struct Snapshot *snapshot,  // ❌ WRONG
            std::vector<uint32_t> *block_numbers_out,
            ErrorContext *ctx = nullptr);
```

**AFTER**:
```cpp
Status scan(const std::vector<uint8_t> *min_value,
            const std::vector<uint8_t> *max_value,
            uint64_t current_xid,  // ✅ CORRECT
            std::vector<uint32_t> *block_numbers_out,
            ErrorContext *ctx = nullptr);
```

#### Step 6.2: HNSW Index API Update
**File**: `include/scratchbird/core/hnsw_index.h:284-288`

**BEFORE**:
```cpp
Status search(const VectorValue &query_vector,
              uint32_t k,
              struct Snapshot *snapshot,  // ❌ WRONG
              std::vector<HnswSearchResult> *results_out,
              ErrorContext *ctx = nullptr);
```

**AFTER**:
```cpp
Status search(const VectorValue &query_vector,
              uint32_t k,
              uint64_t current_xid,  // ✅ CORRECT
              std::vector<HnswSearchResult> *results_out,
              ErrorContext *ctx = nullptr);
```

#### Step 6.3: R-tree API Update
**File**: `include/scratchbird/core/rtree.h:269-272, 283-286`

**BEFORE**:
```cpp
Status insert(const BoundingBox& bbox,
             const TID& tid,
             struct Snapshot* snapshot,  // ❌ WRONG
             ErrorContext* ctx = nullptr);

Status search(const BoundingBox& bbox,
             struct Snapshot* snapshot,  // ❌ WRONG
             std::vector<TID>* tids_out,
             ErrorContext* ctx = nullptr);
```

**AFTER**:
```cpp
Status insert(const BoundingBox& bbox,
             const TID& tid,
             uint64_t current_xid,  // ✅ CORRECT
             ErrorContext* ctx = nullptr);

Status search(const BoundingBox& bbox,
             uint64_t current_xid,  // ✅ CORRECT
             std::vector<TID>* tids_out,
             ErrorContext* ctx = nullptr);
```

#### Step 6.4: Update Implementation Stubs
**For Each Index**:

If implementation returns NOT_IMPLEMENTED:
- Update function signature to match new API
- No visibility logic needed (not implemented yet)

If implementation exists:
- Update to use isVersionVisible() when implemented

### Validation Checklist
- [ ] No Snapshot parameters in brin_index.h
- [ ] No Snapshot parameters in hnsw_index.h
- [ ] No Snapshot parameters in rtree.h
- [ ] Function signatures updated
- [ ] Comments reference Firebird MGA
- [ ] Stub implementations compile

---

## 🔧 PHASE 7: Testing & Validation

**Priority**: 🔴 CRITICAL
**Effort**: 20-30 hours
**Status**: NOT STARTED
**Depends On**: Phases 1-6 complete

### Goals
1. Comprehensive MGA compliance testing
2. Regression testing (ensure nothing broken)
3. Performance validation
4. Documentation updates

### Test Categories

#### 7.1 Unit Tests
**Location**: `tests/core/`

**Files to Create/Update**:
- `transaction_manager_mga_test.cpp` (Phase 1)
- `btree_mga_test.cpp` (Phase 2)
- `hash_index_mga_test.cpp` (Phase 3)
- `bitmap_index_mga_test.cpp` (Phase 4)
- `gin_index_mga_test.cpp` (Phase 5)

**Test Coverage Requirements**: >90% for MGA code paths

#### 7.2 Integration Tests
**Location**: `tests/integration/`

**Test Scenarios**:
1. Multi-index query with MGA visibility
2. Concurrent transactions with different isolation levels
3. Long-running transaction with snapshot stability
4. Transaction abort and rollback visibility
5. Garbage collection with active transactions

#### 7.3 Compliance Validation
**Action**: Verify against MGA_RULES.md

**Checklist** (from MGA_RULES.md Rule 13):
- [ ] TIP implementation present
- [ ] getTransactionState() calls found
- [ ] TxState enum used
- [ ] OIT/OAT/OST markers tracked
- [ ] Back pointers (new → old)
- [ ] In-place updates
- [ ] Stable TIDs
- [ ] "Back version" terminology used

**Contamination Check**:
```bash
# Should return 0 results
grep -r "Snapshot\*" --include="*.cpp" --include="*.h" src/ include/
grep -r "isSnapshotVisible" --include="*.cpp" src/
grep -r "getSnapshot" --include="*.cpp" src/
```

#### 7.4 Performance Testing
**Benchmarks**:
1. Index search performance (TIP vs old snapshot)
2. Concurrent transaction throughput
3. Long-running transaction overhead
4. Garbage collection efficiency

**Acceptance Criteria**:
- Performance regression < 5% vs baseline
- TIP lookups faster than snapshot array search
- No deadlocks or race conditions

#### 7.5 Documentation Updates

**Files to Update**:
1. `/README.md` - Mention Firebird MGA compliance
2. `/docs/specifications/MGA_IMPLEMENTATION.md` - Update implementation status
3. All index header files - Update comments
4. `/CHANGELOG.md` - Document MGA compliance achievement

**Remove**:
- All references to "PostgreSQL MVCC"
- All references to "snapshot-based visibility"
- All mentions of "active transaction arrays"

**Add**:
- References to "Firebird MGA"
- References to "TIP-based visibility"
- Links to /MGA_RULES.md

### Final Validation Commands

```bash
# 1. Check for Snapshot contamination (should be 0)
grep -r "struct Snapshot" --include="*.h" include/
grep -r "Snapshot\*" --include="*.cpp" --include="*.h" src/ include/

# 2. Check for isSnapshotVisible (should be 0)
grep -r "isSnapshotVisible" --include="*.cpp" src/

# 3. Check for TIP usage (should be many)
grep -r "getTransactionState" --include="*.cpp" src/

# 4. Check for isVersionVisible (should be many)
grep -r "isVersionVisible" --include="*.cpp" src/

# 5. Verify no MVCC references in new code
grep -r "MVCC" --include="*.cpp" --include="*.h" src/ include/ | grep -v "// OLD:"
```

### Validation Checklist
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] Compliance validation passes
- [ ] Performance acceptable
- [ ] Documentation updated
- [ ] Zero Snapshot contamination
- [ ] grep validations pass

---

## 📚 REFERENCE LINKS

### Mandatory Reading (Read EVERY Time Before Working)
- **MGA Rules**: `/MGA_RULES.md` (15 absolute rules)
- **Audit Report**: `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md` (all violations)
- **MGA Spec**: `/docs/specifications/MGA_IMPLEMENTATION.md`
- **Firebird Spec**: `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`

### Analysis & History
- **Contamination Analysis**: `/docs/analysis/CRITICAL_MGA_MVCC_CONFUSION_ANALYSIS.md`
- **Previous Fixes**: `/docs/analysis/MGA_RULES_IMPLEMENTATION_SUMMARY.md`

### Executive Summary
- **ALPHA Roadmap**: `/docs/audit/07_ALPHA_COMPLETION_ROADMAP.md`
- **Executive Summary**: `/docs/audit/00_EXECUTIVE_SUMMARY.md`

---

## ⚠️ CRITICAL REMINDERS

### Before Starting ANY Work

1. **READ** `/MGA_RULES.md` in full (650 lines - mandatory)
2. **REVIEW** the relevant section in `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md`
3. **UNDERSTAND** why snapshots are wrong (Rule 0, Rule 1, Rule 3)
4. **VERIFY** you understand TIP-based visibility (Rule 2, Rule 3)

### During Work

1. **NO SNAPSHOTS** - If you see `Snapshot` anywhere, it's wrong
2. **USE TIP** - All visibility must use `getTransactionState()`
3. **FIREBIRD MGA** - Not PostgreSQL MVCC, not custom, pure Firebird
4. **ASK IF UNSURE** - When in doubt, re-read MGA_RULES.md

### After Work

1. **RUN GREP CHECKS** - Ensure zero Snapshot contamination
2. **RUN TESTS** - All tests must pass
3. **UPDATE PROGRESS** - Mark tasks complete in this plan
4. **COMMIT WITH REFERENCE** - Link to this plan in commit messages

---

## 🎯 SUCCESS METRICS

### Definition of Done (100% MGA Compliance)

- ✅ Zero `Snapshot` structures in codebase
- ✅ Zero `isSnapshotVisible()` calls
- ✅ Zero `getSnapshot()` calls
- ✅ All indexes use `getTransactionState()` (TIP)
- ✅ All indexes use `isVersionVisible()`
- ✅ All index APIs use `TransactionId current_xid`
- ✅ All tests pass (unit + integration)
- ✅ Performance acceptable (< 5% regression)
- ✅ Documentation updated
- ✅ Audit report re-run shows 100% compliance

### How to Verify 100% Compliance

**Command**:
```bash
cd /home/dcalford/CliWork/ScratchBird
./scripts/verify_mga_compliance.sh
```

**Expected Output**:
```
MGA Compliance Check
====================
✅ No Snapshot structures found
✅ No isSnapshotVisible() calls found
✅ No getSnapshot() calls found
✅ TIP usage confirmed (X occurrences)
✅ isVersionVisible() usage confirmed (X occurrences)
✅ All tests pass (XXX/XXX)

Result: 100% FIREBIRD MGA COMPLIANT
```

---

## 📝 COMMIT MESSAGE FORMAT

When committing MGA compliance fixes, use this format:

```
MGA: [Component] Brief description

Fixes PostgreSQL MVCC contamination in [component].
Replaces snapshot-based visibility with TIP-based visibility (Firebird MGA).

Changes:
- Remove Snapshot parameters from API
- Replace isSnapshotVisible() with isVersionVisible()
- Add TIP lookups via getTransactionState()

Ref: /docs/planning/MGA_COMPLIANCE_FIX_PLAN.md Phase X
Ref: /docs/audit/01_MGA_COMPLIANCE_AUDIT.md (lines XXX-XXX)
Ref: /MGA_RULES.md Rule X

Tests: [test file names]
```

**Example**:
```
MGA: [B-tree] Remove PostgreSQL MVCC snapshot-based visibility

Fixes PostgreSQL MVCC contamination in B-tree index.
Replaces snapshot-based visibility with TIP-based visibility (Firebird MGA).

Changes:
- Remove Snapshot* parameters from search() and rangeScan()
- Replace isSnapshotVisible() with isVersionVisible() in isEntryVisible()
- Add TIP lookups via getTransactionState()
- Update all callers to pass TransactionId instead of Snapshot

Ref: /docs/planning/MGA_COMPLIANCE_FIX_PLAN.md Phase 2
Ref: /docs/audit/01_MGA_COMPLIANCE_AUDIT.md (lines 551-700)
Ref: /MGA_RULES.md Rule 1, Rule 3

Tests: tests/core/btree_mga_test.cpp
```

---

## 🚨 AFTER MEMORY COMPACTION

**IF THIS SESSION IS SUMMARIZED AND YOU RETURN:**

1. **IMMEDIATELY READ**:
   - `/MGA_RULES.md` (650 lines)
   - `/docs/planning/MGA_COMPLIANCE_FIX_PLAN.md` (this file)
   - `/docs/audit/01_MGA_COMPLIANCE_AUDIT.md` (relevant phase)

2. **VERIFY UNDERSTANDING**:
   - Can you explain why `Snapshot` is wrong?
   - Can you explain TIP-based visibility?
   - Can you explain Firebird MGA vs PostgreSQL MVCC?

3. **CHECK CURRENT PROGRESS**:
   - Review phase completion status in this file
   - Run grep checks to see what's been fixed
   - Continue from last incomplete phase

4. **REMEMBER THE RULE**:
   - **If you see `Snapshot` in transaction code, it's WRONG**
   - **ScratchBird uses Firebird MGA (TIP), NOT PostgreSQL MVCC (snapshots)**

---

**Plan Created**: November 2, 2025
**Plan Status**: ACTIVE
**Next Action**: Begin Phase 1 - TransactionManager API Redesign
**Target Completion**: TBD (150-220 hours estimated)
