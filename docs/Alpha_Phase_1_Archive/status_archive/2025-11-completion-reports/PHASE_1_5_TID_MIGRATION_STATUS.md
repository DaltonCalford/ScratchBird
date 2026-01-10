# Phase 1.5: TID Migration Status Report

**Date**: 2025-10-20
**Status**: PARTIALLY COMPLETE (~20%)
**Recommendation**: Complete remaining work before Phase 2

---

## Executive Summary

Phase 1.5 (TID Migration to GPID-based Format) is approximately **20% complete**. The foundational work is done (heap layer fully migrated), but significant work remains to migrate all index types, StorageEngine, and GarbageCollector APIs.

### Completion Status

| Component | Status | Estimated Hours | Notes |
|-----------|--------|----------------|-------|
| **✅ COMPLETE** |
| Heap Layer (Task 1.5.1) | ✅ 100% | 6h (actual) | TupleHeader migrated to GPID, all heap operations updated |
| TID Infrastructure | ✅ 100% | ~2h | Complete TID struct with all helpers in tid.h |
| **❌ INCOMPLETE** |
| B-Tree Index | 🟡 10% | 3-4h remaining | Header updated, implementation needs ~100+ changes |
| Hash Index | ❌ 0% | 3-4h | Not started |
| GIN Index | ❌ 0% | 4-5h | Not started |
| Bitmap Index | ❌ 0% | 2-3h | Not started |
| BRIN Index | ❌ 0% | 2-3h | Not started |
| HNSW Index | ❌ 0% | 2-3h | Not started |
| StorageEngine | ❌ 0% | 3-4h | Not started |
| GarbageCollector | ❌ 0% | 2-3h | Not started |
| Test Suite | ❌ 0% | 3-5h | Not started |
| Migration Tool | ❌ 0% | 2-3h | Not started |
| **TOTAL** | **20%** | **30-40h** | **6h done, 24-34h remaining** |

---

## ✅ Completed Work

### TASK 1.5.1: Heap Layer TID Migration - COMPLETE

**Files Modified**:
- ✅ `include/scratchbird/core/tid.h` (242 lines) - Complete TID struct infrastructure
- ✅ `include/scratchbird/core/heap_page.h` - TupleHeader expanded from 36 to 44 bytes
- ✅ `src/core/heap_page.cpp` - All tuple operations use GPID-based TID

**Key Changes**:
1. **TupleHeader Breaking Change** (44 bytes, up from 36):
   ```cpp
   // OLD (36 bytes):
   uint64_t xmin, xmax;
   uint64_t back_version_tid;  // 64-bit legacy encoding
   uint32_t ctid_page;         // 32-bit page ID
   uint16_t ctid_item;         // 16-bit slot

   // NEW (44 bytes):
   uint64_t xmin, xmax;
   GPID back_version_gpid;     // 64-bit GPID
   uint16_t back_version_slot; // 16-bit slot
   uint16_t reserved1;         // Padding
   GPID ctid_gpid;             // 64-bit GPID
   uint16_t ctid_slot;         // 16-bit slot
   ```

2. **New TID Methods**:
   ```cpp
   TID getTID() const;                      // Returns TID struct
   void setTID(GPID, uint16_t);            // Set from components
   void setTID(const TID&);                // Set from struct
   TID getBackVersionTID() const;          // Returns TID struct
   void setBackVersionTID(const TID&);     // Set back version
   ```

3. **Heap Operations Updated**:
   - `insertTuple()` uses `setTID()` with GPID (heap_page.cpp:242)
   - Version chain traversal uses `getBackVersionTID()` (lines 1112, 1284)
   - HOT updates use `setTID()` with GPID (line 903)
   - Cross-page version chains in storage_engine.cpp updated

**Verification**: ✅ Core library compiles successfully

---

## 🟡 Partially Complete Work

### B-Tree Index - 10% Complete

