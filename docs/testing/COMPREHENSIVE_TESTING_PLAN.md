# Comprehensive Testing Plan for Alpha Issues

**Date**: October 17, 2025
**Purpose**: Validate all 21 resolved issues from ALPHA_ISSUES_TRACKER.md
**Status**: Planning Complete, Implementation Starting

---

## Executive Summary

**Goal**: Create comprehensive test suite to validate all fixes for:
- 5 CRITICAL issues (race conditions, deadlocks, exception safety)
- 8 HIGH priority issues (resource leaks, lock ordering, memory safety)
- 7 MEDIUM priority issues (performance, overflow checks, error handling)
- 1 LOW priority issue (code modernization)

**Approach**: Multi-layered testing strategy:
1. **ThreadSanitizer (TSAN)** - Detect data races at runtime
2. **Helgrind** - Valgrind-based race condition detection
3. **Stress Tests** - 100+ thread concurrent operations
4. **Functional Tests** - Validate specific fix behaviors
5. **Performance Tests** - Ensure no performance regressions

---

## Test Categories

### CRITICAL Priority Tests (Must Pass for Beta)

#### 1. ThreadSanitizer (TSAN) Tests
**Purpose**: Detect data races in resolved CRITICAL issues

**Target Issues**:
- CRITICAL-1: BufferPool Frame Metadata Race (atomic operations)
- CRITICAL-2: TransactionManager Cache Corruption (const correctness)
- CRITICAL-3: Lock Ordering Inconsistency (deadlock prevention)
- ERROR-CRITICAL-2: Exception Handling (resource cleanup)

**Test Files to Create**:
- `tests/tsan/test_buffer_pool_race.cpp` - Frame metadata access
- `tests/tsan/test_transaction_cache_race.cpp` - Cache modification
- `tests/tsan/test_lock_ordering.cpp` - Lock acquisition order
- `tests/tsan/test_exception_safety.cpp` - Exception cleanup paths

**Compilation**:
```bash
g++ -fsanitize=thread -g -O1 test_buffer_pool_race.cpp -o test_buffer_pool_race
```

**Expected Behavior**: No data race warnings from TSAN

---

#### 2. Helgrind Race Condition Tests
**Purpose**: Valgrind-based race detection with detailed diagnostics

**Target Issues**:
- HIGH-1: BufferPool Page Table Race
- HIGH-2: Lock Manager Multimap Race
- HIGH-4: Snapshot Pin Management Race
- MEDIUM-1: Non-Atomic BufferPool Stats

**Test Files to Create**:
- `tests/helgrind/test_page_table_race.cpp` - Page table insertion order
- `tests/helgrind/test_lock_table_race.cpp` - Lock table concurrent access
- `tests/helgrind/test_snapshot_pins_race.cpp` - Snapshot vector access
- `tests/helgrind/test_stats_accuracy.cpp` - Statistics counter races

**Execution**:
```bash
valgrind --tool=helgrind --log-file=helgrind.log ./test_page_table_race
```

**Expected Behavior**: No race condition warnings

---

#### 3. Multi-Threaded Stress Tests (100+ threads)
**Purpose**: Validate fixes under extreme concurrency

**Test Scenarios**:

**Test 1: Buffer Pool Thrashing** (`test_buffer_pool_stress.cpp`)
- 100 threads, each pinning/unpinning 1000 pages
- Validates: CRITICAL-1 (atomic pin_count/usage_count)
- Expected: No crashes, no double eviction, no pin count corruption

**Test 2: Transaction Storm** (`test_transaction_stress.cpp`)
- 200 threads, each running 100 transactions
- Validates: CRITICAL-2 (cache access), HIGH-5 (atomic XID)
- Expected: No XID collisions, no cache corruption

**Test 3: Lock Contention Torture** (`test_lock_contention.cpp`)
- 150 threads, each acquiring locks in different patterns
- Validates: CRITICAL-3 (lock ordering), HIGH-2 (lock table race)
- Expected: No deadlocks, no lock table corruption

**Test 4: MVCC Snapshot Chaos** (`test_snapshot_stress.cpp`)
- 100 threads creating snapshots and traversing version chains
- Validates: HIGH-4 (snapshot pins), MEDIUM-7 (TIP cache race)
- Expected: No pin leaks, no snapshot corruption

**Test 5: Exception Injection** (`test_exception_injection.cpp`)
- Inject std::bad_alloc at critical points, verify cleanup
- Validates: ERROR-CRITICAL-2 (exception handling)
- Expected: No resource leaks, correct error propagation

---

### HIGH Priority Tests

#### 4. Concurrent Page Access Tests
**Purpose**: Validate page table race fix (HIGH-1)

**Test Scenarios**:
- 50 threads simultaneously pinning same page (cache hit path)
- 50 threads pinning different pages (cache miss path)
- 100 threads mixed pin/unpin operations

**Validation**:
- Page table integrity maintained
- No orphaned frames
- Correct eviction behavior

