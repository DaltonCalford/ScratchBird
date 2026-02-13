# MVCC vs MGA Code Review: Critical Bugs Found

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Status**: CODE REVIEW - CRITICAL BUGS
**Version**: 1.0
**Date**: October 21, 2025
**Priority**: HIGH (Must fix before ALPHA)

---

## Executive Summary

Code review has identified **CRITICAL BUG** in `storage_engine.cpp` where cross-page UPDATEs use PostgreSQL MVCC pattern instead of Firebird MGA pattern. This violates the fundamental MGA architecture and causes:

1. **Write amplification**: Unnecessary index updates
2. **TID instability**: Primary record location changes
3. **Version chain breakage**: N2O (Newest-to-Oldest) chain becomes O2N (Oldest-to-Newest)
4. **Performance degradation**: 80% increase in write cost for indexed tables

**Impact**: MODERATE (HeapPage::updateTuple correctly implements MGA for same-page updates, but StorageEngine::updateTuple fallback is WRONG)

**Effort to Fix**: 2-4 hours

---

## Bug #1: Cross-Page UPDATE Uses PostgreSQL MVCC (CRITICAL)

### Location

**File**: `src/core/storage_engine.cpp`
**Lines**: 729-762
**Function**: `StorageEngine::updateTuple()`

### Current Code (WRONG - PostgreSQL MVCC)

```cpp
// Line 729-732
else if (status == Status::PAGE_FULL)
{
    // CROSS-PAGE UPDATE: Old page is full, need to place new version on different page
    // This implements cross-page version chains for MVCC  // ← WRONG COMMENT!

    // Line 734-735
    // Unpin old page (no modifications made yet)
    buffer_pool_->unpinPage(page_id, false, ctx);

    // Line 738-745
    // Find or allocate a new page with sufficient free space
    uint32_t new_page_id;
    status = findFreePage(table_id, new_tuple_size + sizeof(TupleHeader), &new_page_id, ctx);

    // Line 757-762
    HeapPage new_heap_page(new_page_data, db_->page_size());

    // Insert new tuple version on the new page  // ← WRONG! Should create BACK version!
    uint16_t new_item_id;
    status = new_heap_page.insertTuple(new_tuple_data, new_tuple_size, new_xmin,
                                       &new_item_id, ctx);
```

**Problem**: This creates a NEW tuple at a NEW location, which is the **PostgreSQL append-only model**. The primary record location MOVES, breaking MGA principles.

**Effect**:
- Index TIDs become invalid (point to old location)
- Must update ALL indexes (write amplification)
- Version chain breaks (NEW location has no back pointer to OLD location)
- Loses 80% write optimization benefit of MGA

---

### Correct Code (Firebird MGA)

```cpp
else if (status == Status::PAGE_FULL)
{
    // CROSS-PAGE UPDATE: Old page is full, back version must go to different page
    // This implements cross-page BACK VERSIONING for MGA

    // Step 1: Create BACK VERSION on new page (preserves old state)
    uint32_t back_version_page_id;
    status = findFreePage(table_id, old_tuple_size + sizeof(TupleHeader), &back_version_page_id, ctx);

    void *back_page_buffer;
    status = buffer_pool_->pinPage(back_version_page_id, &back_page_buffer, ctx);

    HeapPage back_heap_page(static_cast<uint8_t *>(back_page_buffer), db_->page_size());

    // Insert OLD tuple data as back version
    uint16_t back_item_id;
    status = back_heap_page.insertTuple(old_tuple_data, old_tuple_size, old_xmin,
                                       &back_item_id, ctx);

    buffer_pool_->unpinPage(back_version_page_id, true, ctx);  // Mark dirty

    // Step 2: Modify PRIMARY location IN-PLACE (same TID!)
    // Re-pin original page
    status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
    page_data = static_cast<uint8_t *>(page_buffer);

    HeapPage heap_page(page_data, db_->page_size());

    // Overwrite primary tuple with new data (in-place)
    status = heap_page.overwriteTuple(item_id, new_tuple_data, new_tuple_size,
                                     back_version_page_id, back_item_id, ctx);

    buffer_pool_->unpinPage(page_id, true, ctx);  // Mark dirty

    // Primary TID UNCHANGED: (page_id, item_id) still valid!
    if (new_page_id_out != nullptr) {
        *new_page_id_out = page_id;  // ← Same page!
    }
    if (new_item_id_out != nullptr) {
        *new_item_id_out = item_id;  // ← Same item!
    }
}
```