**Header Updates** ✅:
- `include/scratchbird/core/btree.h:9` - Added `#include "scratchbird/core/tid.h"`
- `include/scratchbird/core/btree.h:173` - `insert(const std::vector<uint8_t> &key, const TID &tid, ...)`
- `include/scratchbird/core/btree.h:182` - `search(..., std::vector<TID> *tids_out, ...)`
- `include/scratchbird/core/btree.h:186` - `remove(const std::vector<uint8_t> &key, const TID &tid, ...)`
- `include/scratchbird/core/btree.h:214` - `removeDeadEntries(const std::vector<TID> &dead_tids, ...)`
- `include/scratchbird/core/btree.h:257` - `searchPage(..., std::vector<TID> *tids_out)`
- `include/scratchbird/core/btree.h:262` - `split_leaf_page(..., const TID &new_tid, ...)`
- `include/scratchbird/core/btree.h:302` - `BTreeIterator::next(..., TID *tid_out, ...)`

**Implementation Updates** 🟡 (Started):
- `src/core/btree.cpp:307` - `insert()` signature updated, uses `convertTIDtoLegacy()` for Tuple struct
- `src/core/btree.cpp:437` - `searchPage()` signature needs update
- Many more locations need updating (see below)

**Remaining Work** (estimated 3-4 hours):
1. Update `searchPage()` implementation (~30 locations)
2. Update `search()` method
3. Update `remove()` method
4. Update `split_leaf_page()` implementation
5. Update `removeDeadEntries()` implementation
6. Update `BTreeIterator::next()` implementation
7. Update all internal helper methods that handle tuple IDs
8. Convert all `uint64_t tuple_id` to `TID` throughout ~2395 line file

---

## ❌ Incomplete Work

### TASK 1.5.2: Index Layer TID Migration - 0-10% Complete

#### Hash Index (hash_index.h/cpp) - 0% Complete

**Estimated Work**: 3-4 hours

**API Changes Needed**:
```cpp
// Current signatures:
Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id, ...);
Status find(const std::vector<uint8_t> &key, std::vector<uint64_t> *tuple_ids_out, ...);
Status remove(const std::vector<uint8_t> &key, uint64_t tuple_id, ...);
Status removeDeadEntries(const std::vector<uint64_t> &dead_tids, ...);

// New signatures:
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status find(const std::vector<uint8_t> &key, std::vector<TID> *tids_out, ...);
Status remove(const std::vector<uint8_t> &key, const TID &tid, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...);
```

**Files**:
- `include/scratchbird/core/hash_index.h` - Add tid.h include, update 4 method signatures
- `src/core/hash_index.cpp` - Update ~80+ lines of implementation

**On-Disk Format**: Hash buckets store TIDs, may need format update

#### GIN Index (gin_index.h/cpp) - 0% Complete

**Estimated Work**: 4-5 hours

**API Changes Needed**:
```cpp
// Posting lists store TIDs - on-disk format may need update
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status find(const std::vector<uint8_t> &key, std::vector<TID> *tids_out, ...);
Status remove(const std::vector<uint8_t> &key, const TID &tid, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...);
```

**Files**:
- `include/scratchbird/core/gin_index.h` - Update 4 method signatures
- `src/core/gin_index.cpp` - Update ~80+ lines
- **Critical**: Posting tree TID storage (on-disk) may need format update
- **Critical**: Posting list compression may be TID-based

#### Bitmap Index (bitmap_index.h/cpp) - 0% Complete

**Estimated Work**: 2-3 hours

**API Changes Needed**:
```cpp
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status find(const std::vector<uint8_t> &key, std::vector<TID> *tids_out, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...);
```

**Files**:
- `include/scratchbird/core/bitmap_index.h` - Update 3 method signatures
- `src/core/bitmap_index.cpp` - Update TID tracking logic

#### BRIN Index (brin_index.h/cpp) - 0% Complete

**Estimated Work**: 2-3 hours

**API Changes Needed**:
- Update to use GPID for block references
- Update `scan()` to return GPIDs instead of page_ids

**Files**:
- `include/scratchbird/core/brin_index.h` - Update signatures
- `src/core/brin_index.cpp` - Update implementation

