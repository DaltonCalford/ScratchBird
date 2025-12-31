# ScratchBird Native Test Suite

Comprehensive SQL-based test suite for ScratchBird's native features, including functional tests, memory leak detection (Valgrind), and performance benchmarks.

## Overview

**Total Tests:** 8 SQL test scripts (expandable)
**Categories:** Basic, Advanced, MGA/MVCC, Security, Performance
**Test Infrastructure:** SQL scripts + expected outputs + automated test runners

## Test Categories

### 1. Basic Tests (`tests/basic/`)

Fundamental database operations:
- **001_datatypes.sql** - All data types (INTEGER, VARCHAR, DATE, TIME, TIMESTAMP, UUID, BLOB, etc.)
- **002_ddl.sql** - DDL operations (CREATE, ALTER, DROP for tables, indexes, constraints)
- **003_dml.sql** - DML operations (INSERT, UPDATE, DELETE, SELECT with various clauses)

### 2. Advanced Tests (`tests/advanced/`)

Complex features:
- **001_indexes.sql** - All index types (B-Tree, GiST, GIN, BRIN, Hash, partial, expression, covering)
- **002_domains.sql** - Custom domains, constraints, composite types, arrays, ranges

### 3. MGA/MVCC Tests (`tests/mga/`)

Transaction and concurrency control:
- **001_transactions.sql** - Transactions (COMMIT, ROLLBACK, SAVEPOINT, RETAIN, isolation levels)
- **002_mvcc_visibility.sql** - Multi-version concurrency control, tuple visibility, snapshot isolation

### 4. Security Tests (`tests/security/`)

Authentication and authorization:
- **001_authentication.sql** - User management, roles, permissions, row-level security

### 5. Performance Tests (`tests/performance/`)

Benchmarking scripts (executed via `run_performance_tests.sh`)

## Directory Structure

```
tests/compatibility/scratchbird/
├── tests/                      # SQL test scripts
│   ├── basic/                  # Basic functionality tests
│   │   ├── 001_datatypes.sql
│   │   ├── 002_ddl.sql
│   │   └── 003_dml.sql
│   ├── advanced/               # Advanced feature tests
│   │   ├── 001_indexes.sql
│   │   └── 002_domains.sql
│   ├── mga/                    # MGA/MVCC tests
│   │   ├── 001_transactions.sql
│   │   └── 002_mvcc_visibility.sql
│   ├── security/               # Security tests
│   │   └── 001_authentication.sql
│   └── performance/            # Performance tests
│
├── expected/                   # Expected output files
│   ├── basic/
│   ├── advanced/
│   ├── mga/
│   └── security/
│
├── scripts/                    # Test execution scripts
│   ├── run_tests.sh            # Main test runner
│   ├── run_valgrind_tests.sh   # Memory leak detection
│   └── run_performance_tests.sh # Performance benchmarks
│
├── results/                    # Test execution results
│   ├── valgrind/               # Valgrind output logs
│   └── performance/            # Benchmark reports
│
├── config/                     # Configuration files
│   └── valgrind.supp           # Valgrind suppressions
│
└── README.md                   # This file
```

## Quick Start

### Prerequisites

1. **Build ScratchBird:**
   ```bash
   cd build
   cmake -DCMAKE_BUILD_TYPE=Debug ..
   make
   ```

2. **Install Valgrind (for memory tests):**
   ```bash
   sudo apt-get install valgrind
   ```

### Running Tests

#### 1. Run All Functional Tests

```bash
cd tests/compatibility/scratchbird
./scripts/run_tests.sh
```

**Output:**
```
========================================
ScratchBird Native Test Suite
========================================

Category: basic
==========================================
  basic/001_datatypes                                       [PASS]
  basic/002_ddl                                             [PASS]
  basic/003_dml                                             [PASS]

Category: advanced
==========================================
  advanced/001_indexes                                      [PASS]
  advanced/002_domains                                      [PASS]

========================================
Test Summary
========================================
Total tests run:    8
Passed:             8
Failed:             0
Skipped:            0

✓ All tests passed!
```

#### 2. Run Specific Category

