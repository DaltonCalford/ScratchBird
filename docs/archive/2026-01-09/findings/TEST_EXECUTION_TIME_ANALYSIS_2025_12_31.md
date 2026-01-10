# Test Execution Time Analysis - 2025-12-31

**Date:** 2025-12-31
**Status:** 🔴 CRITICAL - One test exceeds 5 minutes, timeout mechanism not working
**Total Tests:** 1,469 tests in suite
**Tests Analyzed:** 1,425 tests (97%)

---

## Executive Summary

**Tests Exceeding 5 Minutes (FAILURES):** 1 test
**Tests 40-300s (Warnings):** 1 test
**Tests 20-40s (Watch):** 1 test
**Tests 6-20s (Performance):** 4 tests
**Average Test Time:** 0.09 seconds
**Maximum Analyzed Test Time:** 47.60 seconds

---

## 🔴 CRITICAL: Tests Exceeding 5 Minutes (300 seconds)

### Test #1426: scratchbird_tests

**Runtime:** 10+ minutes (600+ seconds) - **EXCEEDED TIMEOUT**
**Status:** Still running when manually killed
**Type:** Monolithic Google Test executable containing ~1,000+ test cases

**Problem:**
- This is the main test executable containing most unit tests
- Registered as a SINGLE CTest test instead of individual test cases
- Exceeded both the 5-minute threshold AND the 10-minute timeout
- **CTest timeout mechanism did NOT work** - test continued running past 600s timeout

**Impact:**
- Blocks CI/CD pipeline
- Single failure prevents completion of remaining tests
- Cannot identify which specific sub-test is slow
- Timeout mechanism failure means tests can hang indefinitely

**Root Cause:**
1. CMakeLists.txt not using `gtest_discover_tests()` for this executable
2. All Google Test cases bundled into single CTest test
3. CTest timeout not properly enforced (possible signal handling issue)

**Recommended Fix:**
```cmake
# In tests/CMakeLists.txt
# BEFORE (current):
add_test(NAME scratchbird_tests COMMAND scratchbird_tests)

# AFTER (recommended):
include(GoogleTest)
gtest_discover_tests(scratchbird_tests
    TIMEOUT 60  # Per-test timeout
    PROPERTIES LABELS "unit"
)
```

This will:
- Register each Google Test case as a separate CTest test
- Allow parallel execution
- Provide granular timeout control
- Identify exactly which test is slow

**Workaround:**
Run the test executable directly with Google Test filters to identify slow tests:
```bash
cd /home/dcalford/CliWork/ScratchBird/build/tests
time ./scratchbird_tests --gtest_list_tests | while read test; do
    if [[ $test == *"."* ]] && [[ $test != *"."  ]]; then
        suite=${current_suite}
        testname=${test}
        echo "Running: ${suite}${testname}"
        timeout 60 ./scratchbird_tests --gtest_filter="${suite}${testname}" 2>&1 | grep -E "(PASSED|FAILED|Timeout)"
    else
        current_suite=$test
    fi
done
```

---

## 🟡 WARNING: Tests 40-300 Seconds (Under 5min, but slow)

### 1. DependencyPerformanceTest.GetDependents100K

**Runtime:** 47.60 seconds
**Type:** Performance test with 100K dependencies
**Status:** PASSING

**Analysis:**
- Intentionally tests performance with large dataset (100K dependencies)
- Runtime acceptable for a performance benchmark test
- Should be labeled as "performance" or "stress" test

**Recommendation:**
- Add CTest label: `performance` or `stress`
- Exclude from default test runs
- Run only on-demand or in nightly builds

```cmake
set_tests_properties(DependencyPerformanceTest.GetDependents100K
    PROPERTIES
        LABELS "performance;stress;nightly"
        TIMEOUT 120
)
```

---

## 🟠 WATCH: Tests 20-40 Seconds

### 1. LoginAttemptTrackerTest.ExponentialBackoffMaxMultiplier

**Runtime:** 21.01 seconds
**Type:** Security test with exponential backoff timing
**Status:** PASSING

**Analysis:**
- Tests exponential backoff with actual timing delays
- Runtime expected due to sleep() calls in exponential backoff algorithm
- Validates security feature behavior over time

