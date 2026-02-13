# Task 1.2.5: Index GPID/TID Update - Scope Analysis

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 20, 2025
**Status**: ANALYSIS COMPLETE - SCOPE EXPANSION REQUIRED
**Estimated (Original)**: 2-4 hours
**Actual Complexity**: 20-30 hours (major refactoring required)

---

## Executive Summary

Task 1.2.5 ("Update all index implementations to use GPID for TIDs") is **significantly more complex** than the original 2-4 hour estimate. Analysis reveals this requires:

1. **On-disk format changes**: HeapTuple `ctid_page` from 32-bit to 64-bit GPID
2. **API changes**: All 6 index types must change from `uint64_t tuple_id` to `TID` struct
3. **Migration strategy**: Existing databases must convert old TID format to new format
4. **Extensive testing**: All index operations, heap operations, and GC must be validated

**Recommendation**: **DEFER Task 1.2.5 to Phase 1.5** (between Phase 1 and Phase 2) or make it part of Task 1.3 (Tablespace File Management).

---

## Current TID Encoding

### Heap Layer (heap_page.h)

**TupleHeader structure** (lines 77-105):
```cpp
struct TupleHeader
{
    uint64_t xmin;                  // 8 bytes
    uint64_t xmax;                  // 8 bytes
    uint64_t back_version_tid;      // 8 bytes (encoded TID)
    uint32_t ctid_page;             // 4 bytes ← NEEDS TO BECOME GPID (8 bytes)
    uint16_t ctid_item;             // 2 bytes
    uint16_t infomask;              // 2 bytes
    // ... more fields
};
```

**Current TID encoding**:
- `getTID()`: `(ctid_page << 32) | (ctid_item << 16)`
- Result: 64-bit value with 48 bits used (32-bit page + 16-bit item)
- Assumes tablespace 0 (primary file only)

### Index Layer (btree.h, hash_index.h, gin_index.h, etc.)

**All indexes use** `uint64_t tuple_id` **parameters**:
- B-Tree: `insert(key, uint64_t tuple_id)`
- B-Tree: `search(key, std::vector<uint64_t> *tuple_ids_out)`
- Hash: `insert(key, uint64_t tuple_id)`
- GIN: `insert(key, uint64_t tuple_id)`
- Bitmap: `insert(key, uint64_t tuple_id)`
- BRIN: Stores `uint64_t` block numbers
- HNSW: Stores `uint64_t tuple_id` for node references

**Current encoding works** because:
- Page IDs fit in 32 bits (4 billion pages max)
- Item IDs fit in 16 bits (65K slots per page)
- Combined fits in 64 bits with room to spare

---

## Required TID Format for GPID Support

### New TID Structure

**Created** `include/scratchbird/core/tid.h` (~245 lines):

```cpp
struct TID
{
    GPID gpid;          // 64-bit: [16-bit tablespace_id][48-bit page_number]
    uint16_t slot;      // 16-bit: item ID within page

    // Total: 80 bits (10 bytes)
};
```

**Features**:
- `makeTID(GPID, slot)` and `makeTID(tablespace_id, page_number, slot)`
- `getGPID(tid)`, `getSlot(tid)`, `getTablespaceID(tid)`, `getPageNumber(tid)`
- `convertLegacyTID(uint64_t)` - convert old format to new
- `convertTIDtoLegacy(TID)` - convert new to old (tablespace 0 only)
- `tidToString(tid)` - human-readable format
- `std::hash<TID>` specialization for containers

---

## Required Changes

### 1. Heap Layer Changes (heap_page.h/cpp)

**TupleHeader** (on-disk format change):
```cpp
struct TupleHeader
{
    uint64_t xmin;
    uint64_t xmax;
    uint64_t back_version_tid;  // Keep as 64-bit for now? Or expand to TID?

    // CHANGE: Expand ctid_page from 32-bit to 64-bit GPID
    GPID ctid_gpid;             // 8 bytes (was ctid_page: 4 bytes)
    uint16_t ctid_item;         // 2 bytes (unchanged)
    uint16_t infomask;          // 2 bytes (unchanged)
    // ... SIZE INCREASED BY 4 BYTES
};
```