```bash
./scripts/run_tests.sh --category basic
```

#### 3. Run Single Test

```bash
./scripts/run_tests.sh --test basic/001_datatypes
```

#### 4. Update Expected Outputs

After making changes, regenerate expected outputs:

```bash
./scripts/run_tests.sh --update-expected
```

#### 5. Verbose Mode

```bash
./scripts/run_tests.sh --verbose
```

### Memory Leak Detection (Valgrind)

Run tests under Valgrind to detect memory leaks:

```bash
./scripts/run_valgrind_tests.sh
```

**Output:**
```
========================================
ScratchBird Valgrind Memory Tests
========================================

Category: basic
==========================================
  basic/001_datatypes                                       [PASS]
  basic/002_ddl                                             [PASS]
  basic/003_dml                                             [PASS]

Stress Test (1000 rows)                                     [PASS]

========================================
Valgrind Test Summary
========================================
Total tests run:    4
Passed:             4
Failed:             0
Tests with leaks:   0
Tests with errors:  0

✓ No memory leaks or errors detected!
```

### Performance Benchmarks

Run performance benchmarks:

```bash
./scripts/run_performance_tests.sh
```

**Output:**
```
========================================
ScratchBird Performance Benchmarks
========================================

1. INSERT Performance
==========================================
Sequential INSERT (1000 rows)                              125 ms
  → 8000 rows/sec
Batch INSERT in transaction (10000 rows)                   453 ms
  → 22075 rows/sec

2. SELECT Performance
==========================================
Full table scan COUNT(*)                                    45 ms
Index scan with WHERE (100 rows)                             8 ms
ORDER BY with LIMIT                                         12 ms
GROUP BY with aggregation                                   35 ms

3. UPDATE Performance
==========================================
Single row UPDATE                                            3 ms
Batch UPDATE (100 rows)                                     15 ms
Full table UPDATE (10000 rows)                             234 ms

...

✓ All benchmarks complete!
Report saved to: results/performance/benchmark_report_20251231_120000.txt
```

## Test Script Format

### SQL Test Script Structure

Each test script follows this format:

```sql
-- ScratchBird Native Test: <Test Name>
-- Test ID: <category>_<number>
-- Description: <Brief description>

-- Create database
CREATE DATABASE <test_database>;
\c <test_database>;

-- Test case 1
CREATE TABLE ...;
INSERT INTO ...;
SELECT * FROM ...;

-- Test case 2
...

-- Cleanup
DROP TABLE ...;
DROP DATABASE <test_database>;
```

### Expected Output Format

Expected output files contain the exact output that should be produced:

```
-- ScratchBird Native Test: Data Types
-- Test ID: basic_001

-- Create database
CREATE DATABASE test_datatypes;
\c test_datatypes;

-- Test INTEGER types
CREATE TABLE test_integers ...
 id | small_val | int_val | big_val
----+-----------+---------+-------------------
  1 |       100 |   50000 | 9223372036854775807
  2 |      -100 |  -50000 | -9223372036854775807
  3 |         0 |       0 | 0
(3 rows)
...
```

## Creating New Tests

### 1. Create SQL Test Script

```bash
cd tests/compatibility/scratchbird/tests/<category>/
vim 00X_<test_name>.sql
```

Follow the standard test script format shown above.

### 2. Generate Expected Output

```bash
cd ../..
./scripts/run_tests.sh --test <category>/00X_<test_name> --update-expected
```

### 3. Verify Test

```bash
./scripts/run_tests.sh --test <category>/00X_<test_name>
```

## Test Runner Options

### `run_tests.sh`

| Option | Description |
|--------|-------------|
| `-c, --category CATEGORY` | Run only tests in specified category |
| `-t, --test TEST_FILE` | Run specific test file |
| `-v, --verbose` | Verbose output |
| `-k, --keep-db` | Keep test database after completion |
| `-u, --update-expected` | Update expected output files |
| `-h, --help` | Show help message |

### `run_valgrind_tests.sh`

Automatically runs basic tests and a stress test under Valgrind. No options needed.

### `run_performance_tests.sh`

