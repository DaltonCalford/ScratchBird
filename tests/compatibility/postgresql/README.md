# PostgreSQL Compatibility Tests

This directory contains converted PostgreSQL regression tests from the official [postgres repository](https://github.com/postgres/postgres).

## Statistics

- **Total Tests:** 238 SQL test files
- **Categories:** 1 category (core regression tests)
- **Original Format:** .sql (pure SQL)
- **Source Repository:** postgres/postgres (src/test/regress/sql/)

## Directory Structure

```
postgresql/
├── repos/
│   └── postgres/                  # Vendored snapshot (sparse checkout via update script)
│       └── src/test/regress/
│           ├── sql/              # Test SQL files
│           └── expected/         # Expected output (.out files)
├── converted/                     # Converted SQL test files
│   └── core/                     # Core regression tests
├── expected/                      # Expected output files
│   └── [test_name].expected      # Copied from .out files
│   └── [test_name]_N.expected    # Variant outputs (platform/version specific)
├── scripts/
│   └── convert_pg_test.py        # Conversion script
└── config/
    └── test_manifest.json         # Test catalog
```

## Test Categories

PostgreSQL tests cover comprehensive SQL functionality:

- **Aggregates** - Aggregate functions (SUM, COUNT, AVG, etc.)
- **Arrays** - Array data type and operations
- **Joins** - JOIN operations (INNER, OUTER, CROSS, etc.)
- **Subqueries** - Subquery functionality
- **Transactions** - Transaction control and isolation
- **Indexes** - Index creation and usage
- **Views** - View creation and queries
- **Triggers** - Trigger functionality
- **Functions** - User-defined functions
- **Data Types** - All PostgreSQL data types
- **DDL** - CREATE, ALTER, DROP statements
- **DML** - SELECT, INSERT, UPDATE, DELETE
- And many more advanced features...

## Running Tests

**Prerequisites:** Build CLI clients in the `ScratchBird-driver` repository first. This runner requires `sb_pg_isql` for PostgreSQL wire-protocol parity. Generic `sb_isql` is native-protocol only and is rejected for this lane.

Once the PostgreSQL CLI is available:

```bash
# Run a single test
sb_pg_isql -h localhost -p 5432 -U postgres -d testdb \
  -f converted/core/select.sql \
  -o results/select.out

# Run all tests
for test in converted/core/*.sql; do
  sb_pg_isql -h localhost -p 5432 -U postgres -d testdb \
    -f "$test" \
    -o "results/$(basename $test .sql).out"
done
```

CTest wrapper (opt-in):

```bash
# Uses config/ctest_list.txt and writes to results/ctest/<timestamp>
SCRATCHBIRD_PG_COMPAT_RUN=1 ctest -R CompatibilityPostgreSQL --test-dir build
```

If `sb_pg_isql` is unavailable, set `SCRATCHBIRD_PG_ISQL` to a valid `sb_pg_isql` path after building FDW CLI wrappers; generic `sb_isql` fallback is intentionally blocked.

## Test Format

PostgreSQL tests are mostly pure SQL with minimal directives:

- Metadata header with test name and original path
- Standard SQL statements
- Comments (-- style)
- Some tests use psql meta-commands (\\d, \\dt, etc.) - these will need special handling

Expected output files are in the `expected/` directory, copied from PostgreSQL's `.out` files.

### Output Variants

Some PostgreSQL tests have multiple expected output files for different platforms or configurations:
- `test.expected` - Default expected output
- `test_1.expected`, `test_2.expected` - Platform or version-specific variants

## Conversion Notes

- PostgreSQL tests are already in SQL format, so conversion is mostly metadata addition
- Original `.out` files are copied to `expected/` directory with `.expected` extension
- Tests are designed to be run in a specific order (some depend on prior test state)
- Some tests use psql-specific features (meta-commands) that need special handling in `sb_pg_isql`

## Test Organization

The 238 tests cover:
- Basic SQL functionality (SELECT, INSERT, UPDATE, DELETE)
- Advanced queries (subqueries, CTEs, window functions)
- Data types (numeric, text, date/time, JSON, XML, arrays, etc.)
- Indexes (B-tree, Hash, GiST, GIN, SP-GiST, BRIN)
- Constraints (PRIMARY KEY, FOREIGN KEY, CHECK, UNIQUE)
- Transactions and concurrency
- Functions and procedures
- Triggers
- Views
- Permissions and security
- Performance features

## Test Manifest

The `config/test_manifest.json` file contains:
- Complete test inventory with metadata
- Category organization
- Test statistics
- File paths

## Sources

- **PostgreSQL Repository:** https://github.com/postgres/postgres
- **PostgreSQL Regression Tests:** https://www.postgresql.org/docs/current/regress.html
- **Conversion Script:** `scripts/convert_pg_test.py`

## Related Documentation

- [Plan 06: Dedicated ISQL Clients](/docs/planning/PLAN_06_DEDICATED_ISQL_CLIENTS.md)
- [Plan 06: Test Automation Design](/docs/planning/PLAN_06_TEST_AUTOMATION_DESIGN.md)
- [SQL Compatibility Test Repositories](/docs/findings/SQL_COMPATIBILITY_TEST_REPOSITORIES.md)

## Notes

PostgreSQL's regression test suite is highly regarded in the database community for its comprehensive coverage. These tests represent decades of PostgreSQL development and edge case discovery.
