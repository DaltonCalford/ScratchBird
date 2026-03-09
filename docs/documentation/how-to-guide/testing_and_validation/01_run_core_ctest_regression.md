# Testing and Validation

[Testing README](../README.md)

## Synopsis

Run and validate ScratchBird test suites.

## Test Categories

| Suite | Purpose | Command |
|-------|---------|---------|
| Unit | Component tests | `ctest -R unit` |
| Integration | End-to-end | `ctest -R integration` |
| Parser | SQL parsing | `ctest -R parser` |
| Storage | MGA tests | `ctest -R storage` |
| Security | Auth/RLS | `ctest -R security` |
| Performance | Benchmarks | `ctest -R perf` |

## Running Tests

### Full Test Suite

```bash
# Build with tests
cmake -S . -B build -DENABLE_TESTING=ON
cmake --build build

# Run all tests
ctest --test-dir build --output-on-failure

# Parallel execution
ctest --test-dir build -j$(nproc)

# Verbose output
ctest --test-dir build -V
```

### Specific Test Categories

```bash
# Unit tests only
ctest --test-dir build -R unit

# Parser tests
ctest --test-dir build -R parser_v3

# Security tests
ctest --test-dir build -R security

# Storage/MGA tests
ctest --test-dir build -R storage

# Skip slow tests
ctest --test-dir build -E slow
```

## Regression Testing

### Foreign Engine Tests

```bash
# PostgreSQL regression
./test-scratchbird.sh --engine=postgresql --suite=regression

# MySQL regression
./test-scratchbird.sh --engine=mysql --suite=regression

# Firebird regression
./test-scratchbird.sh --engine=firebird --suite=regression

# All engines
./test-all.sh
```

### Native SB Tests

```bash
# Core functionality
ctest --test-dir build -R scratchbird_core

# Transaction tests
ctest --test-dir build -R transaction

# MGA tests
ctest --test-dir build -R mga

# Index tests
ctest --test-dir build -R index
```

## Test Validation

### Checking Results

```bash
# Generate report
ctest --test-dir build --output-junit test-results.xml

# Coverage report
cmake --build build --target coverage

# View coverage
firefox build/coverage/index.html
```

### Test Failure Analysis

```bash
# Run specific failing test with details
ctest --test-dir build -R test_name -V

# GDB debug
gdb ./build/tests/test_name
run
bt  # backtrace on crash
```

## Beta Gate Tests

Required tests for Beta release:

```bash
# Gate 1: Wire Protocol
./test-scratchbird.sh --gate=wire_protocol

# Gate 2: Transaction Semantics
./test-scratchbird.sh --gate=transactions

# Gate 3: Security Enforcement
./test-scratchbird.sh --gate=security

# Gate 4: SQL Correctness
./test-scratchbird.sh --gate=sql_correctness

# Gate 5: NoSQL/Modal
./test-scratchbird.sh --gate=modal

# Gate 6: Cluster
./test-scratchbird.sh --gate=cluster

# All gates
./test-scratchbird.sh --all-gates
```

## Continuous Integration

### CI Test Matrix

| Configuration | Test Suite |
|---------------|------------|
| Debug | Full |
| Release | Full |
| Linux GCC | Full |
| Linux Clang | Full |
| macOS | Full |
| Windows (WSL) | Smoke |
| ARM64 | Full |

### Smoke Tests

Quick validation (2 minutes):

```bash
ctest --test-dir build -R smoke
```

### Extended Tests

Full validation (30+ minutes):

```bash
ctest --test-dir build -E "perf|stress"
```

### Stress Tests

Load testing (hours):

```bash
ctest --test-dir build -R stress -V
```

## Test Environment

### Setup Test Database

```bash
# Create test database
sb_isql -c "CREATE DATABASE testdb;"

# Run tests against it
export SBTEST_DB=testdb
ctest --test-dir build
```

### Isolated Testing

```bash
# Use Docker for isolation
docker run -v $(pwd):/src scratchbird/test-env \
    bash -c "cd /src && ctest --test-dir build"
```

## Debugging Test Failures

### Enable Debug Logging

```bash
# Set debug level
export SB_LOG_LEVEL=debug
ctest --test-dir build -R failing_test -V
```

### Core Dumps

```bash
# Enable core dumps
ulimit -c unlimited

# Run test
ctest --test-dir build -R crashing_test

# Analyze core
gdb ./build/tests/test_name core
```

## Test Data

### Generate Test Data

```bash
# Generate test fixtures
./scripts/generate_test_data.sh --rows=1000000

# Verify data
./scripts/verify_test_data.sh
```

## Reporting

### Submit Test Results

```bash
# Format for CI
ctest --test-dir build --output-junit results.xml

# Upload
curl -X POST https://ci.scratchbird.io/upload \
    -F "file=@results.xml"
```

## See Also

- [Prepare Audit Evidence Bundle](06_prepare_audit_evidence_bundle.md)
- [Test Flake Investigation](../troubleshooting/07_test_flake_investigation.md)
