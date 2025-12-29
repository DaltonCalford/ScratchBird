# Plan 06: Test Automation Design for Multi-Database Compatibility Testing

**Plan Status:** 📋 DESIGN PHASE
**Version:** 1.0
**Created:** 2025-12-29
**Dependencies:** Plan 06 (Dedicated ISQL Clients)
**Estimated Effort:** 60-90 hours

---

## Document Purpose

This document specifies the **test automation infrastructure** for running SQL compatibility tests from:
1. **FirebirdSQL** - fbt-repository (8,000+ tests)
2. **MySQL** - mysql-test suite (1,000s of tests)
3. **PostgreSQL** - regression test suite (200+ tests)
4. **ScratchBird V2** - Native ScratchBird tests (new)

**This design enables:**
- Automated pulling and conversion of official database test suites
- Organized storage of tests by database and category
- Automated test execution with dedicated ISQL clients
- Test result reporting and compatibility tracking
- CI/CD integration for continuous compatibility testing

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Directory Structure](#directory-structure)
3. [Test Repository Management](#test-repository-management)
4. [Test Conversion Framework](#test-conversion-framework)
5. [Test Runner Framework](#test-runner-framework)
6. [Test Categorization and Filtering](#test-categorization-and-filtering)
7. [Result Reporting](#result-reporting)
8. [CI/CD Integration](#cicd-integration)
9. [Implementation Checklist](#implementation-checklist)

---

## Architecture Overview

### Design Principles

1. **Separation of Concerns:** Original tests, converted tests, and results are stored separately
2. **Database Independence:** Each database's tests managed independently
3. **Automation First:** Everything scriptable, minimal manual intervention
4. **Incremental Execution:** Can run individual tests, categories, or full suites
5. **Reproducibility:** Test execution deterministic and reproducible
6. **Result Tracking:** Historical tracking of test pass/fail rates

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Test Automation System                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐        │
│  │   Test       │    │   Test       │    │   Test       │        │
│  │  Repository  │───▶│  Conversion  │───▶│  Execution   │        │
│  │  Management  │    │   Scripts    │    │   Runner     │        │
│  └──────────────┘    └──────────────┘    └──────┬───────┘        │
│         │                                        │                │
│         ▼                                        ▼                │
│  ┌──────────────┐                        ┌──────────────┐        │
│  │   Original   │                        │   Results    │        │
│  │    Tests     │                        │  & Reports   │        │
│  └──────────────┘                        └──────────────┘        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
         │                                           │
         ▼                                           ▼
┌──────────────────┐                        ┌──────────────┐
│  External Repos  │                        │    CI/CD     │
│  (git submodules)│                        │  Pipeline    │
└──────────────────┘                        └──────────────┘
```

### Data Flow

```
1. Pull Test Repositories (git clone/submodule)
   ↓
2. Scan and Categorize Tests
   ↓
3. Convert to SQL Format (.sql files)
   ↓
4. Execute with Dedicated ISQL Clients
   ↓
5. Capture Results and Generate Reports
   ↓
6. Store Historical Results for Tracking
```

---

## Directory Structure

### Top-Level Organization

```
tests/
├── compatibility/                      # All compatibility testing
│   ├── firebird/                      # Firebird compatibility tests
│   ├── mysql/                         # MySQL compatibility tests
│   ├── postgresql/                    # PostgreSQL compatibility tests
│   ├── scratchbird/                   # Native ScratchBird tests
│   ├── common/                        # Shared test utilities
│   └── results/                       # Test execution results
│
└── [existing test directories]        # Unit, integration tests remain
```

### Firebird Test Structure

```
tests/compatibility/firebird/
├── README.md                          # Firebird test documentation
├── repos/                             # Original test repositories
│   └── fbt-repository/               # Git submodule
│       └── tests/                    # 8,000+ .fbt test files
│
├── converted/                         # Converted SQL tests
│   ├── basic/                        # Basic functionality
│   ├── functional/                   # Functional tests
│   ├── bugs/                         # Bug regression tests
│   └── [other categories]/
│
├── scripts/                           # Firebird-specific scripts
│   ├── convert_fbt_to_sql.py        # .fbt → .sql converter
│   ├── run_firebird_tests.sh        # Test runner
│   └── categorize_tests.py          # Test organizer
│
├── expected/                          # Expected output (if available)
│   └── [test_name].expected
│
├── config/                            # Test configuration
│   ├── test_manifest.json           # Test inventory
│   ├── disabled_tests.txt           # Known failures/skip list
│   └── test_categories.json         # Category definitions
│
└── results/                           # Test execution results
    ├── latest/                       # Most recent run
    └── history/                      # Historical results
        └── YYYY-MM-DD_HHMMSS/
```

### MySQL Test Structure

```
tests/compatibility/mysql/
├── README.md
├── repos/
│   └── mysql-server/                 # Git submodule (sparse checkout)
│       └── mysql-test/
│           ├── t/                    # Test files (.test)
│           └── r/                    # Expected results (.result)
│
├── converted/
│   ├── main/                         # Main test suite
│   ├── innodb/                       # InnoDB tests
│   ├── performance/                  # Performance tests
│   └── [other suites]/
│
├── scripts/
│   ├── convert_mysql_test.py        # .test → .sql converter
│   ├── run_mysql_tests.sh           # Test runner
│   └── parse_expected_results.py    # .result parser
│
├── expected/
│   └── [test_name].expected
│
├── config/
│   ├── test_manifest.json
│   ├── disabled_tests.txt
│   └── test_categories.json
│
└── results/
    ├── latest/
    └── history/
```

### PostgreSQL Test Structure

```
tests/compatibility/postgresql/
├── README.md
├── repos/
│   └── postgres/                     # Git submodule (sparse checkout)
│       └── src/test/regress/
│           ├── sql/                  # SQL test files (.sql)
│           └── expected/             # Expected output (.out)
│
├── converted/
│   ├── core/                         # Core SQL tests
│   ├── advanced/                     # Advanced features
│   ├── aggregates/                   # Aggregate functions
│   └── [other categories]/
│
├── scripts/
│   ├── convert_pg_test.py           # PostgreSQL test converter
│   ├── run_postgresql_tests.sh      # Test runner
│   └── parse_expected_output.py     # .out parser
│
├── expected/
│   └── [test_name].expected
│
├── config/
│   ├── test_manifest.json
│   ├── disabled_tests.txt
│   └── test_categories.json
│
└── results/
    ├── latest/
    └── history/
```

### ScratchBird V2 Test Structure

```
tests/compatibility/scratchbird/
├── README.md                          # Native test documentation
├── tests/                             # Native ScratchBird tests
│   ├── basic/                        # Basic V2 syntax
│   ├── advanced/                     # Advanced V2 features
│   ├── sblr/                         # SBLR-specific tests
│   └── extensions/                   # ScratchBird extensions
│
├── scripts/
│   └── run_scratchbird_tests.sh     # Test runner
│
├── expected/
│   └── [test_name].expected
│
├── config/
│   ├── test_manifest.json
│   └── test_categories.json
│
└── results/
    ├── latest/
    └── history/
```

### Common Utilities

```
tests/compatibility/common/
├── lib/                               # Shared libraries
│   ├── test_runner.py                # Generic test runner
│   ├── result_parser.py              # Output parser
│   ├── diff_engine.py                # Result comparison
│   └── report_generator.py           # Report generation
│
├── templates/                         # Report templates
│   ├── html_report.html.j2
│   ├── markdown_report.md.j2
│   └── json_report.json.j2
│
└── config/
    └── common_config.json            # Global test configuration
```

---

## Test Repository Management

### Git Submodule Approach

Use git submodules to track official test repositories:

```bash
# Add Firebird test repository
git submodule add \
  https://github.com/FirebirdSQL/fbt-repository.git \
  tests/compatibility/firebird/repos/fbt-repository

# Add MySQL test repository (sparse checkout for mysql-test only)
git submodule add \
  https://github.com/mysql/mysql-server.git \
  tests/compatibility/mysql/repos/mysql-server

# Add PostgreSQL test repository (sparse checkout for regress tests)
git submodule add \
  https://github.com/postgres/postgres.git \
  tests/compatibility/postgresql/repos/postgres

# Initialize and update submodules
git submodule update --init --recursive
```

### Sparse Checkout Configuration

For large repositories (MySQL, PostgreSQL), use sparse checkout:

```bash
# MySQL sparse checkout
cd tests/compatibility/mysql/repos/mysql-server
git config core.sparseCheckout true
echo "mysql-test/" >> .git/info/sparse-checkout
git read-tree -mu HEAD

# PostgreSQL sparse checkout
cd tests/compatibility/postgresql/repos/postgres
git config core.sparseCheckout true
echo "src/test/regress/" >> .git/info/sparse-checkout
git read-tree -mu HEAD
```

### Update Script

Create `tests/compatibility/scripts/update_test_repos.sh`:

```bash
#!/bin/bash
# Update all test repository submodules to latest versions

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPAT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Updating test repository submodules..."

# Update Firebird tests
echo "  - Updating Firebird fbt-repository..."
cd "$COMPAT_DIR/firebird/repos/fbt-repository"
git pull origin master
FIREBIRD_COMMIT=$(git rev-parse --short HEAD)
FIREBIRD_DATE=$(git log -1 --format=%cd --date=short)
echo "    ✓ Updated to commit $FIREBIRD_COMMIT ($FIREBIRD_DATE)"

# Update MySQL tests
echo "  - Updating MySQL mysql-test..."
cd "$COMPAT_DIR/mysql/repos/mysql-server"
git pull origin trunk
MYSQL_COMMIT=$(git rev-parse --short HEAD)
MYSQL_DATE=$(git log -1 --format=%cd --date=short)
echo "    ✓ Updated to commit $MYSQL_COMMIT ($MYSQL_DATE)"

# Update PostgreSQL tests
echo "  - Updating PostgreSQL regress tests..."
cd "$COMPAT_DIR/postgresql/repos/postgres"
git pull origin master
POSTGRES_COMMIT=$(git rev-parse --short HEAD)
POSTGRES_DATE=$(git log -1 --format=%cd --date=short)
echo "    ✓ Updated to commit $POSTGRES_COMMIT ($POSTGRES_DATE)"

# Log update to history
cd "$COMPAT_DIR"
cat > update_history.txt <<EOF
Test Repository Update - $(date +%Y-%m-%d)

Firebird:    $FIREBIRD_COMMIT ($FIREBIRD_DATE)
MySQL:       $MYSQL_COMMIT ($MYSQL_DATE)
PostgreSQL:  $POSTGRES_COMMIT ($POSTGRES_DATE)
EOF

echo ""
echo "✓ All test repositories updated successfully!"
echo "  See update_history.txt for details"
```

---

## Test Conversion Framework

### Firebird .fbt Converter

**Input Format:** Python-based .fbt files with embedded SQL and expected output

**Example .fbt file:**
```python
# .fbt file format (simplified)
id = 'bugs.core_1234'
tracker_id = 'CORE-1234'
title = 'SELECT with GROUP BY'

db = db_factory()
test_script = """
    SELECT COUNT(*) FROM RDB$DATABASE;
"""

act = isql_act('db', test_script)

expected_stdout = """
    COUNT
    =====
        1
"""
```

**Converter:** `tests/compatibility/firebird/scripts/convert_fbt_to_sql.py`

```python
#!/usr/bin/env python3
"""
Convert Firebird .fbt test files to plain .sql files
"""

import re
import ast
import sys
from pathlib import Path

def parse_fbt_file(fbt_path):
    """Parse .fbt file and extract test components"""
    with open(fbt_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract metadata
    test_id = re.search(r"id\s*=\s*['\"](.+?)['\"]", content)
    title = re.search(r"title\s*=\s*['\"](.+?)['\"]", content)

    # Extract SQL script
    test_script = re.search(
        r"test_script\s*=\s*['\"]\"\"(.+?)\"\"\"['\"]",
        content,
        re.DOTALL
    )

    # Extract expected output
    expected_stdout = re.search(
        r"expected_stdout\s*=\s*['\"]\"\"(.+?)\"\"\"['\"]",
        content,
        re.DOTALL
    )

    return {
        'id': test_id.group(1) if test_id else 'unknown',
        'title': title.group(1) if title else 'Untitled',
        'sql': test_script.group(1).strip() if test_script else '',
        'expected': expected_stdout.group(1).strip() if expected_stdout else ''
    }

def convert_fbt_to_sql(fbt_path, output_dir):
    """Convert .fbt to .sql file"""
    test_data = parse_fbt_file(fbt_path)

    # Generate SQL file
    sql_filename = Path(fbt_path).stem + '.sql'
    sql_path = Path(output_dir) / sql_filename

    with open(sql_path, 'w', encoding='utf-8') as f:
        f.write(f"-- Test ID: {test_data['id']}\n")
        f.write(f"-- Title: {test_data['title']}\n")
        f.write(f"-- Converted from: {Path(fbt_path).name}\n\n")
        f.write(test_data['sql'])
        f.write("\n")

    # Generate expected output file
    if test_data['expected']:
        expected_path = Path(output_dir).parent / 'expected' / (Path(fbt_path).stem + '.expected')
        expected_path.parent.mkdir(parents=True, exist_ok=True)
        with open(expected_path, 'w', encoding='utf-8') as f:
            f.write(test_data['expected'])

    return sql_path

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: convert_fbt_to_sql.py <input.fbt> <output_dir>")
        sys.exit(1)

    fbt_file = sys.argv[1]
    output_dir = sys.argv[2]

    sql_path = convert_fbt_to_sql(fbt_file, output_dir)
    print(f"Converted: {fbt_file} → {sql_path}")
```

### MySQL .test Converter

**Input Format:** MySQL test format with directives and SQL

**Example .test file:**
```sql
# MySQL test format
--source include/have_innodb.inc

--echo #
--echo # Bug #12345: Test description
--echo #

CREATE TABLE t1 (id INT PRIMARY KEY);
INSERT INTO t1 VALUES (1), (2), (3);

SELECT * FROM t1 ORDER BY id;

--echo # Expected: 3 rows

DROP TABLE t1;
```

**Converter:** `tests/compatibility/mysql/scripts/convert_mysql_test.py`

```python
#!/usr/bin/env python3
"""
Convert MySQL .test files to plain .sql files
"""

import re
import sys
from pathlib import Path

def convert_mysql_test(test_path, output_dir):
    """Convert MySQL .test to .sql file"""
    with open(test_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Extract test name
    test_name = Path(test_path).stem

    # Process directives
    sql_lines = []
    for line in content.split('\n'):
        # Skip directives (--source, --disable_warnings, etc.)
        if line.startswith('--source') or \
           line.startswith('--disable') or \
           line.startswith('--enable'):
            continue

        # Convert --echo to comments
        if line.startswith('--echo'):
            sql_lines.append('-- ' + line[6:].strip())
        # Keep SQL and comments
        elif not line.startswith('--') or line.startswith('-- '):
            sql_lines.append(line)

    # Generate SQL file
    sql_path = Path(output_dir) / f"{test_name}.sql"
    with open(sql_path, 'w', encoding='utf-8') as f:
        f.write(f"-- MySQL Test: {test_name}\n")
        f.write(f"-- Converted from: {Path(test_path).name}\n\n")
        f.write('\n'.join(sql_lines))

    # Check for expected results file
    result_path = Path(test_path).parent.parent / 'r' / f"{test_name}.result"
    if result_path.exists():
        expected_path = Path(output_dir).parent / 'expected' / f"{test_name}.expected"
        expected_path.parent.mkdir(parents=True, exist_ok=True)

        # Copy and process result file
        with open(result_path, 'r', encoding='utf-8') as f:
            result_content = f.read()

        with open(expected_path, 'w', encoding='utf-8') as f:
            f.write(result_content)

    return sql_path

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: convert_mysql_test.py <input.test> <output_dir>")
        sys.exit(1)

    test_file = sys.argv[1]
    output_dir = sys.argv[2]

    sql_path = convert_mysql_test(test_file, output_dir)
    print(f"Converted: {test_file} → {sql_path}")
```

### PostgreSQL Test Converter

**Input Format:** Pure SQL files (minimal conversion needed)

**Converter:** `tests/compatibility/postgresql/scripts/convert_pg_test.py`

```python
#!/usr/bin/env python3
"""
Convert PostgreSQL regression tests to standard format
"""

import sys
from pathlib import Path
import shutil

def convert_pg_test(test_path, output_dir):
    """Convert PostgreSQL .sql test (mostly just copy with metadata)"""
    test_name = Path(test_path).stem

    # Read original SQL
    with open(test_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Generate SQL file with metadata header
    sql_path = Path(output_dir) / f"{test_name}.sql"
    with open(sql_path, 'w', encoding='utf-8') as f:
        f.write(f"-- PostgreSQL Test: {test_name}\n")
        f.write(f"-- Converted from: {Path(test_path).name}\n\n")
        f.write(content)

    # Check for expected output
    expected_source = Path(test_path).parent.parent / 'expected' / f"{test_name}.out"
    if expected_source.exists():
        expected_path = Path(output_dir).parent / 'expected' / f"{test_name}.expected"
        expected_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(expected_source, expected_path)

    return sql_path

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: convert_pg_test.py <input.sql> <output_dir>")
        sys.exit(1)

    test_file = sys.argv[1]
    output_dir = sys.argv[2]

    sql_path = convert_pg_test(test_file, output_dir)
    print(f"Converted: {test_file} → {sql_path}")
```

### Batch Conversion Script

**Script:** `tests/compatibility/scripts/convert_all_tests.sh`

```bash
#!/bin/bash
# Convert all tests from original repositories to SQL format

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPAT_DIR="$(dirname "$SCRIPT_DIR")"

echo "Converting all compatibility tests to SQL format..."

# Convert Firebird tests
echo ""
echo "=== Converting Firebird Tests ==="
FIREBIRD_REPO="$COMPAT_DIR/firebird/repos/fbt-repository/tests"
FIREBIRD_OUTPUT="$COMPAT_DIR/firebird/converted"

if [ -d "$FIREBIRD_REPO" ]; then
    mkdir -p "$FIREBIRD_OUTPUT"

    find "$FIREBIRD_REPO" -name "*.fbt" | while read fbt_file; do
        # Determine category from path
        category=$(dirname "${fbt_file#$FIREBIRD_REPO/}")
        output_dir="$FIREBIRD_OUTPUT/$category"
        mkdir -p "$output_dir"

        python3 "$COMPAT_DIR/firebird/scripts/convert_fbt_to_sql.py" \
            "$fbt_file" "$output_dir" || echo "  ⚠ Failed: $fbt_file"
    done

    count=$(find "$FIREBIRD_OUTPUT" -name "*.sql" | wc -l)
    echo "  ✓ Converted $count Firebird tests"
else
    echo "  ⚠ Firebird repository not found. Run: git submodule update --init"
fi

# Convert MySQL tests
echo ""
echo "=== Converting MySQL Tests ==="
MYSQL_REPO="$COMPAT_DIR/mysql/repos/mysql-server/mysql-test/t"
MYSQL_OUTPUT="$COMPAT_DIR/mysql/converted"

if [ -d "$MYSQL_REPO" ]; then
    mkdir -p "$MYSQL_OUTPUT"

    find "$MYSQL_REPO" -name "*.test" | while read test_file; do
        # Determine suite (main tests are directly in t/)
        suite="main"
        output_dir="$MYSQL_OUTPUT/$suite"
        mkdir -p "$output_dir"

        python3 "$COMPAT_DIR/mysql/scripts/convert_mysql_test.py" \
            "$test_file" "$output_dir" || echo "  ⚠ Failed: $test_file"
    done

    count=$(find "$MYSQL_OUTPUT" -name "*.sql" | wc -l)
    echo "  ✓ Converted $count MySQL tests"
else
    echo "  ⚠ MySQL repository not found. Run: git submodule update --init"
fi

# Convert PostgreSQL tests
echo ""
echo "=== Converting PostgreSQL Tests ==="
POSTGRES_REPO="$COMPAT_DIR/postgresql/repos/postgres/src/test/regress/sql"
POSTGRES_OUTPUT="$COMPAT_DIR/postgresql/converted"

if [ -d "$POSTGRES_REPO" ]; then
    mkdir -p "$POSTGRES_OUTPUT/core"

    find "$POSTGRES_REPO" -name "*.sql" | while read sql_file; do
        python3 "$COMPAT_DIR/postgresql/scripts/convert_pg_test.py" \
            "$sql_file" "$POSTGRES_OUTPUT/core" || echo "  ⚠ Failed: $sql_file"
    done

    count=$(find "$POSTGRES_OUTPUT" -name "*.sql" | wc -l)
    echo "  ✓ Converted $count PostgreSQL tests"
else
    echo "  ⚠ PostgreSQL repository not found. Run: git submodule update --init"
fi

echo ""
echo "✓ Test conversion complete!"
```

---

## Test Runner Framework

### Generic Test Runner Library

**File:** `tests/compatibility/common/lib/test_runner.py`

```python
#!/usr/bin/env python3
"""
Generic test runner for SQL compatibility tests
"""

import subprocess
import json
import time
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional
from enum import Enum

class TestStatus(Enum):
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    TIMEOUT = "timeout"
    ERROR = "error"

@dataclass
class TestResult:
    """Result of a single test execution"""
    test_name: str
    test_path: Path
    status: TestStatus
    execution_time: float
    output: str
    expected_output: Optional[str] = None
    diff: Optional[str] = None
    error_message: Optional[str] = None

class TestRunner:
    """Generic test runner for database compatibility tests"""

    def __init__(self, isql_client: str, connection_args: List[str],
                 timeout: int = 300):
        """
        Initialize test runner

        Args:
            isql_client: Path to ISQL client (sb_fb_isql, sb_pg_isql, etc.)
            connection_args: Connection arguments (user, password, database, etc.)
            timeout: Test timeout in seconds (default: 300)
        """
        self.isql_client = isql_client
        self.connection_args = connection_args
        self.timeout = timeout

    def run_test(self, test_path: Path, expected_path: Optional[Path] = None) -> TestResult:
        """
        Run a single test

        Args:
            test_path: Path to .sql test file
            expected_path: Path to expected output file (optional)

        Returns:
            TestResult object
        """
        test_name = test_path.stem
        start_time = time.time()

        try:
            # Build command
            cmd = [self.isql_client] + self.connection_args + ['-i', str(test_path)]

            # Execute test
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=self.timeout
            )

            execution_time = time.time() - start_time
            output = result.stdout

            # Load expected output if provided
            expected_output = None
            if expected_path and expected_path.exists():
                with open(expected_path, 'r', encoding='utf-8') as f:
                    expected_output = f.read()

            # Determine status
            if result.returncode != 0:
                status = TestStatus.ERROR
                error_message = result.stderr
            elif expected_output:
                # Compare with expected output
                if self._normalize_output(output) == self._normalize_output(expected_output):
                    status = TestStatus.PASSED
                else:
                    status = TestStatus.FAILED
                    diff = self._generate_diff(expected_output, output)
            else:
                # No expected output, consider passed if no error
                status = TestStatus.PASSED

            return TestResult(
                test_name=test_name,
                test_path=test_path,
                status=status,
                execution_time=execution_time,
                output=output,
                expected_output=expected_output,
                diff=diff if status == TestStatus.FAILED else None,
                error_message=error_message if status == TestStatus.ERROR else None
            )

        except subprocess.TimeoutExpired:
            execution_time = time.time() - start_time
            return TestResult(
                test_name=test_name,
                test_path=test_path,
                status=TestStatus.TIMEOUT,
                execution_time=execution_time,
                output="",
                error_message=f"Test timed out after {self.timeout} seconds"
            )

        except Exception as e:
            execution_time = time.time() - start_time
            return TestResult(
                test_name=test_name,
                test_path=test_path,
                status=TestStatus.ERROR,
                execution_time=execution_time,
                output="",
                error_message=str(e)
            )

    def run_test_suite(self, test_dir: Path, expected_dir: Optional[Path] = None,
                      filter_pattern: str = "*.sql") -> List[TestResult]:
        """
        Run a suite of tests

        Args:
            test_dir: Directory containing test files
            expected_dir: Directory containing expected output files
            filter_pattern: Glob pattern for test files (default: *.sql)

        Returns:
            List of TestResult objects
        """
        results = []

        # Find all test files
        test_files = sorted(test_dir.glob(filter_pattern))

        for test_file in test_files:
            # Find corresponding expected output
            expected_file = None
            if expected_dir:
                expected_file = expected_dir / f"{test_file.stem}.expected"

            # Run test
            result = self.run_test(test_file, expected_file)
            results.append(result)

            # Print progress
            status_symbol = {
                TestStatus.PASSED: "✓",
                TestStatus.FAILED: "✗",
                TestStatus.SKIPPED: "⊘",
                TestStatus.TIMEOUT: "⏱",
                TestStatus.ERROR: "⚠"
            }
            print(f"{status_symbol[result.status]} {result.test_name} ({result.execution_time:.2f}s)")

        return results

    def _normalize_output(self, output: str) -> str:
        """Normalize output for comparison (remove whitespace differences, etc.)"""
        # Remove trailing whitespace from lines
        lines = [line.rstrip() for line in output.split('\n')]
        # Remove empty lines at end
        while lines and not lines[-1]:
            lines.pop()
        return '\n'.join(lines)

    def _generate_diff(self, expected: str, actual: str) -> str:
        """Generate unified diff between expected and actual output"""
        import difflib

        expected_lines = expected.split('\n')
        actual_lines = actual.split('\n')

        diff = difflib.unified_diff(
            expected_lines,
            actual_lines,
            fromfile='expected',
            tofile='actual',
            lineterm=''
        )

        return '\n'.join(diff)
```

### Firebird Test Runner

**File:** `tests/compatibility/firebird/scripts/run_firebird_tests.sh`

```bash
#!/bin/bash
# Run Firebird compatibility tests using sb_fb_isql

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIREBIRD_DIR="$(dirname "$SCRIPT_DIR")"
COMMON_DIR="$FIREBIRD_DIR/../common"

# Configuration
ISQL_CLIENT="${ISQL_CLIENT:-./build/src/cli/sb_fb_isql}"
DATABASE="${DATABASE:-/remote/emulated/firebird/localhost/testdb.fdb}"
USER="${USER:-SYSDBA}"
PASSWORD="${PASSWORD:-masterkey}"
TIMEOUT="${TIMEOUT:-300}"

# Test directory
TEST_DIR="${TEST_DIR:-$FIREBIRD_DIR/converted}"
EXPECTED_DIR="$FIREBIRD_DIR/expected"
RESULTS_DIR="$FIREBIRD_DIR/results/latest"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Run tests
echo "Running Firebird compatibility tests..."
echo "  Client: $ISQL_CLIENT"
echo "  Database: $DATABASE"
echo "  Test directory: $TEST_DIR"
echo ""

python3 - <<EOF
import sys
sys.path.insert(0, '$COMMON_DIR/lib')

from test_runner import TestRunner, TestStatus
from pathlib import Path
import json

# Initialize test runner
runner = TestRunner(
    isql_client='$ISQL_CLIENT',
    connection_args=['-user', '$USER', '-password', '$PASSWORD', '$DATABASE'],
    timeout=$TIMEOUT
)

# Run test suite
test_dir = Path('$TEST_DIR')
expected_dir = Path('$EXPECTED_DIR')
results_dir = Path('$RESULTS_DIR')

results = runner.run_test_suite(test_dir, expected_dir)

# Generate summary
passed = sum(1 for r in results if r.status == TestStatus.PASSED)
failed = sum(1 for r in results if r.status == TestStatus.FAILED)
timeout = sum(1 for r in results if r.status == TestStatus.TIMEOUT)
error = sum(1 for r in results if r.status == TestStatus.ERROR)
total = len(results)

print("")
print("=" * 60)
print("Test Summary")
print("=" * 60)
print(f"Total:   {total}")
print(f"Passed:  {passed} ({100*passed/total:.1f}%)")
print(f"Failed:  {failed} ({100*failed/total:.1f}%)")
print(f"Timeout: {timeout} ({100*timeout/total:.1f}%)")
print(f"Error:   {error} ({100*error/total:.1f}%)")
print("=" * 60)

# Save results as JSON
results_json = []
for result in results:
    results_json.append({
        'test_name': result.test_name,
        'test_path': str(result.test_path),
        'status': result.status.value,
        'execution_time': result.execution_time,
        'error_message': result.error_message
    })

with open(results_dir / 'results.json', 'w') as f:
    json.dump(results_json, f, indent=2)

# Save failed tests
if failed > 0:
    with open(results_dir / 'failed_tests.txt', 'w') as f:
        for result in results:
            if result.status == TestStatus.FAILED:
                f.write(f"{result.test_name}\n")

# Exit with error if tests failed
sys.exit(1 if failed > 0 else 0)
EOF
```

### MySQL and PostgreSQL Test Runners

Similar scripts for `tests/compatibility/mysql/scripts/run_mysql_tests.sh` and `tests/compatibility/postgresql/scripts/run_postgresql_tests.sh` with database-specific connection parameters.

---

## Test Categorization and Filtering

### Test Manifest

**File:** `tests/compatibility/firebird/config/test_manifest.json`

```json
{
  "version": "1.0",
  "database": "firebird",
  "test_repository": "fbt-repository",
  "last_updated": "2025-12-29",
  "categories": {
    "basic": {
      "description": "Basic SQL functionality",
      "path": "converted/basic",
      "priority": 1,
      "estimated_tests": 500
    },
    "functional": {
      "description": "Functional tests",
      "path": "converted/functional",
      "priority": 2,
      "estimated_tests": 3000
    },
    "bugs": {
      "description": "Bug regression tests",
      "path": "converted/bugs",
      "priority": 3,
      "estimated_tests": 2000
    }
  },
  "disabled_tests": [
    "bugs.core_xxxx",
    "functional.test_yyyy"
  ],
  "known_failures": [
    {
      "test": "bugs.core_1234",
      "reason": "Index type not yet supported",
      "issue": "SB-456"
    }
  ]
}
```

### Category-Based Execution

Run tests by category:

```bash
# Run only basic tests
TEST_DIR=tests/compatibility/firebird/converted/basic \
  ./tests/compatibility/firebird/scripts/run_firebird_tests.sh

# Run only functional tests
TEST_DIR=tests/compatibility/firebird/converted/functional \
  ./tests/compatibility/firebird/scripts/run_firebird_tests.sh
```

---

## Result Reporting

### HTML Report Generation

**File:** `tests/compatibility/common/lib/report_generator.py`

```python
#!/usr/bin/env python3
"""
Generate HTML/Markdown reports from test results
"""

import json
from pathlib import Path
from datetime import datetime
from jinja2 import Template

class ReportGenerator:
    """Generate test result reports"""

    def generate_html_report(self, results_json: Path, output_path: Path):
        """Generate HTML report from test results"""

        # Load results
        with open(results_json, 'r') as f:
            results = json.load(f)

        # Calculate statistics
        total = len(results)
        passed = sum(1 for r in results if r['status'] == 'passed')
        failed = sum(1 for r in results if r['status'] == 'failed')
        timeout = sum(1 for r in results if r['status'] == 'timeout')
        error = sum(1 for r in results if r['status'] == 'error')

        # Generate HTML
        template = Template('''
<!DOCTYPE html>
<html>
<head>
    <title>Test Results - {{ database }} Compatibility</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .summary { background: #f0f0f0; padding: 20px; border-radius: 5px; }
        .passed { color: green; }
        .failed { color: red; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { padding: 10px; border: 1px solid #ddd; text-align: left; }
        th { background: #4CAF50; color: white; }
    </style>
</head>
<body>
    <h1>{{ database }} Compatibility Test Results</h1>
    <p>Generated: {{ timestamp }}</p>

    <div class="summary">
        <h2>Summary</h2>
        <p><strong>Total Tests:</strong> {{ total }}</p>
        <p class="passed"><strong>Passed:</strong> {{ passed }} ({{ (100*passed/total)|round(1) }}%)</p>
        <p class="failed"><strong>Failed:</strong> {{ failed }} ({{ (100*failed/total)|round(1) }}%)</p>
        <p><strong>Timeout:</strong> {{ timeout }}</p>
        <p><strong>Error:</strong> {{ error }}</p>
    </div>

    <h2>Test Details</h2>
    <table>
        <tr>
            <th>Test Name</th>
            <th>Status</th>
            <th>Time (s)</th>
            <th>Error</th>
        </tr>
        {% for result in results %}
        <tr>
            <td>{{ result.test_name }}</td>
            <td class="{{ result.status }}">{{ result.status }}</td>
            <td>{{ result.execution_time|round(2) }}</td>
            <td>{{ result.error_message or '' }}</td>
        </tr>
        {% endfor %}
    </table>
</body>
</html>
        ''')

        html = template.render(
            database="Firebird",
            timestamp=datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            total=total,
            passed=passed,
            failed=failed,
            timeout=timeout,
            error=error,
            results=results
        )

        with open(output_path, 'w') as f:
            f.write(html)
```

---

## CI/CD Integration

### GitHub Actions Workflow

**File:** `.github/workflows/compatibility_tests.yml`

```yaml
name: Compatibility Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  schedule:
    # Run daily at 2 AM UTC
    - cron: '0 2 * * *'

jobs:
  firebird-compatibility:
    name: Firebird Compatibility Tests
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3
        with:
          submodules: recursive

      - name: Build ScratchBird
        run: |
          mkdir build
          cd build
          cmake ..
          make -j$(nproc)

      - name: Start ScratchBird Server
        run: |
          ./build/src/sb_server &
          sleep 5

      - name: Convert Firebird Tests
        run: |
          ./tests/compatibility/scripts/convert_all_tests.sh

      - name: Run Firebird Tests (Basic)
        run: |
          TEST_DIR=tests/compatibility/firebird/converted/basic \
            ./tests/compatibility/firebird/scripts/run_firebird_tests.sh

      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: firebird-test-results
          path: tests/compatibility/firebird/results/latest/

  mysql-compatibility:
    name: MySQL Compatibility Tests
    runs-on: ubuntu-latest

    steps:
      # Similar to Firebird...

  postgresql-compatibility:
    name: PostgreSQL Compatibility Tests
    runs-on: ubuntu-latest

    steps:
      # Similar to Firebird...
```

---

## Implementation Checklist

### Phase 1: Infrastructure Setup (8-12 hours)

- [ ] Create directory structure for all databases
- [ ] Add git submodules for test repositories
- [ ] Configure sparse checkout for large repositories
- [ ] Create update_test_repos.sh script
- [ ] Test repository cloning and updates

### Phase 2: Test Conversion (16-24 hours)

- [ ] Implement Firebird .fbt converter
- [ ] Implement MySQL .test converter
- [ ] Implement PostgreSQL test converter
- [ ] Create batch conversion script
- [ ] Test conversion on sample tests
- [ ] Convert full test suites

### Phase 3: Test Runner (12-18 hours)

- [ ] Implement generic TestRunner library
- [ ] Create Firebird test runner script
- [ ] Create MySQL test runner script
- [ ] Create PostgreSQL test runner script
- [ ] Test runners with sample tests
- [ ] Add timeout and error handling

### Phase 4: Result Reporting (8-12 hours)

- [ ] Implement result parser
- [ ] Create HTML report generator
- [ ] Create Markdown report generator
- [ ] Create JSON report format
- [ ] Test report generation

### Phase 5: Test Categorization (6-8 hours)

- [ ] Create test manifest files
- [ ] Categorize tests by functionality
- [ ] Create disabled_tests.txt for each database
- [ ] Document known failures
- [ ] Test category-based execution

### Phase 6: CI/CD Integration (10-16 hours)

- [ ] Create GitHub Actions workflow
- [ ] Configure test execution on push/PR
- [ ] Set up daily scheduled runs
- [ ] Add artifact upload for results
- [ ] Test CI/CD pipeline

---

## Conclusion

This test automation design provides a comprehensive framework for managing and executing SQL compatibility tests from three major databases plus native ScratchBird tests.

**Key Deliverables:**
1. Organized directory structure for 10,000+ tests
2. Automated conversion scripts for all database formats
3. Generic test runner framework
4. Result reporting and tracking
5. CI/CD integration for continuous testing

**Estimated Effort:** 60-90 hours

**Next Steps:**
1. Create directory structure
2. Add test repository submodules
3. Implement conversion scripts
4. Build test runner framework
5. Integrate with CI/CD

---

**Plan Status:** ✅ DESIGN COMPLETE - Ready for Implementation
**Created:** 2025-12-29
**Author:** Claude Code