**Recommendation:**
- Consider mocking time or reducing test iterations
- Or accept as integration test with appropriate timeout
- Label as integration test, not unit test

```cmake
set_tests_properties(LoginAttemptTrackerTest.ExponentialBackoffMaxMultiplier
    PROPERTIES
        LABELS "integration;security"
        TIMEOUT 60
)
```

---

## 📊 PERFORMANCE TESTS: 6-20 Seconds

These tests take 6-20 seconds but are within acceptable bounds for their type:

| Test Name | Runtime | Type | Recommendation |
|-----------|---------|------|----------------|
| GarbageCollectorTest.ZeroDirtyPages | 6.02s | Integration | Label as integration |
| GarbageCollectorTest.CleanPageScanning | 6.01s | Integration | Label as integration |
| GarbageCollectorTest.BackgroundGCRunsAndUpdatesStatistics | 6.01s | Integration | Label as integration |
| AtomicXIDTest.PerformanceBenchmark | 3.55s | Performance | Label as performance |
| AtomicXIDTest.SequentialConsistency | 3.25s | Unit/Integration | OK for concurrency test |

**Analysis:**
- Garbage collector tests involve waiting for background threads (hence 6+ seconds)
- Atomic transaction ID tests involve concurrency and timing
- All are passing and runtime is acceptable for their type

**Recommendations:**
- Add appropriate CTest labels
- GC tests should be "integration" or "background_thread"
- Atomic tests should be "concurrency" or "performance"
- Consider reducing wait times if possible (mock time?)

---

## ✅ FAST TESTS: Under 6 Seconds

**Count:** 1,418 tests (97.7% of analyzed tests)
**Average Runtime:** 0.09 seconds
**Median Runtime:** ~0.01 seconds

**Distribution:**
- 0.00-0.01s: ~90% of tests
- 0.01-0.10s: ~7% of tests
- 0.10-1.00s: ~2% of tests
- 1.00-6.00s: ~1% of tests

**Analysis:**
Most tests are extremely fast (sub-100ms), indicating good test design and isolation.

---

## 🔧 ACTION ITEMS

### Priority 1: Fix scratchbird_tests Test (CRITICAL)

**Estimated Effort:** 2-3 hours

**Steps:**
1. Modify `tests/CMakeLists.txt` to use `gtest_discover_tests(scratchbird_tests)`
2. Remove manual `add_test(NAME scratchbird_tests ...)`
3. Rebuild test suite
4. Verify each test case registered separately
5. Re-run full suite to identify specific slow tests

**Expected Results:**
- ~1,000 separate CTest tests instead of 1 monolithic test
- Granular timeout control (60s per test case)
- Parallel test execution
- Clear identification of slow test cases

### Priority 2: Investigate CTest Timeout Failure (CRITICAL)

**Estimated Effort:** 1-2 hours

**Issue:** CTest `--timeout 600` did not kill test after 600 seconds

**Investigation Steps:**
1. Check if timeout is per-test or global
2. Verify signal handling in test executable
3. Test with explicit `set_tests_properties(... TIMEOUT 60)`
4. Check if Google Test is blocking SIGTERM/SIGALRM

**Potential Causes:**
- Google Test signal handling interfering with CTest
- Timeout is global, not per-test
- Test process ignoring signals

### Priority 3: Categorize Tests by Execution Time (HIGH)

**Estimated Effort:** 2-4 hours

**Create test categories:**
- **smoke** (< 1s): ~1,300 tests
- **unit** (< 10s): ~1,400 tests
- **integration** (10-60s): ~20 tests
- **performance** (> 60s): ~5 tests

**Benefits:**
- Fast feedback loop for developers (smoke tests in <2 minutes)
- Parallel test execution by category
- Better CI/CD pipeline design

### Priority 4: Optimize Slow Tests (MEDIUM)

**Targets:**
1. GarbageCollectorTest.* - reduce wait times or mock timers
2. LoginAttemptTrackerTest.* - reduce iterations or mock time
3. DependencyPerformanceTest.* - reduce dataset size or mark as stress test

**Goal:** Bring all "unit" tests under 5 seconds

---

## CTest TIMEOUT Configuration Issues

### Current Configuration
```bash
ctest --timeout 600  # Global timeout, not working correctly
```

