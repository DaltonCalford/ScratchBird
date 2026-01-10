# Phase 1.5: TID Migration - Quick Start

**For**: Developer resuming Phase 1.5 TID migration work
**Time to Complete**: 25-35 hours (5-7 days)
**Current Status**: 20% complete

---

## Start Here

### 1. Read This First (5 minutes)

**What is Phase 1.5?**
Migration from `uint64_t tuple_id` to `TID` struct throughout the entire codebase for multi-tablespace support.

**Why is it important?**
- Heap layer already uses GPID-based TID (44-byte TupleHeader)
- All indexes still use legacy uint64_t format
- Creates API boundary mismatch and type safety issues
- Must be completed before multi-tablespace features work

**What's been done?** (20%)
- ✅ Heap layer fully migrated
- ✅ TID infrastructure complete (tid.h)
- ✅ B-Tree header signatures updated
- ✅ Comprehensive documentation created
- ✅ Code templates and patterns documented

**What remains?** (80%)
- Complete B-Tree implementation
- Migrate 5 other index types
- Update StorageEngine and GarbageCollector
- Fix compilation errors
- Update test suite

---

### 2. Review Documentation (30 minutes)

**Read in this order**:

1. **PHASE_1_5_SUMMARY.md** (10 min) - Overview of what was accomplished
2. **PHASE_1_5_COMPLETION_GUIDE.md** (20 min) - Detailed step-by-step guide

**Optional**:
3. **PHASE_1_5_TID_MIGRATION_STATUS.md** - Full status report

---

### 3. Understand Current State (10 minutes)

**What compiles**:
- Heap layer (heap_page.h, heap_page.cpp)
- TID infrastructure (tid.h)
- B-Tree header (btree.h)

**What doesn't compile**:
- B-Tree implementation (btree.cpp) - partially done
- All other indexes - not started
- StorageEngine - not started
- GarbageCollector - not started
- Tests - need updates

**Key files changed so far**:
- `include/scratchbird/core/tid.h` - NEW, complete
- `include/scratchbird/core/heap_page.h` - TupleHeader updated
- `src/core/heap_page.cpp` - All methods updated
- `include/scratchbird/core/btree.h` - Signatures updated
- `src/core/btree.cpp` - 3/8 methods done
- `include/scratchbird/core/index_gc_interface.h` - Interface updated

---

### 4. Set Up Environment (5 minutes)

```bash
# 1. Navigate to project
cd /home/dcalford/CliWork/ScratchBird

# 2. Check git status
git status

# 3. Create a working branch
git checkout -b feature/phase-1.5-tid-migration

# 4. Verify current state
git diff HEAD

# 5. Attempt compilation to see current errors
cd build
cmake ..
make 2>&1 | head -50
```

---

### 5. Choose Your Starting Point

**Option A: Continue B-Tree** (Recommended)
- Already 10% done
- Familiar with the patterns
- Good warmup before other indexes
- Estimated: 3 hours

**Option B: Start with Simple Index**
- Bitmap index (simplest)
- Learn patterns on easier code
- Estimated: 2-3 hours

**Option C: Run Migration Script First**
- Automate mechanical changes
- Then fix remaining issues manually
- Faster but less control

---

### 6. Quick Reference - Code Patterns

#### Pattern 1: Convert TID to Legacy (for storage)

```cpp
// When you have TID but need to store uint64_t:
uint64_t legacy_tid = convertTIDtoLegacy(tid);

// Check for custom tablespace (ALPHA rejects these):
if (legacy_tid == 0) {
    return Status::NOT_IMPLEMENTED;
}

// Store legacy_tid in on-disk structure
```

#### Pattern 2: Convert Legacy to TID (when reading)

```cpp
// When reading uint64_t from disk:
uint64_t stored_tid = ...;  // Read from page

// Convert to TID before returning:
TID tid = convertLegacyTID(stored_tid);
tids_out->push_back(tid);
```

#### Pattern 3: Method Signature Update

```cpp
// OLD:
Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id, ...);

// NEW:
Status insert(const std::vector<uint8_t> &key, const TID &tid, ...);
```

#### Pattern 4: Variable Renames

```cpp
// Change all instances of:
uint64_t tuple_id          → const TID &tid
uint64_t new_tuple_id      → const TID &new_tid
std::vector<uint64_t>      → std::vector<TID>
*tuple_ids_out             → *tids_out
```