**Impact**:
- **BREAKS ON-DISK COMPATIBILITY**: All existing databases incompatible
- Static assertion `sizeof(TupleHeader)` will fail
- All heap operations must update (insert, update, delete, scan)
- All version chain operations must update

**Methods to update**:
- `getTID()` → return `TID` instead of `uint64_t`
- `setTID(uint32_t page_id, uint16_t item_id)` → `setTID(GPID gpid, uint16_t item_id)`
- `getBackVersionTID()` → return `TID` instead of `uint64_t`
- `setBackVersionTID()` → accept `TID` instead of separate params

### 2. Index Layer Changes (ALL 6 INDEX TYPES)

**API signature changes** (affects ALL call sites):

**B-Tree** (btree.h/cpp):
- `insert(key, uint64_t tuple_id)` → `insert(key, TID tid)`
- `search(key, std::vector<uint64_t> *tuple_ids_out)` → `search(key, std::vector<TID> *tids_out)`
- `remove(key, uint64_t tuple_id)` → `remove(key, TID tid)`
- `removeDeadEntries(std::vector<uint64_t> dead_tids)` → `removeDeadEntries(std::vector<TID> dead_tids)`
- `BTreeIterator::next(..., uint64_t *tuple_id_out)` → `next(..., TID *tid_out)`

**Hash Index** (hash_index.h/cpp):
- `insert(key, uint64_t tuple_id)` → `insert(key, TID tid)`
- `find(key, std::vector<uint64_t> *tuple_ids_out)` → `find(key, std::vector<TID> *tids_out)`
- `remove(key, uint64_t tuple_id)` → `remove(key, TID tid)`
- `removeDeadEntries(std::vector<uint64_t> dead_tids)` → `removeDeadEntries(std::vector<TID> dead_tids)`

**GIN Index** (gin_index.h/cpp):
- `insert(key, uint64_t tuple_id)` → `insert(key, TID tid)`
- `find(key, std::vector<uint64_t> *tuple_ids_out)` → `find(key, std::vector<TID> *tids_out)`
- `remove(key, uint64_t tuple_id)` → `remove(key, TID tid)`
- `removeDeadEntries(std::vector<uint64_t> dead_tids)` → `removeDeadEntries(std::vector<TID> dead_tids)`
- GIN posting tree stores TIDs, must update on-disk format

**Bitmap Index** (bitmap_index.h/cpp):
- `insert(key, uint64_t tuple_id)` → `insert(key, TID tid)`
- `find(key, std::vector<uint64_t> *tuple_ids_out)` → `find(key, std::vector<TID> *tids_out)`
- `removeDeadEntries(std::vector<uint64_t> dead_tids)` → `removeDeadEntries(std::vector<TID> dead_tids)`
- Bitmap stores tuple positions, may need TID → position mapping

**BRIN Index** (brin_index.h/cpp):
- Stores block numbers (currently `uint64_t`)
- Should use GPID for block addressing (not TID)
- `scan(..., std::vector<uint32_t> *block_numbers_out)` → `scan(..., std::vector<GPID> *gpids_out)`

**HNSW Index** (hnsw_index.h/cpp):
- `insert(vector, uint64_t tuple_id)` → `insert(vector, TID tid)`
- `search(vector, k, std::vector<HnswSearchResult> *results)` where `HnswSearchResult` contains `uint64_t tuple_id`
- `removeDeadEntries(std::vector<uint64_t> dead_tids)` → `removeDeadEntries(std::vector<TID> dead_tids)`

### 3. StorageEngine Layer Changes (storage_engine.h/cpp)