#### HNSW Index (hnsw_index.h/cpp) - 0% Complete

**Estimated Work**: 2-3 hours

**API Changes Needed**:
```cpp
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status search(..., std::vector<TID> *tids_out, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...);
```

**Files**:
- `include/scratchbird/core/hnsw_index.h` - Update 3 method signatures
- `src/core/hnsw_index.cpp` - Update node TID storage

---

### TASK 1.5.3: StorageEngine Layer TID Migration - 0% Complete

**Estimated Work**: 3-4 hours

**Current API** (storage_engine.h):
```cpp
struct Tuple {
    const uint8_t *data;
    uint32_t data_size;
    uint64_t tid;        // LEGACY: 64-bit encoding
    uint16_t item_id;
    uint32_t page_id;
};

Status insertTuple(..., uint32_t *page_id_out, uint16_t *item_id_out, ...);
Status updateTuple(uint32_t page_id, uint16_t item_id, ...);
Status deleteTuple(uint32_t page_id, uint16_t item_id, ...);
Status getTuple(uint32_t page_id, uint16_t item_id, Tuple *tuple_out, ...);
```

**New API**:
```cpp
struct Tuple {
    const uint8_t *data;
    uint32_t data_size;
    TID tid;             // NEW: TID struct
};

Status insertTuple(..., TID *tid_out, ...);
Status updateTuple(const TID &tid, ...);
Status deleteTuple(const TID &tid, ...);
Status getTuple(const TID &tid, Tuple *tuple_out, ...);
```

**Files**:
- `include/scratchbird/core/storage_engine.h` - Update Tuple struct, update 4+ method signatures
- `src/core/storage_engine.cpp` - Update all implementations (~200-300 lines)
- All SQL operation call sites need updating

**Impact**: This is a **critical API boundary** - all SQL executor code depends on this

---

### TASK 1.5.4: GarbageCollector TID Migration - 0% Complete

**Estimated Work**: 2-3 hours

**Current API** (garbage_collector.h:203):
```cpp
uint64_t cleanIndexes(uint32_t page_id, const std::vector<uint64_t> &dead_tids, ...);
```

**New API**:
```cpp
uint64_t cleanIndexes(const std::vector<TID> &dead_tids, ...);
std::vector<TID> collectDeadTuples(...);  // Return TID structs
```

**Files**:
- `include/scratchbird/core/garbage_collector.h` - Update 2 method signatures
- `src/core/garbage_collector.cpp` - Update all GC logic
- `include/scratchbird/core/index_gc_interface.h` - Update interface

**Impact**: All 6 index types implement `IndexGCInterface`, all need consistent TID API

---

### TASK 1.5.5: Test Suite Updates - 0% Complete

**Estimated Work**: 3-5 hours

**Scope**:
- ~230 index test call sites need updating
- ~100+ integration test call sites
- Need new TID conversion unit tests
- Need migration verification tests

**Test Files Affected** (estimated):
- `tests/unit/test_btree.cpp` - ~50+ call sites
- `tests/unit/test_hash_index.cpp` - ~40+ call sites
- `tests/unit/test_gin_index.cpp` - ~50+ call sites
- `tests/unit/test_bitmap_index.cpp` - ~30+ call sites
- `tests/unit/test_hnsw_index.cpp` - ~30+ call sites
- `tests/unit/test_brin_index.cpp` - ~30+ call sites
- `tests/integration/*.cpp` - ~100+ call sites
- All test files using `insertTuple()`, `updateTuple()`, `deleteTuple()` APIs

**New Tests Needed**:
1. TID conversion unit tests (legacy ↔ new format)
2. TID hash function tests
3. TID comparison operator tests
4. Cross-tablespace TID tests
5. Database version upgrade tests

---

### TASK 1.5.6: Database Migration Tool - 0% Complete

**Estimated Work**: 2-3 hours

