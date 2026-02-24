# ScratchBird Compatibility Testing

This directory contains comprehensive SQL compatibility tests from three major database systems, converted for use with ScratchBird's multi-database emulation capability.

## Overview

**Total Tests:** 11,905 SQL test files
**Databases:** Firebird, MySQL, PostgreSQL
**Purpose:** Validate ScratchBird's database emulation compatibility
**Source Policy:** Firebird, MySQL, and PostgreSQL suites are vendored snapshots in `tests/compatibility/*/repos/` and updated one-way into ScratchBird via `scripts/update_test_repos.sh`.

## Statistics

| Database   | Tests | Categories | Original Format | Repository |
|------------|-------|------------|-----------------|------------|
| **Firebird**   | 2,826 | 39 | .fbt | [fbt-repository](https://github.com/FirebirdSQL/fbt-repository) |
| **MySQL**      | 8,841 | 59 | .test | [mysql-server](https://github.com/mysql/mysql-server) |
| **PostgreSQL** |   238 |  1 | .sql | [postgres](https://github.com/postgres/postgres) |
| **TOTAL**      | **11,905** | **99** | | |

## Directory Structure

```
tests/compatibility/
├── firebird/                    # Firebird compatibility tests
│   ├── repos/                  # Original test repositories (vendored snapshots)
│   │   ├── fbt-repository/
│   │   └── firebird-qa/
│   ├── converted/              # 2,826 converted SQL tests
│   ├── expected/               # Expected output files
│   ├── scripts/                # Conversion scripts
│   ├── config/                 # Test manifests and configuration
│   └── README.md
│
├── mysql/                       # MySQL compatibility tests
│   ├── repos/                  # Vendored snapshot
│   ├── converted/              # 8,841 converted SQL tests
│   ├── expected/               # Expected output files
│   ├── scripts/                # Conversion scripts
│   ├── config/                 # Test manifests and configuration
│   └── README.md
│
├── postgresql/                  # PostgreSQL compatibility tests
│   ├── repos/                  # Original test repository (vendored snapshot)
│   ├── converted/              # 238 converted SQL tests
│   ├── expected/               # Expected output files
│   ├── scripts/                # Conversion scripts
│   ├── config/                 # Test manifests and configuration
│   └── README.md
│
├── scratchbird/                 # Native ScratchBird V2 tests
│   ├── tests/                  # Native test files
│   ├── scripts/                # Test runners
│   └── config/                 # Configuration
│
├── common/                      # Shared test infrastructure
│   ├── lib/                    # Shared libraries (test runners, parsers)
│   └── templates/              # Report templates
│
├── scripts/                     # Global test scripts
│   ├── convert_all_tests_parallel.sh    # Convert all tests
│   ├── generate_test_manifests.py       # Generate test catalogs
│   └── update_test_repos.sh             # Refresh vendored test repositories
│
├── results/                     # Test execution results
│   └── conversion_report_*.txt          # Conversion reports
│
└── README.md                    # This file
```

## Quick Start

### 1. Refresh Test Repositories (Vendored Snapshots)

Refresh the latest tests from official repositories:

```bash
./scripts/update_test_repos.sh
```

Run from `tests/compatibility/` (or use `./tests/compatibility/scripts/update_test_repos.sh` from the repo root). The script performs shallow clones and sparse checkout to refresh the vendored snapshots.

To skip MySQL refresh for a smaller update pass:

```bash
SCRATCHBIRD_FETCH_MYSQL_TESTS=0 ./scripts/update_test_repos.sh
# or
./scripts/update_test_repos.sh --skip-mysql
```

### 2. Convert Tests to SQL

Convert all tests from original formats to SQL:

```bash
./scripts/convert_all_tests_parallel.sh
```

This will convert ~4,000 original test files to 11,905 SQL test files (some tests generate multiple SQL files for different database versions).

### 3. Generate Test Manifests

Create test catalogs:

```bash
./scripts/generate_test_manifests.py
```

### 4. Run Tests (CTest)

Once the dedicated ISQL clients are built (see [Plan 06](/docs/planning/PLAN_06_DEDICATED_ISQL_CLIENTS.md)):

```bash
# Firebird compatibility subset (CTest)
ctest -R CompatibilityFirebird --test-dir build

# PostgreSQL compatibility subset (CTest, opt-in)
SCRATCHBIRD_PG_COMPAT_RUN=1 ctest -R CompatibilityPostgreSQL --test-dir build

# MySQL compatibility subset (CTest, opt-in)
SCRATCHBIRD_MY_COMPAT_RUN=1 ctest -R CompatibilityMySQL --test-dir build
```

### 5. Run Full Emulation Verification Bundle

To regenerate the standardized gate and wire-capture evidence from in-tree suites:

```bash
./scripts/verify_required_emulation_tests.sh
```

This writes reports under `tests/compatibility/results/emulation/`.

## Test Conversion

### Firebird (.fbt → .sql)

Firebird tests use a Python-based .fbt format containing:
- Test metadata (ID, title, description)
- Multiple version variants (Firebird 2.5, 3.0, etc.)
- Init scripts (DDL setup)
- Test scripts (SQL to execute)
- Expected output (stdout/stderr)

Converter: `firebird/scripts/convert_fbt_to_sql.py`

### MySQL (.test → .sql)

MySQL tests use a custom .test format with:
- SQL statements
- Test directives (--echo, --error, etc.)
- Expected results in separate .result files

Converter: `mysql/scripts/convert_mysql_test.py`

### PostgreSQL (.sql → .sql)

PostgreSQL tests are already SQL, conversion adds:
- Metadata headers
- Standardized file organization
- Expected output from .out files

Converter: `postgresql/scripts/convert_pg_test.py`

## Test Execution

Tests are executed using CLI clients built from the `ScratchBird-driver` repository:

- **sb_fb_isql** - Firebird protocol client (port 3050)
- **sb_my_isql** - MySQL protocol client (port 3306)
- **sb_pg_isql** - PostgreSQL protocol client (port 5432)
- **sb_isql** - ScratchBird native client (port 3092)

When wrapper clients are not built, compatibility scripts fall back to `sb_isql`.

## Test Manifests

Each database has a `config/test_manifest.json` file containing:
- Complete test inventory
- Test metadata (IDs, titles, descriptions)
- Category organization
- File paths
- Statistics

Example:
```json
{
  "database": "firebird",
  "generated": "2025-12-29T17:20:03",
  "version": "1.0",
  "categories": {
    "bugs": {
      "path": "bugs",
      "test_count": 1234,
      "tests": ["core_0001", "core_0002", ...]
    }
  },
  "tests": [...],
  "statistics": {
    "total_tests": 2826,
    "total_categories": 39
  }
}
```

## Repository Management

Test repositories are vendored snapshots (no git submodules). Refresh them with:

```bash
./scripts/update_test_repos.sh
```

Updates are one-way into ScratchBird; do not push to upstream repositories. Avoid adding `.git` metadata under `repos/`.

The update script uses shallow clones and sparse checkout to minimize disk usage and rsyncs the snapshot into `repos/`:
- **Firebird:** `fbt-repository` and `firebird-qa`
- **MySQL:** `mysql-test/` directory from `mysql-server`
- **PostgreSQL:** `src/test/regress/` directory from `postgres`

Large upstream artifacts are kept for fidelity and regression coverage (for example, `tests/compatibility/mysql/repos/mysql-server/mysql-test/std_data/bug36444172/dump.sql`).

## Conversion Statistics

Last conversion run (2025-12-29):

```
Firebird:    2,826 tests converted (21 failed)
MySQL:       8,841 tests converted (0 failed)
PostgreSQL:    238 tests converted (0 failed)

Total:      11,905 tests converted
Success:    99.8%
```

Failed Firebird tests (21 files):
- Most failures due to malformed .fbt files or unsupported Python syntax
- See `results/conversion_report_*.txt` for details

## Related Documentation

- [Plan 06: Dedicated ISQL Clients](/docs/planning/PLAN_06_DEDICATED_ISQL_CLIENTS.md) - ISQL client implementation specs
- [Plan 06: Test Automation Design](/docs/planning/PLAN_06_TEST_AUTOMATION_DESIGN.md) - Test automation infrastructure
- [SQL Compatibility Test Repositories](/docs/findings/SQL_COMPATIBILITY_TEST_REPOSITORIES.md) - Repository analysis
- [Dedicated ISQL Clients Requirement](/docs/findings/DEDICATED_ISQL_CLIENTS_REQUIREMENT.md) - Architecture rationale

## Next Steps

1. **Build ISQL Clients** - Implement sb_fb_isql, sb_my_isql, sb_pg_isql (see Plan 06)
2. **Create Test Runners** - Implement automated test execution framework
3. **Set Up CI/CD** - Integrate compatibility tests into continuous integration
4. **Generate Compatibility Reports** - Track pass rates by database and category
5. **Develop ScratchBird V2 Tests** - Create native test suite for ScratchBird features

## Contributing

When adding new tests:

1. Refresh upstream tests with `scripts/update_test_repos.sh` (or add new tests under `repos/` without `.git` metadata)
2. Run conversion scripts to generate SQL files
3. Update test manifests
4. Document new test categories in database README

## License

Tests are from official database repositories and retain their original licenses:
- Firebird tests: Firebird Public License
- MySQL tests: GNU General Public License
- PostgreSQL tests: PostgreSQL License

See individual repository licenses for details.

---

**Last Updated:** 2026-02-22
**Test Count:** 11,905
**Conversion Success Rate:** 99.8%
