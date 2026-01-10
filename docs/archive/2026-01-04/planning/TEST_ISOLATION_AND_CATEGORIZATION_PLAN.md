# Test Isolation and Categorization Plan

**Created:** 2025-12-29
**Status:** 🚧 IN PROGRESS
**Purpose:** Organize tests by execution time and risk to enable fast CI/CD and reliable testing

---

## Problem Statement

Current issues with test suite:
1. **4 tests timeout** after 300 seconds (deadlock - see TEST_TIMEOUT_ANALYSIS_2025_12_29.md)
2. **No categorization by execution time** - all 1,346 tests run together
3. **No isolation** of known-flaky or problematic tests
4. **Long CI/CD feedback loop** - full suite takes minutes to hours
5. **Build artifact issues** can cause false failures (BytecodeOpcodesTest)

**Goal:** Create fast, reliable, categorized test execution strategy

---

## Current Test Organization

### By CTest Labels

Tests are already labeled by feature (51 labels identified):

- **aggregate**, **batch**, **bitpack**, **brin**, **buffer_pool**
- **check**, **client**, **columnstore**, **composite**, **comprehensive**
- **concurrency**, **connection**, **constraints**, **critical**, **dictionary**
- **dml**, **e2e**, **enforcement**, **exception**, **foreign_key**
- **fulltext**, **gc**, **gin**, **gist**, **hnsw**
- **index**, **integration**, **ipc**, **join_ordering**, **load**
- **memory**, **mga**, **mv_rewriter**, **mvcc**, **network**
- **optimizer**, **p3**, **page**, **performance**, **phase1-7**
- **plan01**, **pool**, **predicate**, **protocol**, **range_operators**
- **range_types**, **rebuild**, **referential_integrity**, **rle**, **robustness**
- **rtree**, **safety**, **server**, **shadow**, **simd**
- **simple**, **storage**, **stress**, **task14-16**, **temporal_range_types**
- **thread**, **tsan**, **unit**, **versioning**, **wire**

### By Test Type

- **Unit tests:** Fast, isolated component tests (~2,500+ tests)
- **Integration tests:** Multi-component tests (~350 tests)
- **Stress tests:** Load/concurrency tests
- **Performance tests:** Benchmark tests

---

## Proposed Test Categories

### Category 1: Smoke Tests (< 1 second each)

**Purpose:** Fast sanity check before commit

**Contents:**
- Core functionality tests
- Basic SQL parsing
- Simple index operations
- Memory allocation/deallocation
- Basic data types

**Execution:** ~30 seconds total

**Example Tests:**
- BasicSQLParsingTest.*
- SimpleIndexTest.*
- DataTypeTest.BasicTypes
- MemoryAllocationTest.*

**Usage:**
```bash
ctest -L smoke --output-on-failure
```

### Category 2: Unit Tests (< 5 seconds each)

**Purpose:** Comprehensive component testing

**Contents:**
- All unit tests
- Single-component functionality
- No external dependencies
- Deterministic results

**Execution:** ~2-5 minutes total

**Example Tests:**
- All tests in tests/unit/
- Parser unit tests
- Catalog unit tests
- Index unit tests

**Usage:**
```bash
ctest -L unit --output-on-failure
```

### Category 3: Integration Tests (< 30 seconds each)

**Purpose:** Multi-component interaction testing

**Contents:**
- Multi-component tests
- Transaction tests
- Concurrency tests (non-stress)
- End-to-end workflows

**Execution:** ~10-20 minutes total

**Example Tests:**
- All tests in tests/integration/
- MGA/MVCC tests
- Client-server tests
- Transaction isolation tests
- Multi-component suites matched by `*Integration*`, `*E2E*`, `*EndToEnd*`, plus `QueryCompilerV2Test.*` and `Week3Week4ComprehensiveTest.*`

**Usage:**
```bash
ctest -L integration --output-on-failure
```

**Runtime Gate:** Socket-based integration tests require `SCRATCHBIRD_TEST_NETWORK=1` to run.

### Category 4: Stress Tests (> 30 seconds each)

**Purpose:** Load, concurrency, and robustness testing

**Contents:**
- High-volume tests
- Long-running transactions
- Concurrency stress tests
- Memory pressure tests

**Execution:** ~30-60 minutes total

**Example Tests:**
- Stress tests
- Load tests
- Concurrent access tests
- Any tests with `Stress` in the name (`*Stress*` filter)

**Usage:**
```bash
ctest -L stress --output-on-failure --timeout 600
```

### Category 5: Flaky/Problematic Tests (QUARANTINE)

**Purpose:** Isolate known-problematic tests for investigation

**Contents:**
- Tests that timeout
- Tests with deadlocks
- Tests with race conditions
- Tests with build dependencies

**Current Members:**
- StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction
- StoredCodeDependencyTest.DropProcedureFailsIfCalled
- StoredCodeDependencyTest.ComplexFunctionChain
- StoredCodeDependencyTest.MixedFunctionProcedureDependencies
- BytecodeOpcodesTest.SBLRVersionIsDefined (build artifact issue)