---

## Quick Start Workflows

### Workflow 1: Complete B-Tree (3 hours)

```bash
# 1. Open btree.cpp
vim src/core/btree.cpp  # or your favorite editor

# 2. Fix remove() method (search for "auto BTree::remove")
# - Change tuple_id variable to tid
# - Add: uint64_t legacy_tid = convertTIDtoLegacy(tid);
# - Use legacy_tid for comparison with stored values

# 3. Fix split_leaf_page() (search for "auto BTree::split_leaf_page")
# - Change all new_tuple_id to new_tid
# - Add conversion when storing: convertTIDtoLegacy(new_tid)

# 4. Fix removeDeadEntries() (search for "Status BTree::removeDeadEntries")
# - Build dead_set from vector<TID>:
#   for (const TID &tid : dead_tids) {
#       uint64_t legacy = convertTIDtoLegacy(tid);
#       if (legacy != 0) dead_set.insert(legacy);
#   }

# 5. Fix BTreeIterator::next() (search for "BTreeIterator::next")
# - When returning TID: *tid_out = convertLegacyTID(stored_tid);

# 6. Compile and test
cd build && make 2>&1 | grep -i error
```

### Workflow 2: Migrate Simple Index (2-3 hours)

```bash
# Example: Bitmap Index

# 1. Update header
vim include/scratchbird/core/bitmap_index.h
# Add: #include "scratchbird/core/tid.h"
# Change all method signatures (see Pattern 3 above)

# 2. Update implementation
vim src/core/bitmap_index.cpp
# For each method:
#   - Update signature
#   - Add conversions at API boundaries
#   - Use Pattern 1 (TID → uint64_t for storage)
#   - Use Pattern 2 (uint64_t → TID for retrieval)

# 3. Compile and fix errors
cd build && make 2>&1 | grep bitmap_index
```

### Workflow 3: Use Automation Script (1 hour setup + manual fixes)

```bash
# 1. Make script executable
chmod +x scripts/migrate_to_tid.sh

# 2. Run script (creates backups automatically)
./scripts/migrate_to_tid.sh

# 3. Review changes
git diff

# 4. Fix remaining issues manually
# Script handles:
#   - Signature updates
#   - Variable type changes
# You must add:
#   - Conversion calls (convertTIDtoLegacy, convertLegacyTID)
#   - Iterator updates
#   - On-disk format handling

# 5. Compile and iterate
cd build && make 2>&1 | tee ../errors.log
```

---

## Daily Progress Tracking

### Day 1 Goals (6-8 hours)
- [ ] Complete B-Tree migration
- [ ] Migrate Hash index
- [ ] First successful compilation (may have warnings)

### Day 2 Goals (6-8 hours)
- [ ] Migrate Bitmap index
- [ ] Migrate HNSW index
- [ ] Migrate BRIN index

### Day 3 Goals (6-8 hours)
- [ ] Migrate GIN index (most complex)
- [ ] Clean compilation (no errors)

### Day 4 Goals (6-8 hours)
- [ ] Migrate StorageEngine
- [ ] Migrate GarbageCollector
- [ ] All components compile cleanly

### Day 5 Goals (6-8 hours)
- [ ] Fix compilation errors across all files
- [ ] Achieve clean build

### Day 6 Goals (6-8 hours)
- [ ] Update test suite
- [ ] All tests compile
- [ ] Begin fixing test failures

### Day 7 Goals (4-6 hours)
- [ ] All tests passing
- [ ] Migration tool documentation
- [ ] Update TABLESPACE_IMPLEMENTATION_PLAN.md
- [ ] Final validation (valgrind, helgrind)

---

## Compilation Error Cheat Sheet

### Error: "cannot convert 'TID' to 'uint64_t'"

**Fix**:
```cpp
uint64_t legacy = convertTIDtoLegacy(tid);
```

### Error: "cannot convert 'uint64_t' to 'TID'"

**Fix**:
```cpp
TID tid = convertLegacyTID(legacy_tid);
```

### Error: "no matching function for call to 'insert'"

**Cause**: Passing uint64_t where TID expected

**Fix**:
```cpp
// Convert parameter first
TID tid = convertLegacyTID(tuple_id);
status = index->insert(key, tid, ctx);
```

### Error: "conflicting declaration of 'std::vector<TID>'"

**Cause**: Forgot to update variable name