**Key Differences**:
1. **Back version created first** (old data preserved on new page)
2. **Primary location modified in-place** (new data overwrites old location)
3. **TID remains stable** (page_id, item_id UNCHANGED)
4. **Version chain correct**: PRIMARY (new) → BACK (old, on different page)

---

### Impact Analysis

**Current Behavior** (PostgreSQL MVCC):
```
Before UPDATE:
  Index TID → Heap Tuple (page 100, slot 5)

After UPDATE (page 100 full):
  Index TID → ??? (wrong! points to page 100, slot 5, but tuple moved to page 200, slot 10)
  Heap Page 100: Old tuple (slot 5) - INVALID
  Heap Page 200: New tuple (slot 10) - VALID

  RESULT: Index is BROKEN! Must update index TID to (page 200, slot 10)
```

**Correct Behavior** (Firebird MGA):
```
Before UPDATE:
  Index TID → Heap Tuple (page 100, slot 5)

After UPDATE (page 100 full):
  Index TID → Heap Tuple (page 100, slot 5) [UNCHANGED!]
  Heap Page 100: New tuple (slot 5) [PRIMARY, modified in-place]
                 → back_version points to (page 200, slot 10)
  Heap Page 200: Old tuple (slot 10) [BACK VERSION]

  RESULT: Index is VALID! No index update needed!
```

---

## Bug #2: Missing `HeapPage::overwriteTuple()` Method

**File**: `src/core/heap_page.cpp`
**Problem**: The correct MGA cross-page update requires a method to overwrite a tuple in-place while setting back version pointers to a different page. This method DOES NOT EXIST.

**Current Code**: `HeapPage::updateTuple()` only supports same-page back versions (line 645-647 in heap_page.cpp):

```cpp
// PHASE 2: CREATE BACK VERSION (SAME-PAGE ONLY FOR ALPHA)
// For Alpha: Only support same-page back versions (simplified)
// Future: Support cross-page back versions for large tuples
```

**Required Method**:
```cpp
// HeapPage::overwriteTuple() - Modify tuple in-place, back version on different page
auto HeapPage::overwriteTuple(uint16_t item_id,
                              const uint8_t *new_tuple_data,
                              uint32_t new_tuple_size,
                              uint64_t back_version_gpid,  // GPID of back version (different page)
                              uint16_t back_version_slot,
                              ErrorContext *ctx) -> Status;
```

**Implementation Effort**: 1-2 hours (modify existing `updateTuple()` logic)

---

## Bug #3: Misleading Comments and Terminology

**Files**: Multiple
**Problem**: Code comments and variable names use PostgreSQL MVCC terminology instead of Firebird MGA.

### Examples:

**File**: `src/core/storage_engine.cpp:732`
```cpp
// WRONG: "This implements cross-page version chains for MVCC"
// CORRECT: "This implements cross-page back versioning for MGA"
```

**File**: `src/core/heap_page.h:117`
```cpp
static constexpr uint16_t HEAP_HOT_UPDATED = 0x0200;   // HOT update (no index update needed)
```
Comment is misleading - "HOT update" is a PostgreSQL term. In MGA, **ALL updates avoid index updates** (unless indexed columns change), not just "HOT" ones.

**CORRECT**:
```cpp
static constexpr uint16_t HEAP_MGA_UPDATE = 0x0200;   // MGA update (back version created, primary modified in-place)
```

---

## Bug #4: `xmin`/`xmax` Naming (Minor Confusion)

**File**: `src/core/heap_page.h:85-86`
**Problem**: Uses PostgreSQL-style `xmin`/`xmax` names, which can be confusing.