**All table operations** return `uint64_t tuple_id`:
- `insertTuple(..., uint64_t *tuple_id_out)` → `insertTuple(..., TID *tid_out)`
- `updateTuple(uint64_t tuple_id, ...)` → `updateTuple(TID tid, ...)`
- `deleteTuple(uint64_t tuple_id, ...)` → `deleteTuple(TID tid, ...)`
- `TableScan::next(..., uint64_t *tuple_id_out)` → `next(..., TID *tid_out)`

**Impact**: ALL SQL operations (SELECT, INSERT, UPDATE, DELETE) affected

### 4. GarbageCollector Layer Changes (garbage_collector.h/cpp)

**Dead tuple tracking**:
- `collectDeadTuples(...)` returns `std::vector<uint64_t> dead_tids` → `std::vector<TID>`
- `cleanIndexes(std::vector<uint64_t> dead_tids)` → `cleanIndexes(std::vector<TID> dead_tids)`

### 5. Test Suite Changes

**ALL index tests** must update:
- `test_btree.cpp`: ~50 call sites
- `test_hash_index.cpp`: ~40 call sites
- `test_gin_index.cpp`: ~60 call sites
- `test_bitmap_index.cpp`: ~30 call sites
- `test_brin_index.cpp`: ~20 call sites
- `test_hnsw_index.cpp`: ~30 call sites
- Integration tests: ~100+ call sites

**Total affected test code**: ~1000+ lines

---

## Migration Strategy

### Database Version Upgrade

**Scenario**: Existing database with old TID format (32-bit page_id)

**Options**:

1. **In-place conversion** (preferred for Phase 1):
   - Add `database_version` field to DatabaseHeader
   - On open, detect old version (v1.0.0) vs new version (v1.1.0)
   - If old: Run conversion process
     - For each heap page:
       - Read TupleHeader, convert `ctid_page` (32-bit) → `ctid_gpid` (64-bit with tablespace=0)
       - Rewrite tuple header
     - For each index:
       - Iterate all entries
       - Convert `tuple_id` (old encoding) → TID (new encoding)
       - Rewrite index entries
   - Mark database as upgraded to v1.1.0

2. **Export/Import** (manual, offline):
   - `scratchbird dump olddb.sbdb > data.sql`
   - Create new database with new format
   - `scratchbird restore newdb.sbdb < data.sql`

3. **Parallel operation** (complex):
   - Runtime detection: Try old format first, fall back to new format
   - Gradual migration during VACUUM
   - Requires dual-format support (complex)

**Recommended**: Option 1 (in-place conversion) as part of Task 1.3 or new Phase 1.5.

---

## Work Estimate Breakdown

### Analysis Complete ✅ (~2 hours)
- [x] Understand current TID encoding
- [x] Design new TID structure
- [x] Create `tid.h` infrastructure
- [x] Identify all affected components
- [x] Document migration strategy

### Remaining Work (~28-38 hours)

**1. Heap Layer** (4-6 hours):
- Update `TupleHeader` on-disk format
- Update `getTID()`, `setTID()`, `getBackVersionTID()`, `setBackVersionTID()`
- Update all heap operations (insert, update, delete, scan)
- Update version chain operations

**2. B-Tree Index** (3-4 hours):
- Update API signatures (10+ methods)
- Update on-disk TID storage (if applicable)
- Update all call sites in btree.cpp

**3. Hash Index** (3-4 hours):
- Update API signatures
- Update on-disk bucket TID storage
- Update all call sites

**4. GIN Index** (4-5 hours):
- Update API signatures
- Update posting tree TID storage (on-disk)
- Update all call sites
- Update compression if TID-based

**5. Bitmap Index** (2-3 hours):
- Update API signatures
- Update TID tracking
- Update all call sites

**6. BRIN Index** (2-3 hours):
- Update to use GPID for block references
- Update API signatures
- Update all call sites