**Required Features**:
1. **Database Version Field**:
   ```cpp
   // In DatabaseHeader:
   uint32_t database_version;  // 0 = legacy, 1 = GPID-based
   ```

2. **Version Detection on Open**:
   ```cpp
   if (header->database_version == 0) {
       // Old format detected - need upgrade
       Status status = migrateToGPIDFormat(ctx);
       if (status != Status::OK) {
           return status;  // User must upgrade
       }
   }
   ```

3. **In-Place TID Conversion**:
   - Scan all heap pages
   - Convert TupleHeader from 36 bytes to 44 bytes
   - Convert all index entries from uint64_t to TID struct
   - Update database version to 1
   - Fsync all changes

**Files to Create**:
- `include/scratchbird/core/database_migration.h`
- `src/core/database_migration.cpp`

**Challenges**:
- On-disk format change is **non-reversible**
- Must handle partial migration (crash during upgrade)
- May need to rebuild all indexes instead of converting in-place

---

## 🚨 Critical Issues

### 1. API Boundary Mismatch

**Problem**: Heap layer produces `TID` structs, but all indexes and StorageEngine expect `uint64_t`.

**Current Workarounds**:
- B-Tree insert() uses `convertTIDtoLegacy()` to convert TID → uint64_t
- This defeats the purpose of type safety
- Creates error-prone manual conversions at every call site

**Impact**:
- Type safety compromised
- Performance overhead from constant conversions
- Risk of bugs when forgetting to convert

### 2. On-Disk Format Already Changed

**Problem**: TupleHeader is now 44 bytes instead of 36 bytes.

**Impact**:
- **Any existing databases are incompatible** with current code
- No migration path exists yet
- Cannot revert to old format without data loss

**Mitigation**:
- Acceptable in ALPHA (no production databases)
- Must complete Phase 1.5 before BETA release

### 3. Test Suite Likely Broken

**Problem**: Tests likely fail due to type mismatches.

**Evidence**:
- Index APIs expect `uint64_t tuple_id`
- Heap APIs return `TID` structs
- No compilation errors flagged yet (headers updated, but not all call sites)

**Next Steps**:
- Attempt compilation to identify all breakage
- Fix systematically file-by-file

---

## 📋 Detailed Work Plan

### Phase 1: Complete B-Tree Migration (3-4 hours)

1. ✅ Update btree.h signatures (DONE)
2. 🟡 Update btree.cpp insert() (STARTED)
3. ❌ Update btree.cpp searchPage() (~30 locations)
4. ❌ Update btree.cpp search()
5. ❌ Update btree.cpp remove()
6. ❌ Update btree.cpp split_leaf_page()
7. ❌ Update btree.cpp removeDeadEntries()
8. ❌ Update BTreeIterator::next()
9. ❌ Update all internal helpers
10. ❌ Test compilation

### Phase 2: Migrate Other Indexes (14-18 hours)

Each index follows same pattern:
1. Add `#include "scratchbird/core/tid.h"` to header
2. Update method signatures (insert, search/find, remove, removeDeadEntries)
3. Update implementation to use TID struct
4. Convert on-disk storage format if needed
5. Test compilation

**Order**:
1. Hash Index (3-4h)
2. Bitmap Index (2-3h) - Simplest, good warmup
3. HNSW Index (2-3h)
4. BRIN Index (2-3h)
5. GIN Index (4-5h) - Most complex, has posting trees

### Phase 3: Migrate StorageEngine (3-4 hours)

1. Update Tuple struct
2. Update insertTuple() signature and implementation
3. Update updateTuple() signature and implementation
4. Update deleteTuple() signature and implementation
5. Update getTuple() signature and implementation
6. Update TableScan::next() to return TID
7. Test compilation

### Phase 4: Migrate GarbageCollector (2-3 hours)

1. Update IndexGCInterface with TID API
2. Update cleanIndexes() to accept `std::vector<TID>`
3. Update collectDeadTuples() to return `std::vector<TID>`
4. Update all call sites
5. Test compilation

### Phase 5: Fix All Compilation Errors (4-6 hours)