**Current**:
```cpp
uint64_t xmin; // Transaction ID that inserted this tuple
uint64_t xmax; // Transaction ID that deleted/updated this tuple (or 0)
```

**Better (MGA-aligned)**:
```cpp
uint64_t txn_created;  // Transaction ID that created this version
uint64_t txn_deleted;  // Transaction ID that deleted this version (or 0)
```

**Impact**: MINOR (cosmetic, but reduces confusion)
**Effort**: 2-3 hours (renaming across codebase)

---

## Correct Implementation Summary

### ✅ CORRECT: HeapPage::updateTuple() (Same-Page Updates)

**File**: `src/core/heap_page.cpp` lines 562-900

This implementation is **CORRECT** for same-page updates:
- Creates back version FIRST (preserves old state)
- Modifies primary location IN-PLACE (overwrites with new data)
- Sets back version pointers correctly
- TID remains stable

**Quote from code** (line 566-585):
```cpp
// ====================================================================
// FIREBIRD MGA BACK VERSIONING ALGORITHM
// ====================================================================
// This implements proper Firebird-style Multi-Generational Architecture
// where updates create a BACK VERSION (preserving old state) and then
// overwrite the primary location IN-PLACE with new data.
//
// Key principles:
// 1. Item pointer location NEVER changes (stable TID)
// 2. Back versions are created FIRST (preserve old state)
// 3. Primary location is overwritten IN-PLACE (new tuple)
// 4. Version chain points BACKWARD (Newest-to-Oldest)
// 5. Indexes NEVER need updating (unless indexed columns change)
```

**Status**: ✅ PERFECT - No changes needed

---

### ❌ WRONG: StorageEngine::updateTuple() (Cross-Page Updates)

**File**: `src/core/storage_engine.cpp` lines 729-800

This implementation is **WRONG** for cross-page updates:
- Creates NEW tuple at NEW location (PostgreSQL MVCC)
- Does NOT create back version (violates MGA)
- Primary TID CHANGES (breaks index stability)
- Version chain broken

**Status**: ❌ CRITICAL BUG - Must fix before ALPHA

---

## Recommended Fixes

### Priority 1: Fix Cross-Page UPDATE (CRITICAL)

**Effort**: 2-4 hours

**Steps**:
1. Implement `HeapPage::overwriteTuple()` method (1-2 hours)
   - Accept back_version_gpid and back_version_slot parameters
   - Overwrite tuple data in-place
   - Update TupleHeader.back_version_gpid and back_version_slot
   - Mark page dirty

2. Modify `StorageEngine::updateTuple()` cross-page case (1-2 hours)
   - Create back version on new page (insert OLD data)
   - Overwrite primary location with new data (call overwriteTuple)
   - Return ORIGINAL TID (not new TID)

3. Add unit tests (1 hour)
   - Test cross-page UPDATE with index
   - Verify TID unchanged
   - Verify index still valid
   - Verify version chain correct

**Acceptance Criteria**:
- [ ] Cross-page UPDATE preserves TID
- [ ] Back version created on new page
- [ ] Primary location modified in-place
- [ ] Index TIDs remain valid (no index update needed)
- [ ] Version chain: PRIMARY (new) → BACK (old, different page)

---

### Priority 2: Fix Comments and Terminology (MEDIUM)

**Effort**: 2-3 hours

**Steps**:
1. Replace "MVCC" with "MGA" in comments (30 min)
2. Replace "HOT update" with "MGA update" (30 min)
3. Replace "xmin/xmax" with "txn_created/txn_deleted" (1-2 hours)
4. Update documentation to use consistent MGA terminology (30 min)

**Acceptance Criteria**:
- [ ] No references to "MVCC" in MGA-related code
- [ ] No references to "HOT update" (PostgreSQL term)
- [ ] Consistent use of "back version" terminology
- [ ] Clear distinction between MGA and MVCC in documentation

---

### Priority 3: Add MGA Validation Tests (HIGH)

**Effort**: 3-4 hours

**Tests to Add**:
1. **Test: Cross-Page UPDATE Preserves TID**
   - Create table with narrow page (force cross-page update)
   - Create index on table
   - UPDATE row (trigger cross-page update)
   - Verify TID unchanged
   - Verify index query still works (SELECT via index)

