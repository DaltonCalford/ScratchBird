# Test Suite Failure Analysis - 2025-12-27

**Test Run:** Full regression suite (1,346 tests)
**Date:** 2025-12-27
**Status:** 1 failed, 4 timed out, 12 not built

---

## Executive Summary

**Test Results:**
- ✅ **1,329 passed** (98.7%)
- ❌ **1 failed** (0.07%) - BytecodeOpcodesTest.SBLRVersionIsDefined
- ⏱️ **4 timed out** (0.3%) - All StoredCodeDependencyTest tests
- ⚠️ **12 not run** (0.9%) - Missing executables

**Overall Assessment:** High pass rate with minor issues requiring attention.

---

## Failure #1: BytecodeOpcodesTest.SBLRVersionIsDefined

### Details

**Test Location:** `tests/unit/test_bytecode_opcodes.cpp:408`
**Failure Type:** Assertion failure (version mismatch)
**Status:** 🟡 Build artifact issue (NOT a code bug)

### Error Output

```
/home/dcalford/CliWork/ScratchBird/tests/unit/test_bytecode_opcodes.cpp:398: Failure
Expected equality of these values:
  SBLR_VERSION
    Which is: '\x2' (2)
  1
```

### Root Cause

**Source-Build Mismatch:**
- **Compiled test binary** expects `SBLR_VERSION = 1` (compiled at line 398)
- **Current source code** expects `SBLR_VERSION = 2` (currently at line 408)
- **Actual value** is `SBLR_VERSION = 2`

The source file was modified after the last build:
- Line numbers shifted (398 → 408)
- Expected value changed (1 → 2)
- Test was correctly updated but binary not rebuilt

### Current Source Code

```cpp
// Line 406-409 in tests/unit/test_bytecode_opcodes.cpp
TEST(BytecodeOpcodesTest, SBLRVersionIsDefined)
{
    EXPECT_EQ(SBLR_VERSION, 2);  // ✅ Now expects 2
}
```

### Impact

**Severity:** LOW
- Test will PASS when rebuilt
- No code changes required
- Source code is correct

### Recommendation

**Action:** Rebuild test suite
```bash
cd build
cmake --build . --target scratchbird_tests
```

Expected outcome: Test will pass after rebuild.

---

## Failure #2-5: StoredCodeDependencyTest Timeouts (4 tests)

### Details

**Test Location:** `tests/unit/test_code_dependencies.cpp`
**Failure Type:** Timeout after 300 seconds
**Status:** 🔴 Likely deadlock or infinite loop

### Failed Tests

All 4 tests timed out:
1. `StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction` (line 210)
2. `StoredCodeDependencyTest.DropProcedureFailsIfCalled` (line 274)
3. `StoredCodeDependencyTest.ComplexFunctionChain` (line 360)
4. `StoredCodeDependencyTest.MixedFunctionProcedureDependencies` (line 437)

### Error Pattern

All 4 tests exhibited the same repeating error:
```
[ERROR] [TRANSACTION] [long_transaction_monitor.cpp:267]
Failed to get active backends for long transaction check
```

This error repeated continuously until the 300-second timeout.

### Test Structure Analysis

**Common pattern in all 4 tests:**
1. Create database objects (tables, functions, procedures)
2. Create dependency relationships between objects
3. **Attempt to drop an object** that has dependencies
4. **Expect Status::CONSTRAINT_VIOLATION**

Example from `DropFunctionFailsIfCalledByAnotherFunction`:
```cpp
// Create base function
ID base_func_id = createTestFunction("get_order_count", ...);

// Create another function that depends on the first
ID caller_func_id = createTestFunction("check_orders", ...);

// Try to drop base function - should fail
Status status = catalog->dropFunction("get_order_count", false, &ctx);
EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);  // ⏱️ Hangs here
```

### Root Cause Analysis

**Probable Issue:** `catalog->dropFunction()` (and similar drop operations) are not returning when dependency constraints exist.

**Hypothesis:**
1. Drop operation starts
2. Discovers dependencies
3. **Deadlock or infinite loop** in dependency check or transaction management
4. Long transaction monitor triggers (transaction exceeds threshold)
5. Monitor tries to check active backends
6. **Monitor fails** (possibly trying to access locked catalog)
7. Error repeats in loop until timeout

**Evidence:**
- All 4 failures occur during drop operations with dependencies
- All show long transaction monitor errors
- Tests that drop without dependencies likely pass (e.g., test names suggest other passing tests do successful drops)