**File**: `tests/unit/test_concurrent_page_access.cpp`

---

#### 5. Cross-Page Update Tests
**Purpose**: Validate unpinning asymmetry fix (HIGH-6)

**Test Scenarios**:
- Update tuple causing cross-page version creation
- Lock acquisition failure after tuple insertion
- Verify dirty flag set correctly

**Validation**:
- Tuple persisted despite lock failure
- No data loss
- Correct WAL/durability semantics

**File**: `tests/unit/test_cross_page_update.cpp`

---

#### 6. Lock Contention Tests
**Purpose**: Validate lock manager fixes (HIGH-2, CRITICAL-3)

**Test Scenarios**:
- Deadlock detection with cycle in wait graph
- Multiple threads acquiring locks in consistent order
- Lock timeout handling

**Validation**:
- No deadlocks with correct lock ordering
- Deadlock detection works when locks acquired incorrectly
- No lock table corruption

**File**: `tests/unit/test_lock_contention.cpp`

---

#### 7. Snapshot Concurrency Tests
**Purpose**: Validate snapshot pin management fix (HIGH-4)

**Test Scenarios**:
- 100 snapshots traversing version chains concurrently
- Version chains spanning multiple pages
- Snapshot cleanup with outstanding pins

**Validation**:
- No vector corruption
- No pin leaks
- Correct visibility semantics

**File**: `tests/unit/test_snapshot_concurrency.cpp`

---

### MEDIUM Priority Tests

#### 8. Buffer Pool Exhaustion Tests
**Purpose**: Validate eviction behavior and resource limits

**Test Scenarios**:
- Fill buffer pool to capacity
- Continue pinning pages beyond capacity
- Verify clock sweep eviction works correctly

**Validation**:
- Clean pages evicted before dirty pages
- LRU approximation (clock sweep) works
- No buffer pool corruption

**File**: `tests/unit/test_buffer_pool_exhaustion.cpp`

---

#### 9. Statistics Accuracy Tests
**Purpose**: Validate atomic stats fix (MEDIUM-1)

**Test Scenarios**:
- 100 threads incrementing stats concurrently
- Verify final counts match expected values
- Check for lost updates

**Validation**:
- All increments accounted for
- No race conditions in stats
- Relaxed memory ordering sufficient

**File**: `tests/unit/test_stats_accuracy.cpp`

---

#### 10. TOAST Overflow Tests
**Purpose**: Validate TOAST integer overflow fixes (MEDIUM-2, MEDIUM-3)

**Test Scenarios**:
- Insert tuple with size near UINT32_MAX
- Verify overflow check triggers
- Test offset validation

**Validation**:
- Overflow detected before calculation
- Status::OUT_OF_RANGE returned
- No integer underflow

**File**: `tests/unit/test_toast_overflow.cpp`

---

#### 11. Page Manager Destructor Tests
**Purpose**: Validate flush error handling fix (MEDIUM-5)

**Test Scenarios**:
- Force flush failure in destructor
- Verify error logged
- Check emergency sync attempted

**Validation**:
- Error logged with context
- Emergency sync called
- No silent failures

**File**: `tests/unit/test_page_manager_destructor.cpp`

---

## Implementation Priority

### Phase 1: Critical Race Condition Tests (Week 1)
1. ✅ Create test directory structure
2. 🔄 ThreadSanitizer tests for CRITICAL issues
3. ⏳ Helgrind tests for HIGH issues
4. ⏳ Multi-threaded stress tests (100+ threads)

### Phase 2: High Priority Functional Tests (Week 2)
5. ⏳ Concurrent page access tests
6. ⏳ Cross-page update tests
7. ⏳ Lock contention tests
8. ⏳ Snapshot concurrency tests

### Phase 3: Medium Priority & Edge Cases (Week 3)
9. ⏳ Buffer pool exhaustion tests
10. ⏳ Statistics accuracy tests
11. ⏳ TOAST overflow tests
12. ⏳ Page manager destructor tests

---

## Test Infrastructure Requirements

### CMakeLists.txt Updates
```cmake
# Add TSAN tests
add_executable(tsan_buffer_pool_race tests/tsan/test_buffer_pool_race.cpp)
target_compile_options(tsan_buffer_pool_race PRIVATE -fsanitize=thread -g -O1)
target_link_libraries(tsan_buffer_pool_race scratchbird_core -fsanitize=thread)

# Add Helgrind tests
add_test(NAME helgrind_page_table_race
         COMMAND valgrind --tool=helgrind --error-exitcode=1
                 $<TARGET_FILE:scratchbird_tests> --gtest_filter=HelgrindTests.PageTableRace)

# Add stress tests
add_executable(stress_buffer_pool tests/stress/test_buffer_pool_stress.cpp)
target_link_libraries(stress_buffer_pool scratchbird_core pthread)
```