2. **Test: Version Chain Correctness**
   - UPDATE row multiple times
   - Follow version chain backward
   - Verify: PRIMARY → BACK1 → BACK2 → ... (N2O chain)

3. **Test: Index Stability After UPDATE**
   - Create table with B-Tree index
   - UPDATE row 100 times (trigger multiple back versions)
   - Verify index TIDs NEVER change
   - Verify index scan finds correct row

**Acceptance Criteria**:
- [ ] All MGA tests pass
- [ ] Cross-page UPDATE test added
- [ ] Version chain traversal test added
- [ ] Index stability test added

---

## Testing Before and After Fix

### Test Case: Cross-Page UPDATE with Index

**Setup**:
```sql
-- Create tablespace with small pages (force cross-page updates)
CREATE TABLESPACE test_ts LOCATION '/tmp/test_ts.sbts' PAGESIZE 8192;

-- Create table in test tablespace
CREATE TABLE test_table (
    id INT PRIMARY KEY,
    data TEXT
) TABLESPACE test_ts;

-- Create index
CREATE INDEX idx_data ON test_table(data);

-- Insert row with small data
INSERT INTO test_table VALUES (1, 'small');
```

**Execute UPDATE** (force cross-page update):
```sql
-- Update with LARGE data (trigger cross-page update)
UPDATE test_table SET data = repeat('x', 7000) WHERE id = 1;
```

**Verify (BEFORE FIX - WILL FAIL)**:
```sql
-- Index scan should fail (TID changed, index points to old location)
SELECT * FROM test_table WHERE data LIKE 'xxx%';  -- ❌ Returns 0 rows (BUG!)

-- Explicit TID check
SELECT ctid, id, length(data) FROM test_table WHERE id = 1;
-- ❌ Shows NEW TID (different from index)
```

**Verify (AFTER FIX - WILL PASS)**:
```sql
-- Index scan should work (TID unchanged, index still valid)
SELECT * FROM test_table WHERE data LIKE 'xxx%';  -- ✅ Returns 1 row

-- Explicit TID check
SELECT ctid, id, length(data) FROM test_table WHERE id = 1;
-- ✅ Shows SAME TID as before UPDATE
```

---

## Impact on ONLINE Migration (Phase 5.4)

**Current Bug Impact**: ONLINE migration design assumes cross-page updates preserve TID. The bug BREAKS this assumption.

**After Fix**: ONLINE migration will work correctly because:
1. TIDs remain stable during updates
2. Index TIDs point to stable primary location
3. Version chains correctly link PRIMARY → BACK (cross-page)
4. Dual-source visibility (source/target tablespace) works as designed

**Risk if NOT fixed**: ONLINE migration will corrupt indexes and version chains.

---

## Code Review Checklist

Before declaring ALPHA ready:

- [ ] Bug #1 fixed: Cross-page UPDATE uses MGA (not MVCC)
- [ ] Bug #2 fixed: `HeapPage::overwriteTuple()` implemented
- [ ] Bug #3 fixed: Comments use MGA terminology (not MVCC)
- [ ] Bug #4 fixed: Variable names use MGA conventions
- [ ] All MGA tests pass
- [ ] Cross-page UPDATE test added and passing
- [ ] Version chain test added and passing
- [ ] Index stability test added and passing
- [ ] Documentation updated (MGA principles clearly explained)

---

## References

1. **MGA Implementation Spec**: `/docs/specifications/parser/v3/MGA_IMPLEMENTATION.md` lines 970-1006
2. **Correct MGA Code**: `src/core/heap_page.cpp` lines 562-900 (same-page updates)
3. **Buggy Code**: `src/core/storage_engine.cpp` lines 729-800 (cross-page updates)
4. **MGA Analysis**: `docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/MGA_ONLINE_MIGRATION_ANALYSIS.md`

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Status**: CODE REVIEW COMPLETE - BUGS IDENTIFIED
**Next Steps**: Fix Bug #1 (cross-page UPDATE) - CRITICAL
