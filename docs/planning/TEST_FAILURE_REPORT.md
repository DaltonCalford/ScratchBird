# Test Failure Report - High Priority

**Date:** 2025-11-27
**Priority:** HIGH - Must be resolved before next phase

## Executive Summary

After fixing API compilation issues in test files, the build succeeds (100%) but several integration tests fail at runtime with "Catalog heap page full" errors. This indicates a storage/catalog system issue that prevents database creation or opening.

---

## Test Results Overview

### Passing Tests (Unit Tests)
| Test | Result |
|------|--------|
| test_range_types | 77/77 PASSED |
| test_network_types | 77/77 PASSED |
| test_range_operators | 51/51 PASSED |
| test_range_lexer | PASSED |
| test_temporal_range_types | PASSED |

### Failing Tests (Integration Tests)
| Test | Failures | Error |
|------|----------|-------|
| test_gist_mvcc | 8/8 FAILED | Catalog heap page full |
| TSAN_TransactionCacheRace | 1/1 FAILED | Status::OK mismatch |
| TSAN_LockOrdering | 3/3 FAILED | Status::OK mismatch |
| test_columnstore_rle | ABORTED | Failed to create test database |

---

## Detailed Failure Analysis

### Primary Error: "Catalog heap page full"

**Affected Tests:**
- GiSTMVCCTest.EmptyTreeSearch
- GiSTMVCCTest.SingleElementMGAVisibility
- GiSTMVCCTest.MultipleElementsOverlapSearch
- GiSTMVCCTest.LogicalDeletionXmax
- GiSTMVCCTest.RepeatableReadIsolation
- GiSTMVCCTest.GarbageCollectionDeadEntries
- GiSTMVCCTest.TransactionIdParameterValidation
- GiSTMVCCTest.OperatorClassFramework

**Error Message:**
```
Expected equality of these values:
  status
    Which is: 4-byte object <EE-03 00-00>
  Status::OK
    Which is: 4-byte object <00-00 00-00>
Failed to open database: Catalog heap page full
```

**Location:** `/home/dcalford/CliWork/ScratchBird/tests/integration/test_gist_mvcc.cpp:207`

**Root Cause Hypothesis:**
The catalog system's heap page allocation is failing. This could be caused by:
1. Insufficient initial page allocation for catalog data
2. FSM (Free Space Map) not properly tracking available space
3. Page size configuration issue (currently 8192 bytes)
4. Catalog bootstrap process not allocating enough pages

---

### Secondary Error: TSAN Test Failures

**Affected Tests:**
- TSANTransactionCacheTest.ConcurrentCacheQueries
- TSANLockOrderingTest.ConcurrentTransactionLifecycle
- TSANLockOrderingTest.MixedOperations
- TSANLockOrderingTest.HighContentionStress

**Error Pattern:**
```
Expected equality of these values:
  status
    Which is: 4-byte object <EE-03 00-00>
  Status::OK
    Which is: 4-byte object <00-00 00-00>
```

**Note:** Status code `0x03EE` = 1006 decimal - need to check error code enum to identify specific error.

---

## Actions Attempted

### 1. API Fixes Applied (Successful)
- Fixed TransactionManager API: `beginTransaction(proc_id, xid_out, ctx)` signature
- Fixed TID constructor: `TID(tablespace_id, page_number, slot)`
- Fixed GiSTIndex::search: Changed from `results` to `&results` (pointer)
- Fixed UUID generation: `UuidV7::generateBytes()` → `generateUuidV7()`
- Added ProcArrayManager registration in test SetUp/TearDown
- Removed duplicate main() functions from test files

### 2. Build Result
```
[100%] Built target sb_timezone_loader
```
All 987 tests registered, build successful.

### 3. Test Execution
- Unit tests (range, network types) execute and pass
- Integration tests fail during database setup phase
- Error occurs in `Database::open()` or `Database::create()`

---

## Recommended Investigation Steps

### Step 1: Identify Error Code
Check `/include/scratchbird/core/status.h` or similar to map `0x03EE` (1006) to specific error type.

### Step 2: Trace Catalog Heap Page Full
1. Search for "Catalog heap page full" error message in codebase
2. Identify which function generates this error
3. Check heap page allocation logic in catalog manager

### Step 3: Check FSM Reconstruction
The logs show FSM reconstruction completing:
```
[INFO] [STORAGE] [page_manager.cpp:568] FSM reconstruction complete: 3 allocated, 0 free, 0 empty, 0 corrupt
```
- Only 3 pages allocated
- 0 free pages available
- May need more initial pages for catalog

### Step 4: Review Database Creation
Check `Database::create()` in:
- `/src/core/database.cpp`
- `/include/scratchbird/core/database.h`

Verify initial page allocation is sufficient for catalog bootstrap.

---

## Files to Investigate

| File | Reason |
|------|--------|
| `src/core/database.cpp` | Database::create() and open() |
| `src/core/catalog_manager.cpp` | Catalog heap page allocation |
| `src/core/page_manager.cpp` | FSM reconstruction, page allocation |
| `src/core/heap_page.cpp` | Heap page full condition |
| `include/scratchbird/core/status.h` | Error code definitions |

---

## Temporary Workarounds Applied

1. **wave1_tests disabled** - TypedValue::makePoint API needs updating
2. **Many integration tests excluded** - Listed in CMakeLists.txt with exclusion patterns

---

## Priority Actions

1. **CRITICAL**: Fix "Catalog heap page full" error - blocks all integration tests
2. **HIGH**: Map error code 1006 to understand root cause
3. **MEDIUM**: Review catalog bootstrap page allocation
4. **LOW**: Fix wave1_tests TypedValue API issues

---

## Test Commands for Verification

```bash
# Run specific failing test with verbose output
./tests/test_gist_mvcc --gtest_filter=GiSTMVCCTest.EmptyTreeSearch

# Run passing unit tests to verify build
./tests/test_range_types
./tests/test_network_types

# Check all registered tests
ctest -N | tail -30
```

---

## Related Documentation

- `/docs/planning/CATALOG_CLEANUP_OVERVIEW.md` - Catalog system plans
- `/MGA_RULES.md` - Transaction manager rules
- `/PROJECT_CONTEXT.md` - Overall project status
