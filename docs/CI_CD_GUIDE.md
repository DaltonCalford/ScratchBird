# ScratchBird CI/CD and Sanitizer Guide

**Document Version**: 1.0
**Last Updated**: October 17, 2025
**Status**: Production Ready

---

## Table of Contents

1. [Overview](#overview)
2. [CI/CD Pipeline](#cicd-pipeline)
3. [Sanitizers](#sanitizers)
4. [Static Analysis](#static-analysis)
5. [Local Testing](#local-testing)
6. [Interpreting Results](#interpreting-results)
7. [Troubleshooting](#troubleshooting)
8. [Best Practices](#best-practices)

---

## Overview

This document describes the CI/CD infrastructure for ScratchBird, including automated sanitizers, static analysis, and quality gates that run on every commit.

### Goals

1. **Catch bugs early**: Detect memory errors, data races, and undefined behavior before they reach production
2. **Enforce code quality**: Maintain consistent code standards across the codebase
3. **Prevent regressions**: Ensure new code doesn't break existing functionality
4. **Fast feedback**: Provide developers with quick, actionable feedback on their changes

### Components

- **GitHub Actions workflows**: Automated CI/CD running on every push/PR
- **ThreadSanitizer (TSAN)**: Detects data races and threading bugs
- **AddressSanitizer (ASAN)**: Detects memory errors (use-after-free, buffer overflows, leaks)
- **Helgrind**: Valgrind-based race condition detector
- **Valgrind Memcheck**: Memory leak detection
- **Clang-Tidy**: Static analysis for bugs, performance issues, and style
- **Pin/Unpin Balance Checker**: Custom tool to detect resource leaks in buffer pool

---

## CI/CD Pipeline

### GitHub Actions Workflows

All workflows are defined in `.github/workflows/`:

#### 1. Sanitizers Workflow (`.github/workflows/sanitizers.yml`)

**Triggers**:
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop`

**Jobs**:
1. **thread-sanitizer**: Builds with `-fsanitize=thread` and runs TSAN tests
2. **address-sanitizer**: Builds with `-fsanitize=address,undefined,leak` and runs ASAN tests
3. **helgrind**: Runs Valgrind Helgrind on concurrency tests
4. **clang-tidy**: Static analysis with comprehensive checks
5. **resource-leak-detection**: Valgrind memcheck for memory leaks
6. **pin-unpin-balance**: Custom balance checking for buffer pool operations
7. **summary**: Aggregates results and generates summary report

**Parallelization**: All jobs run in parallel except `summary` which waits for all jobs to complete

**Timeout**:
- TSAN/ASAN tests: 5 minutes
- Helgrind/Valgrind: 10 minutes
- Clang-Tidy: 15 minutes

**Artifacts**: All job logs are saved for 30 days

### Workflow Configuration

```yaml
# Example job structure
job-name:
  runs-on: ubuntu-latest
  steps:
    - Checkout code
    - Install dependencies
    - Configure CMake with sanitizer flags
    - Build
    - Run tests
    - Upload artifacts
```

---

## Sanitizers

### 1. ThreadSanitizer (TSAN)

**Purpose**: Detect data races in multi-threaded code

**How it works**:
- Instruments all memory accesses and synchronization operations
- Tracks happens-before relationships between threads
- Reports when two threads access the same memory location without proper synchronization

**Build flags**:
```cmake
-DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitizer=thread"
```

**Tests run**:
- All `tsan_*` dedicated race detection tests
- `StatisticsAccuracy` tests (concurrent statistics updates)
- `PageManagerDestructor` tests (concurrent FSM access)

**Expected output** (passing):
```
==========Running 6 tests from 1 test suite==========
[       OK ] All tests passed
ThreadSanitizer: no issues found
```

**Failed output example**:
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x7b0400001234 by thread T2:
    #0 BufferPool::pinPage() buffer_pool.cpp:175
  Previous write of size 4 at 0x7b0400001234 by thread T1:
    #0 BufferPool::evictPage() buffer_pool.cpp:250
```

**When it fails**:
1. Review the stack traces to identify the conflicting accesses
2. Check if proper locking/atomics are used
3. Verify lock ordering matches documented hierarchy
4. Add suppression only for confirmed false positives

---

### 2. AddressSanitizer (ASAN)

**Purpose**: Detect memory errors and undefined behavior

**Detects**:
- Use-after-free
- Heap/stack/global buffer overflows
- Use-after-scope
- Memory leaks (via LeakSanitizer)
- Integer overflows (via UBSanitizer)
- Null pointer dereferences

**Build flags**:
```cmake
-DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined -fsanitize=leak -fno-omit-frame-pointer -g -O1"
```

**Environment variables**:
```bash
ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:strict_init_order=1"
LSAN_OPTIONS="suppressions=tools/lsan_suppressions.txt"
UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"
```

**Tests run**: All test suites

**Expected output** (passing):
```
==========All tests passed==========
SUMMARY: AddressSanitizer: 0 detected issues
SUMMARY: LeakSanitizer: no leaks found
SUMMARY: UBSanitizer: 0 detected issues
```

**Failed output example**:
```
SUMMARY: AddressSanitizer: heap-use-after-free on address 0x60300000eff0
    #0 0x498bcd in BufferPool::unpinPage() buffer_pool.cpp:200
```

**When it fails**:
1. Check the error type (use-after-free, buffer overflow, leak, etc.)
2. Review the stack trace to find the bug location
3. For leaks: Check if resources are properly freed in destructors/cleanup
4. For buffer overflows: Verify bounds checking before array access

---

### 3. Helgrind

**Purpose**: Valgrind-based race condition detection

**Detects**:
- Data races (same as TSAN but with different algorithm)
- Lock order violations
- Improper use of pthreads API

**Advantages over TSAN**:
- Can detect some races TSAN misses
- Better at finding lock ordering issues
- Works on older GCC versions

**Disadvantages**:
- Slower than TSAN (50-100x slowdown)
- More false positives
- Requires suppression file tuning

**Command**:
```bash
valgrind --tool=helgrind \
    --suppressions=tools/helgrind_suppressions.txt \
    --log-file=helgrind.log \
    ./tests/helgrind_races
```

**Expected output** (passing):
```
ERROR SUMMARY: 0 errors from 0 contexts
```

**Failed output example**:
```
Possible data race during write at 0x4a2b6c0 by thread #2
   at 0x4012F3: BufferPool::clockSweep() (buffer_pool.cpp:350)
 This conflicts with a previous write at 0x4a2b6c0 by thread #1
   at 0x401234: BufferPool::evictPage() (buffer_pool.cpp:250)
```

---

### 4. Valgrind Memcheck (Leak Detection)

**Purpose**: Comprehensive memory leak detection

**Detects**:
- Definitely lost: Memory no longer accessible (true leaks)
- Indirectly lost: Memory pointed to by leaked memory
- Possibly lost: Memory with interior pointers only
- Still reachable: Memory accessible at exit (often false positives)

**Command**:
```bash
valgrind --tool=memcheck \
    --leak-check=full \
    --show-leak-kinds=definite,possible \
    --track-origins=yes \
    --suppressions=tools/valgrind_suppressions.txt \
    ./tests/scratchbird_tests
```

**Expected output** (passing):
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
   possibly lost: 0 bytes in 0 blocks
   still reachable: 1,024 bytes in 5 blocks (static globals)
```

**Failed output example**:
```
LEAK SUMMARY:
   definitely lost: 4,096 bytes in 1 blocks
   at BufferPool::allocateFrame() (buffer_pool.cpp:150)
```

**When it fails**:
1. Focus on "definitely lost" - these are real leaks
2. "Possibly lost" may be false positives (e.g., interior pointers)
3. "Still reachable" at exit is usually OK (static/global data)
4. Check destructor chains for missing `delete`/`free` calls

---

## Static Analysis

### Clang-Tidy

**Purpose**: Static code analysis for bugs, performance, and style

**Checks enabled**:
- `bugprone-*`: Bug-prone code patterns
- `clang-analyzer-*`: Deep static analysis
- `cppcoreguidelines-*`: C++ Core Guidelines violations
- `performance-*`: Performance anti-patterns
- `readability-*`: Readability and maintainability
- `modernize-*`: Modern C++ idioms
- `concurrency-*`: Concurrency issues

**Critical checks (warnings-as-errors)**:
- `bugprone-use-after-move`
- `bugprone-dangling-handle`
- `bugprone-undefined-memory-manipulation`
- `clang-analyzer-core.NullDereference`
- `clang-analyzer-core.DivideZero`
- `concurrency-mt-unsafe`

**Configuration**: `.clang-tidy` in project root

**Command**:
```bash
clang-tidy src/core/buffer_pool.cpp -p=build \
    --checks='bugprone-*,clang-analyzer-*,concurrency-*'
```

**Expected output** (passing):
```
[No warnings or errors]
```

**Failed output example**:
```
buffer_pool.cpp:175:5: warning: use of a moved-from object [bugprone-use-after-move]
    frame->data = std::move(buffer);
    ^
```

**When it fails**:
1. Read the warning message and suggested fix
2. Most warnings indicate real bugs or bad patterns
3. Only suppress with `// NOLINT(check-name)` if absolutely necessary
4. Document why suppression is needed with a comment

---

## Local Testing

### Quick Start

Run all sanitizers before committing:
```bash
./tools/run_sanitizers.sh --all
```

### Selective Testing

Run specific sanitizers:
```bash
./tools/run_sanitizers.sh --tsan        # ThreadSanitizer only
./tools/run_sanitizers.sh --asan        # AddressSanitizer only
./tools/run_sanitizers.sh --helgrind    # Helgrind only
./tools/run_sanitizers.sh --valgrind    # Leak detection only
./tools/run_sanitizers.sh --clang-tidy  # Static analysis only
```

### Prerequisites

Install required tools:
```bash
# Ubuntu/Debian
sudo apt-get install \
    cmake g++ clang clang-tidy \
    valgrind libgtest-dev

# Fedora/RHEL
sudo dnf install \
    cmake gcc-c++ clang clang-tools-extra \
    valgrind gtest-devel
```

### Build Directories

The script creates separate build directories for each sanitizer:
- `build_tsan/` - ThreadSanitizer build
- `build_asan/` - AddressSanitizer build
- `build_tidy/` - Clang-Tidy build (with compile_commands.json)
- `build/` - Regular debug build (for Helgrind/Valgrind)

### Interpreting Script Output

**Success**:
```
========================================
Summary
========================================

✓ All sanitizer checks passed!
Code is ready for commit.
```

**Failure**:
```
========================================
Summary
========================================

✗ 2 check(s) failed
Please fix the issues before committing.
```

---

## Interpreting Results

### Pin/Unpin Balance Checker

**Purpose**: Detect buffer pool resource leaks

**How it works**:
- Parses test output logs for `pinPage` and `unpinPage` calls
- Tracks balance per page ID
- Reports imbalances at test end

**Output example** (passing):
```
=== Analyzing test-output.log ===
Total pins: 1,000
Total unpins: 1,000
Balance: 0
✓ Pin/unpin balance is perfect
```

**Output example** (failing):
```
=== Analyzing test-output.log ===
Total pins: 1,000
Total unpins: 998
Balance: +2

WARNING: Pin/unpin imbalance detected: +2

Imbalanced pages: 2
  Page 42: +1
  Page 57: +1
```

**When it fails**:
1. Positive balance: More pins than unpins (leak)
2. Negative balance: More unpins than pins (double-free/corruption)
3. Check error paths - are pages unpinned on failure?
4. Check exception safety - are pages unpinned in destructors?

---

## Troubleshooting

### TSAN False Positives

**Symptom**: TSAN reports a race on known-safe atomic operations

**Solution**: Add suppression to `tools/tsan_suppressions.txt` (if file exists) or use TSAN_OPTIONS:
```bash
export TSAN_OPTIONS="suppressions=tools/tsan_suppressions.txt"
```

**Example suppression**:
```
race:std::atomic::load
race:BufferPool::getStats  # benign race on statistics
```

### ASAN Performance Issues

**Symptom**: ASAN tests time out or run very slowly

**Solution**:
1. Reduce test iteration counts for ASAN builds
2. Use `ASAN_OPTIONS="quarantine_size_mb=64"` to limit memory usage
3. Run fewer tests concurrently

### Helgrind False Positives

**Symptom**: Helgrind reports races in standard library or GoogleTest

**Solution**: Add suppression to `tools/helgrind_suppressions.txt`:
```
{
   gtest_false_positive
   Helgrind:Race
   ...
   fun:*testing*
}
```

### Valgrind "Still Reachable" Leaks

**Symptom**: Valgrind reports memory "still reachable" at exit

**Explanation**: Not a real leak - this is memory from static/global objects

**Action**: Suppress with `--show-leak-kinds=definite` to hide "still reachable"

---

## Best Practices

### For Developers

1. **Run sanitizers before every commit**:
   ```bash
   ./tools/run_sanitizers.sh --tsan --asan
   ```

2. **Fix bugs, don't add suppressions**:
   - Suppressions should be rare (false positives only)
   - Every suppression must be documented with a comment

3. **Test concurrency code thoroughly**:
   - Write dedicated TSAN tests for lock-free algorithms
   - Test with high thread counts (50+) to expose races

4. **Use RAII for resource management**:
   - Automatic cleanup prevents leaks
   - Exception-safe by design

5. **Enable compiler warnings**:
   ```cmake
   -Wall -Wextra -Werror -Wpedantic
   ```

### For Code Reviewers

1. **Check sanitizer results in CI**:
   - All sanitizer jobs must pass before merging
   - Review logs for any warnings (even if tests pass)

2. **Verify test coverage**:
   - New code should have TSAN tests if multi-threaded
   - New code should have ASAN tests for memory safety

3. **Look for anti-patterns**:
   - Manual memory management (`new`/`delete`)
   - Naked pointers instead of `unique_ptr`/`shared_ptr`
   - Missing mutex protection in concurrent code

### For Release Managers

1. **All sanitizers must pass** before release
2. **No suppressions** without documented justification
3. **Leak detection** with zero "definitely lost" bytes
4. **Clang-Tidy** with zero critical warnings

---

## Suppression Files

### Location
- `tools/lsan_suppressions.txt` - LeakSanitizer suppressions
- `tools/helgrind_suppressions.txt` - Helgrind suppressions
- `tools/valgrind_suppressions.txt` - Valgrind Memcheck suppressions

### Adding Suppressions

**Only add suppressions for**:
1. Third-party library issues (GoogleTest, spdlog, system libraries)
2. Confirmed false positives in sanitizers
3. Known issues with workarounds in place

**Never suppress**:
- Real bugs in ScratchBird code
- Undiagnosed issues
- Issues you don't understand

**Documentation required**:
```
# Suppression for GoogleTest static singleton (false positive)
{
   gtest_singleton_leak
   Memcheck:Leak
   match-leak-kinds: reachable
   ...
   fun:*testing*UnitTest*
}
```

---

## CI/CD Metrics

### Performance Targets
- TSAN tests: < 2 minutes
- ASAN tests: < 3 minutes
- Helgrind: < 5 minutes
- Clang-Tidy: < 10 minutes
- Total pipeline: < 15 minutes

### Quality Gates
- Zero TSAN data races
- Zero ASAN errors
- Zero "definitely lost" bytes in Valgrind
- Zero Clang-Tidy critical warnings
- All tests passing

### Monitoring
- Track sanitizer job success rate (target: >95%)
- Track average pipeline duration (target: <15 min)
- Alert on increasing suppression file size

---

## References

- [ThreadSanitizer Documentation](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual)
- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [Valgrind User Manual](https://valgrind.org/docs/manual/manual.html)
- [Clang-Tidy Checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)

---

## Changelog

**v1.0 (2025-10-17)**:
- Initial CI/CD infrastructure
- Added all 6 sanitizer jobs
- Created comprehensive suppression files
- Added local testing script
- Implemented pin/unpin balance checker

---

**Document Status**: ✅ Production Ready
**Maintainer**: ScratchBird Development Team
**Last Review**: October 17, 2025