**Execution:** Manual only, not in CI/CD

**Usage:**
```bash
ctest -L quarantine --output-on-failure --timeout 60
```

### Category 6: Performance/Benchmark Tests

**Purpose:** Track performance regressions

**Contents:**
- Benchmark tests
- Performance measurement tests
- Query optimization tests
- Any tests with `Benchmark` in the name (`*Benchmark*` filter)

**Execution:** On-demand or nightly only

**Usage:**
```bash
ctest -L performance --output-on-failure
```

---

## Implementation Plan

### Phase 1: Add CTest Labels (2-3 hours)

**Update CMakeLists.txt to add execution-time labels:**

```cmake
# Smoke tests (< 1s)
set_tests_properties(
    BasicSQLParsingTest.SimpleSelect
    BasicSQLParsingTest.SimpleInsert
    # ... more smoke tests ...
    PROPERTIES LABELS "smoke;unit;critical"
)

# Integration tests (< 30s)
set_tests_properties(
    TransactionIsolationTest.ReadCommitted
    TransactionIsolationTest.RepeatableRead
    # ... more integration tests ...
    PROPERTIES LABELS "integration;mvcc"
)

# Stress tests (> 30s)
set_tests_properties(
    ConcurrencyStressTest.MultipleWriters
    LoadTest.HighVolume
    # ... more stress tests ...
    PROPERTIES LABELS "stress;concurrency"
)

# Quarantine (problematic tests)
set_tests_properties(
    StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction
    StoredCodeDependencyTest.DropProcedureFailsIfCalled
    StoredCodeDependencyTest.ComplexFunctionChain
    StoredCodeDependencyTest.MixedFunctionProcedureDependencies
    BytecodeOpcodesTest.SBLRVersionIsDefined
    PROPERTIES LABELS "quarantine;disabled"
)
```

### Phase 2: Create Test Execution Scripts (1 hour)

**Create:** `tests/run_tests.sh` (builds `scratchbird_test_binaries` before running CTest)

```bash
#!/bin/bash
# ScratchBird Test Execution Script

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Build directory not found: ${BUILD_DIR}" >&2
    exit 1
fi

cmake --build "${BUILD_DIR}" --target scratchbird_test_binaries

cd "${BUILD_DIR}"

case "$1" in
    smoke)
        echo "Running smoke tests..."
        ctest -L smoke --output-on-failure
        ;;
    unit)
        echo "Running unit tests..."
        ctest -L unit --output-on-failure --timeout 10
        ;;
    integration)
        echo "Running integration tests..."
        ctest -L integration --output-on-failure --timeout 60
        ;;
    stress)
        echo "Running stress tests..."
        ctest -L stress --output-on-failure --timeout 600
        ;;
    quarantine)
        echo "Running quarantine tests (known issues)..."
        ctest -L quarantine --output-on-failure --timeout 60 || true
        ;;
    quick)
        echo "Running quick test suite (smoke + unit)..."
        ctest -L "smoke|unit" --output-on-failure --timeout 10
        ;;
    ci)
        echo "Running CI test suite (smoke + unit + integration)..."
        ctest -L "smoke|unit|integration" -E "quarantine" --output-on-failure --timeout 60
        ;;
    all)
        echo "Running ALL tests (excluding quarantine)..."
        ctest -E "quarantine" --output-on-failure --timeout 300
        ;;
    *)
        echo "Usage: $0 {smoke|unit|integration|stress|quarantine|quick|ci|all}"
        echo ""
        echo "  smoke       - Fast sanity tests (< 1s each, ~30s total)"
        echo "  unit        - Unit tests (< 5s each, ~5min total)"
        echo "  integration - Integration tests (< 30s each, ~20min total)"
        echo "  stress      - Stress tests (> 30s each, ~1hr total)"
        echo "  quarantine  - Known-problematic tests (manual investigation)"
        echo "  quick       - smoke + unit (fast feedback, ~5min)"
        echo "  ci          - CI suite: smoke + unit + integration (~25min)"
        echo "  all         - Everything except quarantine (~1.5hrs)"
        exit 1
        ;;
esac
```

**Update:** add a `performance` option and document `SCRATCHBIRD_TEST_NETWORK=1` for socket-based integration tests.

**Make executable:**
```bash
chmod +x tests/run_tests.sh
```

### Phase 3: Update CI/CD Configuration (1 hour)

**Create:** `.github/workflows/tests.yml`

```yaml
name: Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  smoke-tests:
    name: Smoke Tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. && make -j$(nproc)
      - name: Run Smoke Tests
        run: ./tests/run_tests.sh smoke
```