### Suspected Code Location

**Primary suspect:** `src/core/catalog_manager.cpp` - drop operations
- `dropFunction()`
- `dropProcedure()`
- `dropTable()`

**Secondary suspect:** `src/core/long_transaction_monitor.cpp:267`
- Why is "get active backends" failing?
- Possible catalog lock contention

### Impact

**Severity:** MEDIUM
- Tests hang indefinitely (capped at 300s timeout)
- Suggests potential production deadlock scenario
- Only affects dependency-protected drop operations
- May not occur in normal use (tests create artificial dependency chains)

### Recommendation

**Immediate Action:** Investigate `catalog->dropFunction()` implementation
1. Check for deadlock conditions when dependencies exist
2. Verify transaction handling in drop operations
3. Examine lock acquisition order
4. Review long transaction monitor's catalog access

**Debugging Steps:**
```bash
# Run single test with timeout trace
cd build
timeout 30 ./tests/scratchbird_tests \
  --gtest_filter="StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction" \
  --gtest_break_on_failure

# Attach debugger if test hangs
gdb ./tests/scratchbird_tests
(gdb) run --gtest_filter="StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction"
# When it hangs:
(gdb) thread apply all bt
```

**Files to Examine:**
1. `src/core/catalog_manager.cpp` - drop operations with dependency checks
2. `src/core/long_transaction_monitor.cpp:267` - getActiveBackends failure
3. `src/core/transaction_manager.cpp` - transaction lifecycle
4. `tests/unit/test_code_dependencies.cpp` - test setup (verify not test issue)

---

## Issue #3: Missing Test Executables (12 tests)

### Details

**Status:** ⚠️ Not built (may be intentional)
**Severity:** LOW (informational)

### Tests Marked "Not Run"

```
test_postgresql_parser_NOT_BUILT
test_gist_mvcc
test_check_constraints
test_foreign_keys
test_join_ordering
test_mv_rewriter
test_brin_mvcc
test_brin_dml
test_composite_fk
test_exception_safety
test_concurrent_page_access
test_buffer_pool_exhaustion
test_index_dml_integration
test_gin_dml
test_heap_page_ownership
test_fulltext_gc
test_columnstore_persistence
```

### Analysis

**Observations:**
- Test name `test_postgresql_parser_NOT_BUILT` suggests intentional exclusion
- Some tests may be for features not yet implemented
- Others may have build configuration flags disabling them
- No corresponding executable files in `build/tests/`

### Impact

**Severity:** LOW
- Tests are not built, not failing
- May indicate incomplete feature coverage
- Could be deferred implementation tests

### Recommendation

**Action:** Document build configuration
1. Identify which tests are intentionally disabled
2. Document reasons (features not implemented, conditional builds, etc.)
3. Consider adding CMake options to enable/disable test groups

**Not urgent** - informational only.

---

## Summary of Recommendations

### Priority 1: Rebuild Tests (Quick Fix)

```bash
cd build
cmake --build . --target scratchbird_tests
ctest -R BytecodeOpcodesTest.SBLRVersionIsDefined
```

**Expected:** Test passes
**Effort:** < 5 minutes

### Priority 2: Investigate Dependency Drop Deadlocks (Critical)

**Files to review:**
- `src/core/catalog_manager.cpp` - dropFunction(), dropProcedure(), dropTable()
- `src/core/long_transaction_monitor.cpp` - getActiveBackends()
- Transaction lock ordering

**Investigation time:** 1-2 hours
**Fix time:** Unknown (depends on complexity)

**Risk:** Medium - suggests potential production deadlock scenario

### Priority 3: Document Missing Tests (Low)

**Action:** Create test matrix showing:
- Which tests are intentionally disabled
- Why (features not implemented, conditional compilation, etc.)
- When they should be enabled

**Effort:** 30 minutes - 1 hour

---

## Test Suite Health

**Overall:** 98.7% pass rate is excellent

**Issues:**
1. ✅ One build artifact issue (trivial fix)
2. ⚠️ Potential deadlock in dependency tracking (requires investigation)
3. ℹ️ Some tests not built (may be intentional)

**Recommendation:** Safe to proceed with development while investigating the dependency drop deadlock in parallel.

---

**Analysis Date:** 2025-12-27
**Analyzed By:** Claude Code
**Next Review:** After deadlock investigation complete
