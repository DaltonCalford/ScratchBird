# Phase 1.5: TID Migration - Completion Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Document Purpose**: Step-by-step guide to complete the remaining 80% of Phase 1.5 TID migration
**Estimated Time**: 25-35 hours
**Current Status**: ~20% complete (Heap layer done, interfaces updated)

---

## Table of Contents

1. [Current State](#current-state)
2. [Completion Strategy](#completion-strategy)
3. [File-by-File Migration Guide](#file-by-file-migration-guide)
4. [Code Templates](#code-templates)
5. [Testing Strategy](#testing-strategy)
6. [Troubleshooting](#troubleshooting)

---

## Current State

### ✅ Completed (20%)

1. **Heap Layer** - Fully migrated
   - `tid.h` - Complete TID struct (242 lines)
   - `heap_page.h` - TupleHeader uses GPID (44 bytes)
   - `heap_page.cpp` - All operations use TID struct

2. **B-Tree Index** - Partially migrated (10%)
   - Header: ✅ All signatures updated
   - Implementation:
     - ✅ `insert()` - signature updated, uses convertTIDtoLegacy()
     - ✅ `searchPage()` - signature updated, converts to TID
     - ✅ `search()` - signature updated
     - ❌ `remove()` - signature updated, but still uses `tuple_id` variable name
     - ❌ `split_leaf_page()` - signature updated but not implementation
     - ❌ `removeDeadEntries()` - not started
     - ❌ `BTreeIterator::next()` - not started

3. **Interfaces**
   - ✅ `IndexGCInterface` - Updated to use `std::vector<TID>`

### ❌ Remaining (80%)

1. **B-Tree**: 90% remaining (~3h)
2. **Hash Index**: 100% remaining (~3-4h)
3. **GIN Index**: 100% remaining (~4-5h)
4. **Bitmap Index**: 100% remaining (~2-3h)
5. **HNSW Index**: 100% remaining (~2-3h)
6. **BRIN Index**: 100% remaining (~2-3h)
7. **StorageEngine**: 100% remaining (~3-4h)
8. **GarbageCollector**: 100% remaining (~2-3h)
9. **Tests**: 100% remaining (~3-5h)
10. **Migration Tool**: 100% remaining (~2-3h)

---

## Completion Strategy

### Recommended Order

1. **Complete B-Tree** (finish what we started)
2. **Simple indexes first** (Bitmap, HNSW, BRIN)
3. **Complex indexes** (Hash, GIN)
4. **Core APIs** (StorageEngine, GarbageCollector)
5. **Compilation fixes** (iterative)
6. **Tests** (update as each component completes)
7. **Migration tool** (final step)

### Two-Pass Approach

**Pass 1: Mechanical Changes** (automated where possible)
- Update method signatures
- Change variable names
- Update type declarations

**Pass 2: Semantic Changes** (manual)
- Add TID conversion calls
- Update on-disk format handling
- Fix iterator implementations
- Handle edge cases

---

## File-by-File Migration Guide

### 1. Complete B-Tree Migration (~3 hours)

**File**: `src/core/btree.cpp` (2395 lines)

**Remaining Work**:

#### 1.1. Fix remove() method (lines 862-990)

**Current Issue**: Uses `tuple_id` variable name but signature says `tid`

```cpp
// LINE 933: Change comparison
// OLD:
if (tuple_ids_ptr[j] == tuple_id)

// NEW:
uint64_t legacy_tid = convertTIDtoLegacy(tid);
if (tuple_ids_ptr[j] == legacy_tid)
```

#### 1.2. Fix split_leaf_page() method (lines 992+)

**Current State**: Signature updated to `const TID &new_tid` but implementation uses `new_tuple_id`

**Search for**: All occurrences of `new_tuple_id` in the function
**Replace with**: `new_tid`
**Add conversion**: When storing to on-disk format:

```cpp
// When writing TID to page:
uint64_t legacy_tid = convertTIDtoLegacy(new_tid);
// Store legacy_tid in the uint64_t array
```

#### 1.3. Implement removeDeadEntries() (lines ~2194+)

**Current Signature**:
```cpp
Status BTree::removeDeadEntries(const std::vector<TID> &dead_tids, ...)
```

**Current Issue**: Uses `std::set<uint64_t>` internally

**Fix**:
```cpp
// LINE ~2216: Change from:
std::set<uint64_t> dead_set(dead_tids.begin(), dead_tids.end());

// To:
std::set<uint64_t> dead_set;
for (const TID &tid : dead_tids) {
    uint64_t legacy = convertTIDtoLegacy(tid);
    if (legacy != 0) {  // Skip custom tablespace TIDs
        dead_set.insert(legacy);
    }
}
```

#### 1.4. Fix BTreeIterator::next()

**File**: `src/core/btree.cpp` (search for "BTreeIterator::next")

**Current Signature**:
```cpp
Status next(std::vector<uint8_t> *key_out, TID *tid_out, ...)
```

**Fix**: When returning TID, convert from stored uint64_t:
```cpp
// When reading tuple_id from page:
uint64_t stored_tid = ...;  // Read from page
*tid_out = convertLegacyTID(stored_tid);
```

---

### 2. Hash Index Migration (~3-4 hours)

**Files**:
- `include/scratchbird/core/hash_index.h`
- `src/core/hash_index.cpp`

#### 2.1. Update header

```cpp
// Add include at top:
#include "scratchbird/core/tid.h"

// Update method signatures:
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status find(const std::vector<uint8_t> &key, std::vector<TID> *tids_out, ...);
Status remove(const std::vector<uint8_t> &key, const TID &tid, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...) override;
```

#### 2.2. Update implementation

**Pattern 1: Insert**
```cpp
// Convert TID to legacy for storage:
uint64_t legacy_tid = convertTIDtoLegacy(tid);

// Store legacy_tid in hash bucket
// (On-disk format still uses uint64_t arrays)
```

**Pattern 2: Find**
```cpp
// Read uint64_t from bucket
uint64_t stored_tid = ...;

// Convert to TID before returning
TID tid = convertLegacyTID(stored_tid);
tids_out->push_back(tid);
```

**Pattern 3: Remove**
```cpp
// Convert TID to legacy for comparison:
uint64_t legacy_tid = convertTIDtoLegacy(tid);

// Compare with stored uint64_t values
if (stored_tid == legacy_tid) {
    // Remove entry
}
```

**Pattern 4: removeDeadEntries**
```cpp
// Build set of legacy TIDs:
std::set<uint64_t> dead_set;
for (const TID &tid : dead_tids) {
    uint64_t legacy = convertTIDtoLegacy(tid);
    if (legacy != 0) {
        dead_set.insert(legacy);
    }
}

// Scan buckets and remove matching entries
```

---

### 3. Bitmap Index Migration (~2-3 hours)

**Files**:
- `include/scratchbird/core/bitmap_index.h`
- `src/core/bitmap_index.cpp`

**Simplification**: Bitmap index is simpler - just update signatures and add conversions.

#### 3.1. Update header

```cpp
#include "scratchbird/core/tid.h"

Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status find(const std::vector<uint8_t> &key, std::vector<TID> *tids_out, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...) override;
```

#### 3.2. Update implementation

Same patterns as Hash Index - convert at API boundaries.

---

### 4. HNSW Index Migration (~2-3 hours)

**Files**:
- `include/scratchbird/core/hnsw_index.h`
- `src/core/hnsw_index.cpp`

**Note**: HNSW nodes store TIDs - may need on-disk format update.

#### 4.1. Update header

```cpp
#include "scratchbird/core/tid.h"

Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status search(..., std::vector<TID> *tids_out, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...) override;
```

#### 4.2. Update node storage

**Decision Point**:
- **Option A**: Store uint64_t (legacy format), convert at API boundary
- **Option B**: Store TID struct (80 bits = 10 bytes vs 8 bytes)

**Recommendation**: Option A (less disruptive)

---

### 5. BRIN Index Migration (~2-3 hours)

**Files**:
- `include/scratchbird/core/brin_index.h`
- `src/core/brin_index.cpp`

**Special Case**: BRIN uses GPIDs for block references (already GPID-aware!)

#### 5.1. Update header

```cpp
#include "scratchbird/core/tid.h"

// BRIN already uses GPID for block references - minimal changes needed
Status scan(..., std::vector<TID> *tids_out, ...);
```

---

### 6. GIN Index Migration (~4-5 hours)

**Files**:
- `include/scratchbird/core/gin_index.h`
- `src/core/gin_index.cpp`

**Complexity**: GIN has posting trees with TID compression - most complex migration.

#### 6.1. Update header

```cpp
#include "scratchbird/core/tid.h"

Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
Status find(const std::vector<uint8_t> &key, std::vector<TID> *tids_out, ...);
Status remove(const std::vector<uint8_t> &key, const TID &tid, ...);
Status removeDeadEntries(const std::vector<TID> &dead_tids, ...) override;
```

#### 6.2. Update posting tree storage

**Critical Decision**: GIN compresses TIDs in posting lists.

**Current Compression**: Delta encoding of uint64_t TIDs
**Options**:
1. Keep uint64_t storage, convert at API boundary (simpler)
2. Update compression to handle TID struct (better, but complex)

**Recommendation**: Option 1 for Phase 1.5, Option 2 for future optimization

#### 6.3. Update posting list operations

```cpp
// When adding TID to posting list:
uint64_t legacy_tid = convertTIDtoLegacy(tid);
// Store legacy_tid (existing compression works)

// When reading from posting list:
uint64_t stored_tid = ...;  // Decompress
TID tid = convertLegacyTID(stored_tid);
tids_out->push_back(tid);
```

---

### 7. StorageEngine Migration (~3-4 hours)

**Files**:
- `include/scratchbird/core/storage_engine.h`
- `src/core/storage_engine.cpp`

**Impact**: HIGH - This is the main API used by SQL executor

#### 7.1. Update Tuple struct

```cpp
// OLD:
struct Tuple {
    const uint8_t *data;
    uint32_t data_size;
    uint64_t tid;        // LEGACY
    uint16_t item_id;
    uint32_t page_id;
};

// NEW:
struct Tuple {
    const uint8_t *data;
    uint32_t data_size;
    TID tid;             // NEW: TID struct

    // Legacy compatibility (remove in future):
    uint16_t item_id;    // Deprecated: use getSlot(tid)
    uint32_t page_id;    // Deprecated: use getPageNumber(tid)
};
```

#### 7.2. Update insertTuple()

```cpp
// OLD:
Status insertTuple(..., uint32_t *page_id_out, uint16_t *item_id_out, ...);

// NEW:
Status insertTuple(..., TID *tid_out, ...);

// Implementation:
TID tid = makeTID(tablespace_id, page_number, slot);
if (tid_out != nullptr) {
    *tid_out = tid;
}
```

#### 7.3. Update updateTuple()

```cpp
// OLD:
Status updateTuple(uint32_t page_id, uint16_t item_id, ...);

// NEW:
Status updateTuple(const TID &tid, ...);

// Implementation:
uint32_t page_id = static_cast<uint32_t>(getPageNumber(tid));
uint16_t item_id = getSlot(tid);
// Use page_id and item_id with existing code
```

#### 7.4. Update deleteTuple() and getTuple()

Same pattern as updateTuple().

#### 7.5. Update TableScan

```cpp
// TableScan::next() should return TID:
Status next(Tuple *tuple_out, ...);

// In implementation:
tuple_out->tid = makeTID(current_tablespace, current_page, current_slot);
```

---

### 8. GarbageCollector Migration (~2-3 hours)

**Files**:
- `include/scratchbird/core/garbage_collector.h`
- `src/core/garbage_collector.cpp`

#### 8.1. Update header

```cpp
#include "scratchbird/core/tid.h"

// Update method signatures:
std::vector<TID> collectDeadTuples(...);
uint64_t cleanIndexes(const std::vector<TID> &dead_tids, ...);
```

#### 8.2. Update collectDeadTuples()

```cpp
// When collecting dead tuples:
std::vector<TID> dead_tids;

// For each dead tuple found:
TID tid = tuple_hdr->getTID();  // Already returns TID struct!
dead_tids.push_back(tid);

return dead_tids;
```

#### 8.3. Update cleanIndexes()

```cpp
// Call removeDeadEntries() on each index:
for (auto &index : table_indexes) {
    uint64_t removed = 0;
    Status status = index->removeDeadEntries(dead_tids, &removed, nullptr, ctx);
    // ... handle status
}
```

---

### 9. Compilation Fixes (~4-6 hours)

After completing index/storage/GC migrations, attempt compilation:

```bash
cd build
cmake ..
make 2>&1 | tee compile_errors.txt
```

**Expected Errors**:
1. Type mismatches (uint64_t vs TID)
2. Missing conversion calls
3. Variable name mismatches

**Strategy**:
1. Fix errors file-by-file
2. Start with the first error reported
3. Fix all errors in that file
4. Recompile
5. Repeat

**Common Fixes**:

```cpp
// Error: cannot convert 'TID' to 'uint64_t'
// Fix: Add conversion
uint64_t legacy = convertTIDtoLegacy(tid);

// Error: cannot convert 'uint64_t' to 'TID'
// Fix: Add conversion
TID tid = convertLegacyTID(legacy_tid);

// Error: no matching function for call to 'insert(std::vector<uint8_t>&, uint64_t&)'
// Fix: Convert parameter
TID tid = convertLegacyTID(tuple_id);
status = index->insert(key, tid, ctx);
```

---

### 10. Test Suite Updates (~3-5 hours)

**Strategy**: Fix tests incrementally as components complete.

#### 10.1. Index Unit Tests

**Files**: `tests/unit/test_btree.cpp`, `test_hash_index.cpp`, etc.

**Pattern**:
```cpp
// OLD:
uint64_t tuple_id = (page_id << 32) | (item_id << 16);
status = btree->insert(key, tuple_id, &ctx);

// NEW:
TID tid = makeTID(PRIMARY_TABLESPACE_ID, page_id, item_id);
status = btree->insert(key, tid, &ctx);

// OLD:
std::vector<uint64_t> results;
status = btree->search(key, nullptr, &results, &ctx);

// NEW:
std::vector<TID> results;
status = btree->search(key, nullptr, &results, &ctx);
```

#### 10.2. StorageEngine Tests

```cpp
// OLD:
uint32_t page_id;
uint16_t item_id;
status = storage->insertTuple(table_id, data, size, &page_id, &item_id, &ctx);

// NEW:
TID tid;
status = storage->insertTuple(table_id, data, size, &tid, &ctx);

// To get components:
uint32_t page_id = static_cast<uint32_t>(getPageNumber(tid));
uint16_t item_id = getSlot(tid);
```

#### 10.3. Integration Tests

Update all integration tests that use indexes or storage engine APIs.

#### 10.4. New TID Tests

Create `tests/unit/test_tid.cpp`:

```cpp
TEST(TIDTest, CreateAndExtract) {
    TID tid = makeTID(1, 12345, 67);
    EXPECT_EQ(getTablespaceID(tid), 1);
    EXPECT_EQ(getPageNumber(tid), 12345);
    EXPECT_EQ(getSlot(tid), 67);
}

TEST(TIDTest, LegacyConversion) {
    // Test conversion for tablespace 0
    TID tid = makeTID(0, 100, 200);
    uint64_t legacy = convertTIDtoLegacy(tid);
    TID tid2 = convertLegacyTID(legacy);
    EXPECT_EQ(tid, tid2);
}

TEST(TIDTest, CustomTablespaceNoLegacy) {
    // Custom tablespace cannot convert to legacy
    TID tid = makeTID(5, 100, 200);
    uint64_t legacy = convertTIDtoLegacy(tid);
    EXPECT_EQ(legacy, 0);  // Cannot convert
}

TEST(TIDTest, HashFunction) {
    TID tid1 = makeTID(1, 100, 50);
    TID tid2 = makeTID(1, 100, 50);
    TID tid3 = makeTID(1, 100, 51);

    std::hash<TID> hasher;
    EXPECT_EQ(hasher(tid1), hasher(tid2));
    EXPECT_NE(hasher(tid1), hasher(tid3));
}
```

---

### 11. Database Migration Tool (~2-3 hours)

**File**: `src/core/database_migration.cpp` (new file)

#### 11.1. Add database version to DatabaseHeader

```cpp
// In include/scratchbird/core/ondisk.h:

struct DatabaseHeader {
    // ... existing fields ...
    uint32_t database_version;  // NEW: 0 = legacy, 1 = GPID-based
    uint8_t reserved_migration[28];  // Padding for future use
};
```

#### 11.2. Implement version detection

```cpp
// In Database::open():

if (header->database_version == 0) {
    LOG_WARNING(STORAGE, "Database uses legacy format (version 0). "
                         "Upgrade required for tablespace support.");

    // Option 1: Auto-upgrade (risky)
    // Status status = upgradeDatabaseFormat(ctx);

    // Option 2: Require manual upgrade (safer for ALPHA)
    SET_ERROR_CONTEXT(ctx, Status::VERSION_MISMATCH,
                      "Database format upgrade required. "
                      "Run: scratchbird_upgrade /path/to/database.sbdb");
    return Status::VERSION_MISMATCH;
}
```

#### 11.3. Implement upgrade tool

**Note**: In ALPHA, we can skip this - just document that old databases are incompatible.

**For BETA**: Implement full upgrade:

```cpp
Status upgradeDatabaseToGPIDFormat(const std::string &db_path, ErrorContext *ctx) {
    // 1. Open database in compatibility mode
    // 2. Scan all heap pages
    // 3. Convert TupleHeader from 36 to 44 bytes
    // 4. Rebuild all indexes (simpler than converting in-place)
    // 5. Update database_version to 1
    // 6. Fsync all changes
    // 7. Close database
}
```

**For ALPHA**: Just document:

```markdown
# Upgrading from Alpha 1.2 to Alpha 1.3

**BREAKING CHANGE**: TupleHeader format changed from 36 to 44 bytes.

Existing databases from Alpha 1.2 are **NOT COMPATIBLE** with Alpha 1.3.

To migrate:
1. Export all data: `scratchbird_export old_database.sbdb > data.sql`
2. Create new database: `scratchbird_create new_database.sbdb`
3. Import data: `scratchbird_import new_database.sbdb < data.sql`

Automatic migration will be available in BETA.
```

---

## Code Templates

### Template 1: Index Header Update

```cpp
#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/tid.h"  // ADD THIS
#include "scratchbird/core/index_gc_interface.h"
// ... other includes ...

class MyIndex : public IndexGCInterface {
public:
    // Update all these signatures:
    Status insert(const std::vector<uint8_t> &key, const TID &tid, ErrorContext *ctx = nullptr);
    Status search(const std::vector<uint8_t> &key, struct Snapshot *snapshot,
                  std::vector<TID> *tids_out, ErrorContext *ctx = nullptr);
    Status remove(const std::vector<uint8_t> &key, const TID &tid, ErrorContext *ctx = nullptr);

    // IndexGCInterface implementation:
    Status removeDeadEntries(const std::vector<TID> &dead_tids,
                            uint64_t *entries_removed_out = nullptr,
                            uint64_t *pages_modified_out = nullptr,
                            ErrorContext *ctx = nullptr) override;

    const char *indexTypeName() const override { return "MyIndex"; }
};
```

### Template 2: Index insert() Implementation

```cpp
Status MyIndex::insert(const std::vector<uint8_t> &key, const TID &tid, ErrorContext *ctx) {
    // 1. Find page for insertion
    uint64_t page_num = findPageForKey(key);

    // 2. Pin page
    void *page_data;
    Status status = buffer_pool_->pinPage(page_num, &page_data, ctx);
    if (status != Status::OK) return status;

    // 3. Convert TID to legacy format for storage
    uint64_t legacy_tid = convertTIDtoLegacy(tid);
    if (legacy_tid == 0) {
        // Custom tablespace - for now, reject
        buffer_pool_->unpinPage(page_num, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "Custom tablespace indexes not yet supported");
        return Status::NOT_IMPLEMENTED;
    }

    // 4. Store legacy_tid in on-disk structure
    status = addEntryToPage(page_data, key, legacy_tid);

    // 5. Unpin page
    buffer_pool_->unpinPage(page_num, status == Status::OK, ctx);

    return status;
}
```

### Template 3: Index search() Implementation

```cpp
Status MyIndex::search(const std::vector<uint8_t> &key, Snapshot *snapshot,
                      std::vector<TID> *tids_out, ErrorContext *ctx) {
    // 1. Find page
    uint64_t page_num = findPageForKey(key);

    // 2. Pin page
    void *page_data;
    Status status = buffer_pool_->pinPage(page_num, &page_data, ctx);
    if (status != Status::OK) return status;

    // 3. Search page for matching keys
    std::vector<uint64_t> legacy_tids;
    bool found = searchPageForKey(page_data, key, &legacy_tids);

    // 4. Convert legacy TIDs to TID structs
    for (uint64_t legacy_tid : legacy_tids) {
        TID tid = convertLegacyTID(legacy_tid);
        tids_out->push_back(tid);
    }

    // 5. Unpin page
    buffer_pool_->unpinPage(page_num, false, ctx);

    // 6. MVCC filtering (optional - usually done at storage layer)
    (void)snapshot;  // Not used in index layer for Firebird MGA

    return found ? Status::OK : Status::NOT_FOUND;
}
```

### Template 4: Index removeDeadEntries() Implementation

```cpp
Status MyIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                 uint64_t *entries_removed_out,
                                 uint64_t *pages_modified_out,
                                 ErrorContext *ctx) {
    if (dead_tids.empty()) {
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;
        return Status::OK;
    }

    // 1. Convert TIDs to legacy format and build lookup set
    std::set<uint64_t> dead_set;
    for (const TID &tid : dead_tids) {
        uint64_t legacy = convertTIDtoLegacy(tid);
        if (legacy != 0) {  // Skip custom tablespace TIDs
            dead_set.insert(legacy);
        }
    }

    // 2. Scan entire index and remove matching entries
    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    // Implementation depends on index structure
    // For B-Tree: scan all leaf pages
    // For Hash: scan all buckets
    // For GIN: scan posting lists

    // 3. Return statistics
    if (entries_removed_out) *entries_removed_out = entries_removed;
    if (pages_modified_out) *pages_modified_out = pages_modified;

    return Status::OK;
}
```

---

## Testing Strategy

### Incremental Testing

After each component migration:

```bash
# 1. Attempt compilation
cd build && make 2>&1 | tee ../errors.log

# 2. Fix compilation errors
# (Edit files based on errors.log)

# 3. Run unit tests for that component
ctest -R test_btree --output-on-failure

# 4. If tests pass, move to next component
```

### Integration Testing

After all components migrated:

```bash
# 1. Full build
cd build && cmake .. && make

# 2. Run all unit tests
ctest --output-on-failure

# 3. Run integration tests
ctest -R integration --output-on-failure

# 4. Memory leak check
valgrind --leak-check=full ./build/scratchbird_tests

# 5. Thread safety check
valgrind --tool=helgrind ./build/scratchbird_tests
```

---

## Troubleshooting

### Problem 1: "Cannot convert TID to uint64_t"

**Cause**: Trying to use TID where uint64_t expected

**Fix**: Add conversion call:
```cpp
uint64_t legacy = convertTIDtoLegacy(tid);
```

### Problem 2: "Cannot convert uint64_t to TID"

**Cause**: Trying to use uint64_t where TID expected

**Fix**: Add conversion call:
```cpp
TID tid = convertLegacyTID(legacy_tid);
```

### Problem 3: "Custom tablespace TID converts to 0"

**Cause**: `convertTIDtoLegacy()` returns 0 for non-primary tablespaces

**Fix**: Either:
1. Reject custom tablespaces (ALPHA approach):
   ```cpp
   if (getTablespaceID(tid) != PRIMARY_TABLESPACE_ID) {
       return Status::NOT_IMPLEMENTED;
   }
   ```

2. Update on-disk format to store TID struct (BETA approach)

### Problem 4: "On-disk format incompatible"

**Cause**: Stored uint64_t, but TID is 80 bits

**Fix**: Keep storing uint64_t (legacy format), convert at API boundaries

### Problem 5: "Test failures after migration"

**Diagnosis**:
```bash
# Run with verbose output:
ctest -V -R test_name

# Run single test with gdb:
gdb --args ./build/test_name
```

**Common Issues**:
- Forgot to update test code
- TID conversion missing
- On-disk format mismatch

---

## Completion Checklist

### Index Layer

- [ ] B-Tree: All methods migrated and tested
- [ ] Hash: All methods migrated and tested
- [ ] GIN: All methods migrated and tested
- [ ] Bitmap: All methods migrated and tested
- [ ] BRIN: All methods migrated and tested
- [ ] HNSW: All methods migrated and tested

### Core APIs

- [ ] StorageEngine: All methods migrated
- [ ] GarbageCollector: All methods migrated
- [ ] Tuple struct updated

### Compilation

- [ ] Full codebase compiles with no errors
- [ ] No warnings related to TID migration
- [ ] All conversion calls in place

### Testing

- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] New TID tests created and passing
- [ ] No memory leaks (valgrind clean)
- [ ] No threading issues (helgrind clean)

### Documentation

- [ ] TABLESPACE_IMPLEMENTATION_PLAN.md updated
- [ ] PROJECT_CONTEXT.md updated
- [ ] Migration notes documented
- [ ] Breaking changes documented for ALPHA users

---

## Final Validation

Before marking Phase 1.5 complete:

```bash
# 1. Clean build
rm -rf build && mkdir build && cd build

# 2. Full compilation
cmake .. && make -j$(nproc)

# 3. All tests
ctest --output-on-failure

# 4. Memory checks
valgrind --leak-check=full --show-leak-kinds=all ./scratchbird_tests

# 5. Thread safety
valgrind --tool=helgrind ./scratchbird_tests

# 6. Verify no manual conversions in hot paths
grep -r "convertTIDtoLegacy\|convertLegacyTID" src/core/*.cpp | wc -l
# Should be minimal - only at API boundaries

# 7. Verify type safety
grep -r "uint64_t tuple_id\|uint64_t new_tuple_id" include/scratchbird/core/*.h
# Should return 0 results (all should be TID)
```

---

## Estimated Timeline

**Total**: 25-35 hours

| Task | Hours | Cumulative |
|------|-------|------------|
| Complete B-Tree | 3 | 3h |
| Hash Index | 3 | 6h |
| Bitmap Index | 2 | 8h |
| HNSW Index | 2 | 10h |
| BRIN Index | 2 | 12h |
| GIN Index | 4 | 16h |
| StorageEngine | 3 | 19h |
| GarbageCollector | 2 | 21h |
| Compilation Fixes | 4 | 25h |
| Test Updates | 4 | 29h |
| Migration Tool | 2 | 31h |
| Documentation | 1 | 32h |
| Final Validation | 2 | 34h |

**Recommended Schedule**:
- **Days 1-2**: Indexes (B-Tree, Hash, Bitmap)
- **Days 3-4**: Indexes (HNSW, BRIN, GIN)
- **Day 5**: Core APIs (StorageEngine, GarbageCollector)
- **Day 6**: Compilation fixes and test updates
- **Day 7**: Migration tool, documentation, validation

---

**END OF COMPLETION GUIDE**

*Created: 2025-10-20*
*Last Updated: 2025-10-20 23:45 UTC*
*For: Phase 1.5 TID Migration to GPID-based Format*