**Fix**:
```cpp
// OLD:
std::vector<uint64_t> *tuple_ids_out

// NEW:
std::vector<TID> *tids_out
```

---

## Testing Quick Commands

```bash
# Compile only
cd build && make

# Compile with verbose errors
cd build && make 2>&1 | tee errors.log

# Run specific test
cd build && ctest -R test_btree --output-on-failure

# Run all tests
cd build && ctest --output-on-failure

# Memory leak check
valgrind --leak-check=full ./build/scratchbird_tests

# Thread safety check
valgrind --tool=helgrind ./build/scratchbird_tests
```

---

## Getting Unstuck

### Problem: Too many compilation errors

**Solution**: Fix one file at a time
```bash
# Compile just one file
cd build && make btree.cpp.o 2>&1 | head -50

# Fix errors in that file
vim src/core/btree.cpp

# Repeat until file compiles
```

### Problem: Don't understand the pattern

**Solution**: Look at completed examples
```bash
# See how heap_page.cpp was migrated:
git diff HEAD~10 src/core/heap_page.cpp

# See TID struct definition:
cat include/scratchbird/core/tid.h

# See conversion helpers:
grep -A10 "convertLegacyTID\|convertTIDtoLegacy" include/scratchbird/core/tid.h
```

### Problem: Test failing after migration

**Solution**: Debug systematically
```bash
# Run single test with verbose output
cd build && ctest -V -R test_name

# Run with gdb
gdb --args ./build/test_name

# Check for conversion issues
grep -n "convertTIDtoLegacy\|convertLegacyTID" src/core/file.cpp
```

---

## Success Indicators

You're on track if:
- ✅ Each component compiles cleanly before moving to next
- ✅ Conversion calls appear only at API boundaries
- ✅ No `uint64_t tuple_id` in method signatures
- ✅ Tests update incrementally as components complete
- ✅ Making steady progress (~1 component per day)

You're off track if:
- ❌ Hundreds of compilation errors accumulating
- ❌ Conversion calls scattered throughout function bodies
- ❌ Trying to change everything at once
- ❌ Skipping testing until the end

---

## When You're Done

```bash
# 1. Final clean build
rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc)

# 2. All tests pass
ctest --output-on-failure

# 3. No memory leaks
valgrind --leak-check=full ./scratchbird_tests 2>&1 | grep "no leaks"

# 4. No threading issues
valgrind --tool=helgrind ./scratchbird_tests 2>&1 | grep "ERROR SUMMARY: 0"

# 5. Update documentation
vim docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/TABLESPACE_IMPLEMENTATION_PLAN.md
# Mark Phase 1.5 as COMPLETE

# 6. Update project context
vim PROJECT_CONTEXT.md
# Update Phase 1.5 status

# 7. Commit
git add -A
git commit -m "Complete Phase 1.5: TID Migration to GPID-based Format

- Migrated all 6 index types to TID struct API
- Updated StorageEngine and GarbageCollector
- All tests passing
- Phase 1.5: COMPLETE"

# 8. Celebrate! 🎉
```

---

## Resources

**Documentation**:
- Completion Guide: `docs/guides/PHASE_1_5_COMPLETION_GUIDE.md`
- Status Report: `docs/status/PHASE_1_5_TID_MIGRATION_STATUS.md`
- Summary: `docs/status/PHASE_1_5_SUMMARY.md`

**Code**:
- TID struct: `include/scratchbird/core/tid.h`
- Heap example: `src/core/heap_page.cpp`
- B-Tree partial: `src/core/btree.cpp`

**Tools**:
- Migration script: `scripts/migrate_to_tid.sh`

---

## Quick Decision Matrix

| If you have... | Then start with... | Estimated time |
|----------------|-------------------|----------------|
| 3 hours | Complete B-Tree | 3h |
| 1 full day | B-Tree + Hash | 6-7h |
| 2-3 days | All indexes | 16-20h |
| 4-5 days | Indexes + APIs | 22-26h |
| 6-7 days | Full completion | 30-35h |

---

**Ready to start? Open the Completion Guide and begin with Day 1!**

`docs/guides/PHASE_1_5_COMPLETION_GUIDE.md`

---

**END OF QUICK START GUIDE**

*Last Updated: 2025-10-20 23:55 UTC*
*For: Phase 1.5 TID Migration*
*Status: 20% complete, ready for systematic completion*