**7. HNSW Index** (2-3 hours):
- Update API signatures
- Update node TID storage
- Update all call sites

**8. StorageEngine Layer** (3-4 hours):
- Update all table operation APIs
- Update TableScan APIs
- Update all SQL operation call sites

**9. GarbageCollector Layer** (2-3 hours):
- Update dead tuple tracking
- Update index cleaning coordination

**10. Test Suite** (3-5 hours):
- Update all index unit tests
- Update all integration tests
- Add TID conversion tests

**Total Remaining**: ~28-38 hours

**Grand Total**: ~30-40 hours (vs original estimate of 2-4 hours)

---

## Recommendation

### Option A: Complete Task 1.2.5 Now (30-40 hours)
**Pros**:
- Full GPID support ready immediately
- Clean architecture from start

**Cons**:
- Delays Phase 1 completion by 3-4 weeks
- Breaks ALL existing databases (migration required)
- High risk of introducing bugs across entire codebase

### Option B: Defer Task 1.2.5 to Phase 1.5 (RECOMMENDED)
**Pros**:
- Completes Phase 1 quickly with current infrastructure
- Allows testing of GPID addressing at BufferPool/Database level first
- Can plan proper migration strategy
- Can complete Phase 2 (SQL DDL) in parallel

**Cons**:
- Indexes still use old TID format temporarily
- Requires tracking "Phase 1.5" between Phase 1 and Phase 2

**Recommended Timeline**:
1. **Now**: Mark Task 1.2.5 as "DEFERRED"
2. **Complete Phase 1**: Finish Task 1.3 (Tablespace File Management)
3. **Phase 1.5**: Full TID migration (30-40 hours)
4. **Phase 2**: SQL DDL with full GPID support

### Option C: Hybrid Approach (Split Task 1.2.5)
**Task 1.2.5a** (do now, 2-3 hours):
- Add `tid.h` infrastructure ✅ (DONE)
- Add conversion helpers
- Mark heap/index layers as "LEGACY TID FORMAT"

**Task 1.2.5b** (defer, 28-35 hours):
- Full heap/index TID migration
- On-disk format changes
- Migration tool implementation

---

## Files Created

### ✅ include/scratchbird/core/tid.h (~245 lines)
**Features**:
- `struct TID { GPID gpid; uint16_t slot; }`
- `makeTID()`, `getGPID()`, `getSlot()`, `getTablespaceID()`, `getPageNumber()`
- `convertLegacyTID()`, `convertTIDtoLegacy()` for migration
- `tidToString()` for debugging
- `std::hash<TID>` specialization
- Full documentation

**Status**: COMPLETE, ready for use

---

## Next Steps

**Immediate** (based on recommendation):
1. Update TABLESPACE_IMPLEMENTATION_PLAN.md:
   - Mark Task 1.2.5 as "PARTIALLY COMPLETE (infrastructure only)"
   - Add Phase 1.5 for full TID migration (30-40 hours)
   - Update Phase 1 completion estimate (remove 2-4 hours from current phase)

2. Proceed with Task 1.3: Tablespace File Management
   - This can proceed with current TID format
   - BufferPool/Database GPID support is sufficient

3. Plan Phase 1.5 (TID Migration) after Phase 1 complete:
   - Full heap/index layer migration
   - Database version upgrade tool
   - Comprehensive testing

**Long-term**:
- Consider making TID migration part of a major version release (v2.0.0)
- Document breaking changes clearly
- Provide migration tools and documentation

---

## Conclusion

Task 1.2.5 is **10x more complex** than originally estimated (30-40 hours vs 2-4 hours). The created `tid.h` infrastructure is complete and ready, but full implementation requires extensive changes across heap layer, all 6 index types, storage engine, garbage collector, and test suite.

**Recommendation**: **DEFER** full implementation to Phase 1.5, complete Phase 1 with current infrastructure, then do proper TID migration as a required major refactoring effort.