### Directory Structure
```
tests/
├── tsan/              # ThreadSanitizer tests
│   ├── test_buffer_pool_race.cpp
│   ├── test_transaction_cache_race.cpp
│   ├── test_lock_ordering.cpp
│   └── test_exception_safety.cpp
├── helgrind/          # Helgrind tests
│   ├── test_page_table_race.cpp
│   ├── test_lock_table_race.cpp
│   ├── test_snapshot_pins_race.cpp
│   └── test_stats_accuracy.cpp
├── stress/            # Multi-threaded stress tests
│   ├── test_buffer_pool_stress.cpp
│   ├── test_transaction_stress.cpp
│   ├── test_lock_contention.cpp
│   ├── test_snapshot_stress.cpp
│   └── test_exception_injection.cpp
└── unit/              # Existing unit tests + new functional tests
    ├── test_concurrent_page_access.cpp
    ├── test_cross_page_update.cpp
    ├── test_lock_contention.cpp
    ├── test_snapshot_concurrency.cpp
    ├── test_buffer_pool_exhaustion.cpp
    ├── test_stats_accuracy.cpp
    ├── test_toast_overflow.cpp
    └── test_page_manager_destructor.cpp
```

---

## Success Criteria

### Must Pass for Beta Release
- ✅ All TSAN tests pass with no warnings
- ✅ All Helgrind tests pass with no warnings
- ✅ All stress tests complete without crashes
- ✅ 100% of functional tests pass
- ✅ No resource leaks detected by valgrind

### Performance Requirements
- ✅ Stress tests complete in reasonable time (<5 minutes each)
- ✅ No performance regression vs. pre-fix baseline
- ✅ Buffer pool hit rate remains >90% under stress

### Documentation Requirements
- ✅ Test results documented in test report
- ✅ Any failures analyzed and root cause documented
- ✅ Remaining issues tracked in ALPHA_ISSUES_TRACKER.md

---

## Execution Plan

### Week 1: TSAN & Helgrind (Oct 17-24)
- Day 1-2: Create TSAN tests for CRITICAL issues
- Day 3-4: Create Helgrind tests for HIGH issues
- Day 5-7: Run tests, fix any regressions, document results

### Week 2: Stress Tests (Oct 24-31)
- Day 1-2: Create 100+ thread stress tests
- Day 3-4: Create concurrent access functional tests
- Day 5-7: Run tests, analyze performance, document results

### Week 3: Functional & Edge Cases (Oct 31 - Nov 7)
- Day 1-2: Create MEDIUM priority tests
- Day 3-4: Create edge case tests
- Day 5-7: Full test suite execution, final report

---

## Expected Issues

### Known Challenges
1. **ThreadSanitizer False Positives**: May report benign races in std::atomic
   - Mitigation: Annotate atomics with TSAN suppressions

2. **Helgrind Slowdown**: Tests may run 20-50x slower
   - Mitigation: Use smaller datasets for Helgrind tests

3. **100+ Thread Overhead**: OS may not schedule all threads efficiently
   - Mitigation: Use thread pools, stagger thread creation

4. **Exception Injection Complexity**: Hard to inject at specific points
   - Mitigation: Use LD_PRELOAD to override malloc, or mock allocators

### Contingency Plans
- If TSAN/Helgrind unavailable: Use alternative race detectors (e.g., Intel Inspector)
- If stress tests timeout: Reduce thread count to 50, increase timeout
- If tests flaky: Add retry logic, increase timeouts, reduce concurrency

---

## Test Results Template

```markdown
# Test Results - [Test Category]

**Date**: [Execution Date]
**Build**: [Git Commit]
**Environment**: [CPU, RAM, OS]

## Summary
- Total Tests: [X]
- Passed: [Y]
- Failed: [Z]
- Skipped: [W]

## Details

### TSAN Tests
- test_buffer_pool_race: [PASS/FAIL] - [Duration] - [Race warnings: 0]
- test_transaction_cache_race: [PASS/FAIL] - [Duration] - [Race warnings: 0]
...

### Helgrind Tests
- test_page_table_race: [PASS/FAIL] - [Duration] - [Race warnings: 0]
...

### Stress Tests
- test_buffer_pool_stress: [PASS/FAIL] - [Duration] - [Threads: 100]
...

## Issues Found
[List any regressions or newly discovered issues]

## Performance Metrics
- Buffer pool hit rate: [X]%
- Average transaction time: [Y] μs
- Lock contention: [Z] waits per 1000 ops

## Conclusion
[Overall assessment and recommendations]
```

---

## References

- ALPHA_ISSUES_TRACKER.md - Issue definitions and fixes
- COMPREHENSIVE_AUDIT_REPORT.md - Original issue discovery
- ThreadSanitizer Manual: https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual
- Helgrind Manual: https://valgrind.org/docs/manual/hg-manual.html

---

**Document Status**: Planning Complete, Implementation Starting
**Next Action**: Create tests/tsan/ directory and first TSAN test
**Owner**: Claude (AI Assistant)
**Review Date**: October 24, 2025