1. Attempt full build
2. Fix all type mismatch errors
3. Fix all conversion errors
4. Fix all test compilation errors
5. Verify no warnings

### Phase 6: Update Test Suite (3-5 hours)

1. Update all index unit tests
2. Update all integration tests
3. Add new TID unit tests
4. Add migration verification tests
5. Run full test suite

### Phase 7: Database Migration Tool (2-3 hours)

1. Add database_version to DatabaseHeader
2. Implement version detection
3. Implement in-place upgrade
4. Test upgrade with sample database
5. Document upgrade procedure

---

## 🎯 Estimated Total Effort

| Phase | Hours | Status |
|-------|-------|--------|
| 1. B-Tree | 3-4h | 🟡 10% done |
| 2. Other Indexes | 14-18h | ❌ Not started |
| 3. StorageEngine | 3-4h | ❌ Not started |
| 4. GarbageCollector | 2-3h | ❌ Not started |
| 5. Compilation Fixes | 4-6h | ❌ Not started |
| 6. Test Suite | 3-5h | ❌ Not started |
| 7. Migration Tool | 2-3h | ❌ Not started |
| **TOTAL** | **31-43h** | **~5% done** |

**Current Progress**: ~2 hours invested (header updates)
**Remaining Work**: ~29-41 hours

---

## 🔧 Technical Approach

### Strategy 1: Incremental File-by-File Migration

**Pros**:
- Lower risk of breaking everything at once
- Can test each component independently
- Easier to debug

**Cons**:
- Requires temporary conversion layer
- Slower overall progress
- More complex intermediate state

**Recommended for**: Large, complex projects with existing tests

### Strategy 2: Big Bang Migration

**Pros**:
- Cleaner final result
- No temporary conversion code
- Faster if done correctly

**Cons**:
- High risk of breaking everything
- Harder to debug
- All-or-nothing approach

**Recommended for**: ALPHA projects with few dependencies (like ScratchBird)

### Recommended Approach: Hybrid

1. **File-by-file for indexes** (Phase 1-2)
   - Each index is independent
   - Can test in isolation
   - Lower risk

2. **Big bang for StorageEngine + GC** (Phase 3-4)
   - These are tightly coupled
   - Conversion layer would be complex
   - Better to do together

3. **Incremental test fixes** (Phase 6)
   - Fix tests as each component completes
   - Maintains CI/CD pipeline

---

## 📝 Code Patterns

### Pattern 1: Converting Legacy TID to New Format

```cpp
// When receiving legacy uint64_t from old code:
uint64_t legacy_tid = ...;
TID tid = convertLegacyTID(legacy_tid);

// Extract components:
uint32_t page_id = static_cast<uint32_t>(getPageNumber(tid));
uint16_t slot = getSlot(tid);
GPID gpid = getGPID(tid);
```

### Pattern 2: Converting New TID to Legacy Format

```cpp
// When interfacing with old code that expects uint64_t:
TID tid = ...;
uint64_t legacy_tid = convertTIDtoLegacy(tid);

// WARNING: Returns 0 if tid is from custom tablespace!
if (legacy_tid == 0) {
    // TID is from custom tablespace, cannot convert
    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                      "Cannot convert custom tablespace TID to legacy format");
    return Status::INVALID_ARGUMENT;
}
```

### Pattern 3: Creating TID from Components

```cpp
// From GPID and slot:
GPID gpid = makeGPID(tablespace_id, page_number);
TID tid = makeTID(gpid, slot);

// Or directly:
TID tid = makeTID(tablespace_id, page_number, slot);
```

### Pattern 4: Updating Method Signatures

```cpp
// OLD:
Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id, ...);

// NEW:
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);

// OLD:
Status search(..., std::vector<uint64_t> *tuple_ids_out, ...);

// NEW:
Status search(..., std::vector<TID> *tids_out, ...);
```

---

## ✅ Acceptance Criteria

Phase 1.5 is COMPLETE when:

