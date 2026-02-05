# Deprecated Tests

This directory is reserved for tests that are temporarily deprecated due to API changes or incomplete implementations.

## Current Status

**All deprecated tests have been restored!** (As of February 2026)

The following test files were successfully converted from deprecated standalone tests
to GoogleTest format and moved to appropriate locations:

### GIN Index Tests (Restored)
- `test_gin_basic.cpp` → `../unit/gin/test_gin_basic.cpp` (5 tests)
- `test_gin_phase3.cpp` → `../unit/gin/test_gin_phase3.cpp` (7 tests)
- `test_gin_posting_tree.cpp` → `../unit/gin/test_gin_posting_tree.cpp` (7 tests)

### Deadlock Detection Tests (Restored)
- `test_deadlock_detection.cpp` → Merged into `../unit/test_deadlock_detection.cpp` (9 tests total)
- `test_transaction_deadlock.cpp` → Removed (content already in `test_transaction_deadlock_simple.cpp`)

### Page Manager Tests (Restored)
- `test_page_manager_overflow.cpp` → `../unit/test_page_manager_overflow.cpp`
- `test_page_manager_race.cpp` → `../unit/test_page_manager_race.cpp`

### Tuple Tests (Restored)
- `test_tuple_alignment.cpp` → `../unit/test_tuple_alignment.cpp` (4 tests)
- `test_tuple_size_validation.cpp` → `../unit/test_tuple_size_validation.cpp` (5 tests)

### Index Tests (Restored)
- `test_cross_page_updates.cpp` → `../unit/test_cross_page_updates.cpp` (7 tests)

### Index Tests (Still Deprecated - Complex API Changes Required)
- `test_index_updates_crosspage.cpp` - Requires significant API updates:
  - ConnectionContext initialization pattern changed
  - createIndex() signature changed (added tablespace_id parameter)
  - IndexInfo structure changed (root_page removed)
  - BTree::open() and HashIndex::open() signatures changed
  - TID format changed from uint64_t to struct with GPID

### Storage Tests (Restored)
- `test_heap_free_space.cpp` → `../unit/test_heap_free_space.cpp` (5 tests)
- `test_hint_bits.cpp` → `../unit/test_hint_bits.cpp` (5 tests)
- `test_overflow_fix.cpp` → `../unit/test_overflow_fix.cpp` (9 tests)

## API Migration Pattern

When restoring deprecated tests, use this pattern:

```cpp
// OLD (deprecated):
Database *db = Database::create(path, page_size, &ctx);
assert(db != nullptr);
// ... use db ...
delete db;
std::remove(db_path);

// NEW (correct):
scratchbird::testing::TestDatabaseFile db_file("test_name");
Database db;
ErrorContext ctx;
ASSERT_EQ(Database::create(db_file.path(), page_size, &ctx), Status::OK);
ASSERT_EQ(db.open(db_file.path(), &ctx), Status::OK);
// ... use &db ...
// File auto-deleted when db_file goes out of scope
```

## How to restore a test:

1. Update the test to use current APIs (see pattern above)
2. Convert from `int main()` with `assert()` to GoogleTest `TEST_F` format
3. Move it to the appropriate subdirectory under `tests/unit/`
4. Remove the original file from this directory
5. Update this README
6. Run `cmake --build . && ctest` to verify it passes
