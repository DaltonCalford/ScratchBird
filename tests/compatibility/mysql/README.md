# MySQL Compatibility Tests

This directory contains converted MySQL compatibility tests from the official [mysql-server repository](https://github.com/mysql/mysql-server). The mysql test snapshot is vendored in this tree under `repos/mysql-server/mysql-test`.

## Statistics

- **Total Tests:** 8,841 SQL test files
- **Categories:** 59 test suites
- **Original Format:** .test (MySQL test format)
- **Source Repository:** mysql/mysql-server (mysql-test directory)

## Directory Structure

```
mysql/
├── repos/
│   └── mysql-server/             # Vendored snapshot (mysql-test subtree)
│       └── mysql-test/
│           ├── t/               # Main test suite (.test files)
│           ├── r/               # Expected results (.result files)
│           └── suite/           # Test suites
├── converted/                    # Converted SQL test files
│   ├── main/                    # Main test suite
│   ├── auth_sec/                # Authentication and security tests
│   ├── binlog/                  # Binary log tests
│   ├── innodb/                  # InnoDB storage engine tests
│   ├── perfschema/              # Performance Schema tests
│   ├── replication/             # Replication tests
│   ├── sys_vars/                # System variables tests
│   └── [other suites]/          # 59 total suites
├── expected/                     # Expected output files
├── scripts/
│   └── convert_mysql_test.py   # Conversion script
└── config/
    └── test_manifest.json       # Test catalog
```

## Test Categories (Suites)

The converted tests are organized into 59 suites including:

- **main/** - Core MySQL functionality tests
- **innodb/** - InnoDB storage engine tests
- **perfschema/** - Performance Schema tests
- **replication/** - Replication tests
- **binlog/** - Binary log tests
- **sys_vars/** - System variable tests
- **auth_sec/** - Authentication and security
- **json/** - JSON data type and functions
- **gis/** - Geographic Information System (spatial data)
- **x/** - X Plugin tests
- And 49 more specialized suites...

## Running Tests

**Prerequisites:** Build CLI clients in the `ScratchBird-driver` repository first. This runner requires `sb_my_isql` for MySQL wire-protocol parity. Generic `sb_isql` is native-protocol only and is rejected for this lane.

**Refresh vendored MySQL tests:**
```bash
./tests/compatibility/scripts/update_test_repos.sh
```

Once the MySQL CLI is available:

```bash
# Run a single test
sb_my_isql -u root -p \
  -D /remote/emulated/mysql/localhost/testdb < converted/main/select.sql \
  > results/select.out

# Run all tests in a suite
for test in converted/innodb/*.sql; do
  sb_my_isql -u root -p -D testdb < "$test" \
    > "results/$(basename $test .sql).out"
done
```

CTest wrapper (opt-in):

```bash
# Uses config/ctest_list.txt and writes to results/ctest/<timestamp>
SCRATCHBIRD_MY_COMPAT_RUN=1 ctest -R CompatibilityMySQL --test-dir build
```

If `sb_my_isql` is unavailable, set `SCRATCHBIRD_MY_ISQL` to a valid `sb_my_isql` path after building FDW CLI wrappers; generic `sb_isql` fallback is intentionally blocked.

## Test Format

Each converted SQL file contains:
- Metadata header with test name and original path
- SQL statements (SELECT, INSERT, UPDATE, DELETE, DDL, etc.)
- Directives converted to comments:
  - `--echo` messages converted to `-- ECHO:` comments
  - `--error` expectations converted to `-- EXPECTED ERROR:` comments
  - `--source` includes noted as `-- DIRECTIVE:` comments

Expected output files (when available) are in the `expected/` directory and were extracted from corresponding `.result` files.

## Conversion Notes

- MySQL test format uses directives like `--echo`, `--error`, `--disable_warnings`
- Directives are converted to comments for documentation
- `DELIMITER` commands are noted but not executed (will need special handling in test runner)
- Original `.result` files are copied to `expected/` directory for output comparison
- Some tests use mysqltest-specific features that may need special handling

## Test Manifest

The `config/test_manifest.json` file contains:
- Complete test inventory with metadata
- Suite organization (59 suites)
- Test statistics
- File paths

## Sources

- **MySQL Server Repository:** https://github.com/mysql/mysql-server
- **MySQL Test Framework:** https://dev.mysql.com/doc/dev/mysql-server/latest/PAGE_MYSQL_TEST_RUN.html
- **Conversion Script:** `scripts/convert_mysql_test.py`

## Related Documentation

- [Plan 06: Dedicated ISQL Clients](/docs/planning/PLAN_06_DEDICATED_ISQL_CLIENTS.md)
- [Plan 06: Test Automation Design](/docs/planning/PLAN_06_TEST_AUTOMATION_DESIGN.md)
- [SQL Compatibility Test Repositories](/docs/findings/SQL_COMPATIBILITY_TEST_REPOSITORIES.md)