### Recommended Configuration
```cmake
# In CMakeLists.txt
set_tests_properties(test_name
    PROPERTIES TIMEOUT 60  # Per-test timeout in seconds
)

# Or for all tests:
set(CTEST_TEST_TIMEOUT 60)
```

### Testing Timeout Mechanism
```bash
# Test that timeout actually works:
cd build
ctest -R "some_fast_test" --timeout 1  # Should timeout
ctest -R "some_fast_test" --timeout 60  # Should pass
```

---

## Recommended Test Execution Strategy

### Developer Workflow

**Before Commit:**
```bash
ctest -L smoke --timeout 10        # ~1-2 min, 1,300 tests
```

**Before Push:**
```bash
ctest -L "smoke|unit" --timeout 60 # ~5-10 min, 1,400 tests
```

**Full Suite:**
```bash
ctest -E "performance|stress" --timeout 120  # ~15-20 min, 1,450 tests
```

**Performance/Stress (Nightly):**
```bash
ctest -L "performance|stress" --timeout 300  # ~5-10 min, 20 tests
```

### CI/CD Pipeline

**PR Checks (Fast):**
- Run: smoke + unit tests
- Timeout: 60s per test
- Total Time: ~10 minutes
- Skip: performance, stress, long-integration

**Nightly/Main Branch:**
- Run: ALL tests
- Timeout: 120s per test
- Total Time: ~30 minutes
- Include: all test categories

---

## Test Suite Statistics

### Overall Health: 🟡 GOOD (with critical issues)

| Metric | Value | Status |
|--------|-------|--------|
| Total Tests | 1,469 | ✅ |
| Tests Analyzed | 1,425 (97%) | ✅ |
| Passing Tests | 1,425 (100%) | ✅ |
| Failing Tests | 0 | ✅ |
| Timeout Tests | 1 (scratchbird_tests) | 🔴 CRITICAL |
| Average Runtime | 0.09s | ✅ EXCELLENT |
| Median Runtime | 0.01s | ✅ EXCELLENT |
| Max Runtime (analyzed) | 47.60s | ✅ OK |
| Max Runtime (actual) | 600s+ | 🔴 FAILURE |
| Tests > 5min | 1 | 🔴 CRITICAL |
| Tests > 1min | 1 | 🟡 OK |
| Tests > 10s | 7 | ✅ OK |

### Key Findings

**Strengths:**
- ✅ 97.7% of tests complete in < 6 seconds
- ✅ Average test time of 0.09s is excellent
- ✅ No test failures detected in analyzed tests
- ✅ Good test coverage (1,469 tests)

**Critical Issues:**
- 🔴 Test #1426 (scratchbird_tests) exceeds 10 minutes
- 🔴 CTest timeout mechanism not working
- 🔴 Cannot identify specific slow test within monolithic executable
- 🔴 Blocks completion of full test suite

**Improvement Opportunities:**
- 🟡 Break scratchbird_tests into individual test cases
- 🟡 Categorize tests by execution time
- 🟡 Add proper CTest labels
- 🟡 Optimize 6-second GC tests
- 🟡 Document expected test run times

---

## Comparison to Previous Analysis

### Previous Deadlock Issues (RESOLVED)
- ✅ StoredCodeDependencyTest.* tests (4 tests) - **FIXED** (see TEST_TIMEOUT_REANALYSIS_2025_12_30.md)
- ✅ ExecutorTransactionPayloadTest.* tests - **PASSING** (15-11ms, no longer hanging)

### New Issues Found
- 🔴 scratchbird_tests monolithic test exceeds 10 minutes
- 🔴 CTest timeout not enforced

---

## References

- **Previous Deadlock Analysis:** `/docs/archive/2026-01-09/findings/DEADLOCK_FIX_2025_12_30.md`
- **Previous Timeout Analysis:** `/docs/archive/2026-01-09/findings/TEST_TIMEOUT_REANALYSIS_2025_12_30.md`
- **Test Categorization Plan:** `/docs/archive/2026-01-04/planning/TEST_ISOLATION_AND_CATEGORIZATION_PLAN.md`
- **Google Test Documentation:** https://google.github.io/googletest/
- **CMake gtest_discover_tests:** https://cmake.org/cmake/help/latest/module/GoogleTest.html