- [ ] All 6 index types use TID struct API
- [ ] StorageEngine uses TID struct API
- [ ] GarbageCollector uses TID struct API
- [ ] All tests updated and passing
- [ ] Database migration tool implemented
- [ ] Full codebase compiles with no errors
- [ ] No manual TID conversions required (except for legacy compatibility)
- [ ] Documentation updated (TABLESPACE_IMPLEMENTATION_PLAN.md)
- [ ] No type safety compromises

---

## 🚀 Next Steps (Immediate)

### Option A: Complete Phase 1.5 Now (~30-40 hours)

**Pros**:
- Clean, consistent API
- Type safety throughout
- No technical debt
- Multi-tablespace support fully functional

**Cons**:
- Delays Phase 2 (SQL DDL)
- Significant time investment
- Risk of introducing bugs

**Recommendation**: ✅ **DO THIS** - Breaking changes now acceptable in ALPHA

### Option B: Defer to Post-BETA

**Pros**:
- Faster progress to SQL DDL
- Can release basic tablespace support sooner

**Cons**:
- Accumulates technical debt
- Makes completion harder later
- Breaking change when databases exist
- Type safety compromised
- Manual conversions everywhere

**Recommendation**: ❌ **DON'T DO THIS** - Will be much harder later

### Option C: Revert Heap Layer Changes

**Pros**:
- Restores consistency (all APIs use uint64_t)
- No breaking changes yet

**Cons**:
- Wastes 6 hours of work
- Still need to do migration eventually
- Misses opportunity to do it in ALPHA

**Recommendation**: ❌ **DON'T DO THIS** - Work already done is valuable

---

## 📊 Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Breaking existing tests | HIGH | HIGH | Fix incrementally, maintain CI |
| Introducing new bugs | MEDIUM | HIGH | Comprehensive testing after each phase |
| Missing call sites | MEDIUM | MEDIUM | Use compiler errors to find all |
| On-disk format issues | LOW | HIGH | Test migration tool thoroughly |
| Performance regression | LOW | MEDIUM | Benchmark after completion |
| Schedule slip | MEDIUM | MEDIUM | Time-box each phase, track progress |

---

## 📅 Estimated Schedule

**Assuming 8 hours/day of focused work:**

- **Day 1**: Complete B-Tree migration (3-4h) + Start Hash Index (3-4h)
- **Day 2**: Complete Hash/Bitmap/HNSW indexes (8h)
- **Day 3**: Complete BRIN/GIN indexes (6-8h) + Start StorageEngine (2h)
- **Day 4**: Complete StorageEngine + GarbageCollector (6-8h)
- **Day 5**: Fix all compilation errors (6-8h)
- **Day 6**: Update test suite (6-8h)
- **Day 7**: Database migration tool + final testing (6-8h)

**Total**: ~5-7 working days (40-50 hours)

---

## 📖 References

- **Specification**: `docs/specifications/TABLESPACE_SPECIFICATION.md`
- **Implementation Plan**: `docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_IMPLEMENTATION_PLAN.md` (Section Phase 1.5)
- **TID Analysis**: `docs/STATUS_PHASE1_TASK1_2_5_TID_ANALYSIS.md`
- **Project Context**: `PROJECT_CONTEXT.md`

---

## 🎯 Decision Point

**Should we complete Phase 1.5 now (Option A) or defer (Option B)?**

**My Recommendation**: **Complete Phase 1.5 now** (Option A)

**Rationale**:
1. ✅ **ALPHA is the right time for breaking changes**
2. ✅ **Heap layer already migrated** - turning back wastes effort
3. ✅ **Type safety is valuable** - prevents bugs
4. ✅ **Cleaner architecture** - no conversion layer needed
5. ✅ **Consistent API** - all components use same types
6. ✅ **Multi-tablespace ready** - Phase 2 can use TID structs directly

**Estimated Time**: 30-40 hours (~5-7 working days)

---

**END OF STATUS REPORT**

*Last Updated: 2025-10-20 23:30 UTC*
*Next Update: After phase completion decision*
