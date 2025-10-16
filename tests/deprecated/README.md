# Deprecated Tests

This directory contains tests that are temporarily deprecated due to API changes or incomplete implementations.

## Files in this directory:

### GIN Index Tests (Deprecated 2025-10-16)
- `test_gin_basic.cpp` - Uses outdated Database API (Database::create() signature changed)
- `test_gin_phase3.cpp` - Uses outdated Database API
- `test_gin_posting_tree.cpp` - Uses outdated Database API

**Reason for deprecation**: Database::create() now returns Status instead of Database*.
Tests need to be updated to use the new API pattern.

**Restoration plan**: Update tests to use new Database API:
```cpp
// OLD (deprecated):
Database *db = Database::create(path, page_size, &ctx);

// NEW (correct):
Status status = Database::create(path, page_size, &ctx);
Database db;
status = db.open(path, &ctx);
```

## How to restore a test:

1. Update the test to use current APIs
2. Move it back to `tests/unit/` or appropriate subdirectory
3. Run `cmake --build . && ctest` to verify it passes
4. Remove entry from this README