Automatically runs all benchmark categories. No options needed.

## Continuous Integration

### GitHub Actions Example

```yaml
name: ScratchBird Tests

on: [push, pull_request]

jobs:
  functional-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Debug ..
          make -j$(nproc)
      - name: Run Tests
        run: |
          cd tests/compatibility/scratchbird
          ./scripts/run_tests.sh

  memory-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Install Valgrind
        run: sudo apt-get install -y valgrind
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Debug ..
          make -j$(nproc)
      - name: Run Valgrind Tests
        run: |
          cd tests/compatibility/scratchbird
          ./scripts/run_valgrind_tests.sh

  performance-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: |
          mkdir build && cd build
          cmake -DCMAKE_BUILD_TYPE=Release ..
          make -j$(nproc)
      - name: Run Performance Tests
        run: |
          cd tests/compatibility/scratchbird
          ./scripts/run_performance_tests.sh
```

## Test Coverage

### Current Coverage

| Category | Tests | Coverage |
|----------|-------|----------|
| Data Types | 1 | INTEGER, REAL, VARCHAR, TEXT, BOOLEAN, DATE, TIME, TIMESTAMP, UUID, BLOB, NULL |
| DDL | 1 | CREATE/ALTER/DROP TABLE, INDEX, CONSTRAINT, FOREIGN KEY |
| DML | 1 | INSERT, UPDATE, DELETE, SELECT, UPSERT, TRUNCATE |
| Indexes | 1 | B-Tree, GiST, GIN, BRIN, Hash, Partial, Expression, Covering, Unique |
| Domains | 1 | CREATE DOMAIN, Constraints, Nested, Composite Types |
| Transactions | 1 | BEGIN, COMMIT, ROLLBACK, SAVEPOINT, RETAIN, Isolation Levels |
| MVCC | 1 | Visibility, Snapshot Isolation, Phantom Reads |
| Security | 1 | Users, Roles, Permissions, Row-Level Security |

### Planned Tests

- Stored procedures and functions
- Triggers
- Views and materialized views
- Foreign data wrappers
- Full-text search
- JSON/JSONB support
- Array operations
- Window functions
- CTEs (Common Table Expressions)
- Partitioning
- Replication

## Debugging Failed Tests

### 1. Check Test Output

```bash
cat results/<category>_<test>.result
```

### 2. Compare with Expected

```bash
diff -u expected/<category>/<test>.expected results/<category>_<test>.result
```

### 3. View Diff

```bash
cat results/<category>_<test>.diff
```

### 4. Run Test Manually

```bash
../../build/sb_isql -f tests/<category>/<test>.sql
```

### 5. Debug with Verbose Mode

```bash
./scripts/run_tests.sh --test <category>/<test> --verbose
```

## Valgrind Suppressions

If you encounter false positives from Valgrind, add suppressions to `config/valgrind.supp`:

```
{
   description_of_false_positive
   Memcheck:Leak
   ...
   fun:function_name
}
```

## Performance Baselines

Establish performance baselines by running benchmarks on your target hardware:

```bash
./scripts/run_performance_tests.sh > baselines/$(hostname)_$(date +%Y%m%d).txt
```

Track performance regressions by comparing against baselines.

## Related Documentation

- [Main Compatibility Test README](../README.md) - Overall compatibility testing
- [Plan 06: Test Automation](../../../docs/planning/PLAN_06_TEST_AUTOMATION_DESIGN.md) - Test infrastructure design
- [MGA Rules](../../../MGA_RULES.md) - MGA transaction rules
- [Implementation Standards](../../../IMPLEMENTATION_STANDARDS.md) - Code standards

## Contributing

When adding new tests:

1. Place test in appropriate category (`basic/`, `advanced/`, etc.)
2. Follow standard test script format
3. Generate expected output
4. Run test to verify
5. Add memory/performance tests if applicable
6. Update this README with new test coverage

## License

These tests are part of the ScratchBird project and use the same license.

---

**Last Updated:** 2025-12-31
**Test Count:** 8 SQL scripts
**Test Runner Version:** 1.0