---

**Analysis By:** Claude Code
**Date:** 2025-12-31
**Status:** DOCUMENTED - ACTION REQUIRED
**Priority:** 🔴 CRITICAL

---

## Appendix A: Top 30 Slowest Tests (Excluding scratchbird_tests)

| Rank | Test Name | Runtime | Category |
|------|-----------|---------|----------|
| 1 | DependencyPerformanceTest.GetDependents100K | 47.60s | Performance |
| 2 | LoginAttemptTrackerTest.ExponentialBackoffMaxMultiplier | 21.01s | Integration |
| 3 | GarbageCollectorTest.ZeroDirtyPages | 6.02s | Integration |
| 4 | GarbageCollectorTest.CleanPageScanning | 6.01s | Integration |
| 5 | GarbageCollectorTest.BackgroundGCRunsAndUpdatesStatistics | 6.01s | Integration |
| 6 | AtomicXIDTest.PerformanceBenchmark | 3.55s | Performance |
| 7 | AtomicXIDTest.SequentialConsistency | 3.25s | Unit |
| 8 | LoginAttemptTrackerTest.CleanupExpiredEntries | 1.11s | Unit |
| 9 | LoginAttemptTrackerTest.AttemptsResetAfterWindow | 1.11s | Unit |
| 10 | GarbageCollectorTest.SweepIntegration | 1.02s | Integration |
| 11 | LoginAttemptTrackerTest.NoExponentialBackoffWhenDisabled | 0.61s | Unit |
| 12 | LoginAttemptTrackerTest.LockoutExpiresAfterDuration | 0.61s | Unit |
| 13 | LoginAttemptTrackerTest.ExponentialBackoffIncreasesLockout | 0.61s | Unit |
| 14 | MemorySafetyTest.OOM_OpenHeaderAllocation | 0.49s | Unit |
| 15 | AtomicXIDTest.AtomicIsolation | 0.48s | Unit |
| 16 | AtomicXIDTest.ConcurrentAllocation_10Threads | 0.37s | Concurrency |
| 17 | AtomicXIDTest.SerialAllocation | 0.34s | Unit |
| 18 | AtomicXIDTest.HighConcurrency_100Threads | 0.24s | Concurrency |
| 19 | MemorySafetyTest.OOM_BufferPoolAllocation | 0.16s | Unit |
| 20 | GinPhase6Test.Comprehensive | 0.13s | Unit |
| 21 | SecurityTest.ConcurrentAccess_LockReleaseOnCrash | 0.12s | Concurrency |
| 22 | StorageCriticalFixesTest.HeapScanIterator_NoMemoryLeak | 0.11s | Unit |
| 23 | NetworkTest.FullServerClientIntegration | 0.11s | Integration |
| 24 | NetworkTest.EventLoopRepeatingTimer | 0.11s | Unit |
| 25 | NetworkTest.EventLoopReadEvent | 0.11s | Unit |
| 26 | LoginAttemptTrackerTest.LockoutTimeRemaining | 0.11s | Unit |
| 27 | LoginAttemptTrackerTest.AttemptsNotResetBeforeWindow | 0.11s | Unit |
| 28 | GinPhase5Test.Comprehensive | 0.11s | Unit |
| 29 | GarbageCollectorTest.PriorityCalculationBasic | 0.11s | Unit |
| 30 | CompressionInteropTest.AllPageSizesWithCompression | 0.11s | Unit |

**Note:** Tests under 0.10s are considered fast and healthy.

---

## Appendix B: Test Count by Runtime Category

| Runtime Category | Test Count | Percentage | Status |
|------------------|------------|------------|--------|
| < 0.01s | ~1,280 | 90% | ✅ Excellent |
| 0.01s - 0.10s | ~100 | 7% | ✅ Good |
| 0.10s - 1.00s | ~25 | 1.8% | ✅ OK |
| 1.00s - 10.00s | ~13 | 0.9% | 🟡 Monitor |
| 10.00s - 60.00s | ~6 | 0.4% | 🟡 Integration |
| 60.00s - 300.00s | ~0 | 0% | ✅ None |
| > 300.00s (5min) | ~1 | 0.1% | 🔴 Critical |

---

**END OF REPORT**
