# Critical Fixes Test Plan - Alpha 1.04 Storage Engine

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


## Overview
This document describes the tests created to verify the critical fixes required for the Storage Engine implementation. These tests are designed to **FAIL** with the current implementation and **PASS** once Agent A applies the fixes.

## Critical Issues to Fix

### 1. Memory Leak in HeapScanIterator (Line 287-293)
**Problem**: Creates a new StorageEngine instance for every tuple during scanning
```cpp
// CURRENT BUGGY CODE:
StorageEngine* engine = new(std::nothrow) StorageEngine(db_);
if (!engine) {
    SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to create StorageEngine");
    return Status::OOM;
}
if (engine->is_visible(hdr->xmin, hdr->xmax, engine->get_current_xid())) {
    // ... use engine ...
}
delete engine;  // This line is missing!
```

**Expected Fix**: 
- Option 1: Use the parent StorageEngine instance passed to the iterator
- Option 2: Make is_visible() a static method
- Option 3: Store StorageEngine reference in HeapScanIterator constructor

### 2. Buffer Overflow in HeapPage::insert_tuple (Line 77-78)
**Problem**: Assumes tuple_data size includes TupleHeader, causing potential buffer overflow
```cpp
// CURRENT BUGGY CODE:
memcpy(page_data_ + tuple_offset + sizeof(TupleHeader), 
       tuple_data, tuple_size - sizeof(TupleHeader));
```

**Issues**:
- If `tuple_size < sizeof(TupleHeader)`, causes integer underflow
- If `tuple_data` is raw data without header space, reads past buffer
- API contract is unclear about what `tuple_size` represents

**Expected Fix**:
- Option 1: Change API to accept raw data and size separately
- Option 2: Validate `tuple_size >= sizeof(TupleHeader)` before subtraction
- Option 3: Clarify API contract and add size validation

## Test Files Created

### 1. `test_storage_critical_fixes.cpp`
Contains 6 tests specifically for the critical issues:

#### Test 1: `HeapScanIterator_NoMemoryLeak`
- **Purpose**: Detect memory leak in scan operations
- **Method**: Performs multiple scans and measures memory growth
- **Expected**: Currently FAILS with ~10MB+ memory leak
- **Pass Criteria**: Memory growth < 10MB after 10 scans of 1000 tuples

#### Test 2: `HeapPage_InsertTuple_BufferOverflowProtection`
- **Purpose**: Detect buffer overflow vulnerability
- **Method**: Tests various tuple sizes including edge cases
- **Expected**: Currently FAILS or causes undefined behavior
- **Pass Criteria**: Proper validation of tuple sizes, clear API contract

#### Test 3: `HeapScanIterator_ProperEngineUsage`
- **Purpose**: Demonstrate correct StorageEngine usage pattern
- **Method**: Shows how iterator should use parent engine
- **Expected**: Tests the intended behavior after fix

#### Test 4: `HeapScanIterator_StressTestMemoryLeak`
- **Purpose**: Stress test to amplify memory leak
- **Method**: 100 scans of 500 tuples, periodic memory checks
- **Expected**: Currently FAILS with steady memory growth
- **Pass Criteria**: Stable memory usage across iterations

#### Test 5: `HeapPage_InsertTuple_BoundaryValidation`
- **Purpose**: Test boundary conditions for buffer overflow
- **Method**: Tests minimum sizes, exact header size, etc.
- **Expected**: Currently causes undefined behavior
- **Pass Criteria**: Proper handling of all edge cases

#### Test 6: `CombinedFixes_ScanAndInsert`
- **Purpose**: Test both fixes working together
- **Method**: Insert various tuples, scan multiple times
- **Expected**: Currently FAILS due to both issues
- **Pass Criteria**: Correct operation with stable memory

## How to Run the Tests

```bash
# Build the tests
cd /workspace/build
cmake --build . -j

# Run only the critical fixes tests
./tests/scratchbird_tests --gtest_filter="StorageCriticalFixesTest.*"

# Run with memory leak detection
valgrind --leak-check=full ./tests/scratchbird_tests --gtest_filter="StorageCriticalFixesTest.*"
```

## Expected Test Results

### Before Fixes (Current State):
```
[  FAILED  ] StorageCriticalFixesTest.HeapScanIterator_NoMemoryLeak
[  FAILED  ] StorageCriticalFixesTest.HeapPage_InsertTuple_BufferOverflowProtection
[  FAILED  ] StorageCriticalFixesTest.HeapScanIterator_StressTestMemoryLeak
[  FAILED  ] StorageCriticalFixesTest.HeapPage_InsertTuple_BoundaryValidation
[  FAILED  ] StorageCriticalFixesTest.CombinedFixes_ScanAndInsert
```

### After Fixes (Target State):
```
[  PASSED  ] All StorageCriticalFixesTest.* tests
```

## Recommended Fix Implementation

### Fix 1: HeapScanIterator Memory Leak
```cpp
// In HeapScanIterator constructor:
HeapScanIterator(Database* db, StorageEngine* engine, ID table_id, uint32_t start_page)
    : db_(db), engine_(engine), table_id_(table_id), ... {
    // Store reference to parent engine
}

// In next() method:
if (engine_->is_visible(hdr->xmin, hdr->xmax, engine_->get_current_xid())) {
    // Use stored engine reference, no allocation
}
```

### Fix 2: HeapPage Buffer Overflow
```cpp
Status HeapPage::insert_tuple(const uint8_t* tuple_data, uint32_t tuple_size,
                             uint32_t xmin, uint16_t* item_id_out,
                             ErrorContext* ctx) {
    // Add validation
    if (tuple_size < sizeof(TupleHeader)) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Tuple size must be at least TupleHeader size");
        return Status::InvalidArgument;
    }
    
    // Clear API: tuple_data is raw data, tuple_size includes header space
    uint32_t data_size = tuple_size - sizeof(TupleHeader);
    
    // ... rest of implementation
}
```

## Next Steps

1. Agent A should review these tests
2. Apply fixes to `storage_engine.cpp` and `heap_page.cpp`
3. Run the critical fixes tests to verify
4. Once all tests pass, proceed with performance and stress testing

## Notes

- These tests use `get_memory_usage()` which may need adjustment for different platforms
- Some tests may need `valgrind` or similar tools for detailed memory leak detection
- The buffer overflow tests may trigger AddressSanitizer if enabled