**Update:** workflow now runs smoke, unit, and integration jobs via `tests/run_tests.sh`, with `SCRATCHBIRD_TEST_NETWORK=1` set for integration.

  unit-tests:
    name: Unit Tests
    runs-on: ubuntu-latest
    needs: smoke-tests
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. && make -j$(nproc)
      - name: Run Unit Tests
        run: ./tests/run_tests.sh unit

  integration-tests:
    name: Integration Tests
    runs-on: ubuntu-latest
    needs: unit-tests
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. && make -j$(nproc)
      - name: Run Integration Tests
        run: ./tests/run_tests.sh integration

  stress-tests:
    name: Stress Tests (Nightly)
    runs-on: ubuntu-latest
    # Only run on schedule or manual trigger
    if: github.event_name == 'schedule'
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: |
          mkdir build && cd build
          cmake .. && make -j$(nproc)
      - name: Run Stress Tests
        run: ./tests/run_tests.sh stress
```

### Phase 4: Fix Quarantine Tests (Separate Work)

**Priority 1:** Fix deadlock tests (see TEST_TIMEOUT_ANALYSIS_2025_12_29.md)
- Implement lock ordering fix
- Verify all 4 StoredCodeDependencyTest tests pass
- Remove from quarantine

**Priority 2:** Fix build artifact test
- Add CMake target dependency to ensure rebuild
- Or add version check to skip if SBLR_VERSION changes

**Priority 3:** Move tests back to main suites
- Once fixed, relabel tests appropriately
- Remove from quarantine

---

## Expected Benefits

### Fast Feedback

**Before:**
- Full test run: ~1.5 hours
- No partial runs
- All tests or nothing

**After:**
- Smoke tests: ~30 seconds
- Quick suite: ~5 minutes
- CI suite: ~25 minutes
- Full suite: ~1.5 hours (unchanged)

**Developer Workflow:**
```bash
# Before commit
./tests/run_tests.sh quick        # 5 minutes

# Before push
./tests/run_tests.sh ci           # 25 minutes

# Before release
./tests/run_tests.sh all          # 1.5 hours

# Investigate problems
./tests/run_tests.sh quarantine   # Manual
```

### Improved Reliability

- **Quarantine** isolates known-flaky tests
- **Timeouts** prevent infinite hangs
- **Label-based** execution avoids problematic tests in CI
- **Build dependencies** caught early with smoke tests

### Better Diagnostics

- **Clear categorization** makes failures easier to understand
- **Execution time** expectations set per category
- **Quarantine** clearly marks known issues

---

## Migration Plan

### Week 1: Label Assignment

1. Identify all current tests (Done: 1,346 tests, 51 labels)
2. Add execution-time labels to CMakeLists.txt (IN PROGRESS: smoke/perf/stress/integration/quarantine filters added for gtest_discover + integration labels added for IPC/server/wire/client tests)
3. Create smoke test subset (target: 500-1000 tests) (EXPANDED to ~1000 via `SCRATCHBIRD_SMOKE_TESTS` + gtest filter in `tests/CMakeLists.txt`)

### Week 2: Script Creation

1. Create `tests/run_tests.sh`
2. Test locally with all categories
3. Document usage

### Week 3: CI/CD Integration

1. Create GitHub Actions workflow
2. Test on feature branch
3. Merge to main

### Week 4: Quarantine Fixes

1. Fix StoredCodeDependencyTest deadlock (2-3 hours)
2. Fix BytecodeOpcodesTest build dependency
3. Move tests out of quarantine

---

## Success Criteria

✅ **Smoke tests** run in < 1 minute
✅ **Quick suite** runs in < 10 minutes
✅ **CI suite** runs in < 30 minutes
✅ **Zero timeout failures** in CI (quarantine tests excluded)
✅ **All tests** labeled appropriately
✅ **Quarantine** cleared within 2 weeks

---

## Maintenance

### Adding New Tests

When creating new tests:

1. **Estimate execution time**
2. **Add appropriate labels:**
   ```cmake
   add_test(NAME MyNewTest ...)
   set_tests_properties(MyNewTest
       PROPERTIES LABELS "unit;feature_name;quick")
   ```
3. **If > 5s:** Add to integration, not unit
4. **If > 30s:** Add to stress, not integration
5. **If flaky:** Add to quarantine temporarily, then fix

### Reviewing Quarantine

**Monthly:** Review quarantine tests
- Fix or document known issues
- Remove fixed tests from quarantine
- Update labels as appropriate

---

## References

- **Current Failures:** `/docs/archive/2026-01-09/findings/TEST_SUITE_FAILURES_2025_12_27.md`
- **Timeout Analysis:** `/docs/archive/2026-01-09/findings/TEST_TIMEOUT_ANALYSIS_2025_12_29.md`
- **Test Statistics:** `/docs/PROJECT_STATISTICS.md` (3,023 test cases, 1,346 CTest tests)

---

**Status:** 📋 READY FOR IMPLEMENTATION
**Estimated Effort:** 5-7 hours
**Expected Completion:** 1 week for full implementation
**Priority:** HIGH - Blocks efficient CI/CD
