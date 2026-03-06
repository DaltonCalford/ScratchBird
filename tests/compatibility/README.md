# ScratchBird Compatibility Testing

This directory contains comprehensive SQL compatibility tests from three major database systems, converted for use with ScratchBird's multi-database emulation capability.

## Overview

**Total Tests:** 11,905 SQL test files
**Databases:** Firebird, MySQL, PostgreSQL
**Purpose:** Validate ScratchBird's database emulation compatibility
**Parser Core:** `v3` (canonical)
**Source Policy:** Firebird, MySQL, and PostgreSQL suites are vendored snapshots in `tests/compatibility/*/repos/` and updated one-way into ScratchBird via `scripts/update_test_repos.sh`. Snapshot provenance is written to `tests/compatibility/SNAPSHOT_MANIFEST.md`.

## Parser Boundary (Authoritative)

- `v3` is the only canonical SQL parser surface in ScratchBird.
- PostgreSQL/MySQL/Firebird compatibility lanes are **emulation surfaces only** (adapter/protocol behavior over the same core semantics pipeline).
- Native ScratchBird lane is the direct proof lane for core parser behavior.

Every compatibility run now writes these files under its run directory:

- `RUN_MANIFEST.json` (engine/protocol/parser_core/parser_mode, list file, listed tests, run status)
- `PARSER_BOUNDARY.txt` (human-readable parser boundary marker)

`parser_mode` values:

- `native_core`: native ScratchBird lane (`tests/compatibility/scratchbird/results/ctest/<run_id>/`)
- `emulation_surface_only`: PostgreSQL/MySQL/Firebird lanes (`tests/compatibility/<engine>/results/ctest/<run_id>/`)

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
├── scratchbird/                 # Native ScratchBird v3 tests
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
│   ├── generate_ctest_lists.py          # Generate expanded/full ctest lists + summary
│   ├── generate_test_manifests.py       # Generate test catalogs
│   ├── run_required_upstream_harnesses.sh # Upstream harness launcher (plan/execute)
│   └── update_test_repos.sh             # Refresh vendored test repositories
│
├── CTEST_LIST_SUMMARY.md        # Per-engine curated/expanded/full counts + runtime estimates
├── SNAPSHOT_MANIFEST.md         # Source remotes/commits for current vendored snapshots
│
├── results/                     # Test execution results
│   └── conversion_report_*.txt          # Conversion reports
│
└── README.md                    # This file
```

## Quick Start

### 1. Refresh Test Repositories (Vendored Snapshots)

Refresh the latest tests into the vendored tree:

```bash
./scripts/update_test_repos.sh
```

Run from `tests/compatibility/` (or use `./tests/compatibility/scripts/update_test_repos.sh` from the repo root).

`update_test_repos.sh` supports three source modes:

- `auto` (default): use local reference clones when available, otherwise clone from remotes.
- `local`: copy from local reference clones (default root: `~/CliWork`).
- `remote`: always clone from remotes.

Examples:

```bash
# Force local reference clones from ~/CliWork
./scripts/update_test_repos.sh --source local --local-root ~/CliWork

# In local mode, fetch/pull local references first (fast-forward only when clean)
SCRATCHBIRD_SYNC_LOCAL_REFS=1 ./scripts/update_test_repos.sh --source local

# Force remote refresh
./scripts/update_test_repos.sh --source remote
```

Every refresh writes `tests/compatibility/SNAPSHOT_MANIFEST.md` with source remotes, commits, and file counts so users can see exactly what snapshot is being run.

Compatibility CTest timeout window can be increased at configure time:

```bash
cmake -S . -B build -DSCRATCHBIRD_TEST_TIMEOUT_COMPATIBILITY=43200
```

Default compatibility timeout is `21600` seconds (6h). Set `0` to fall back to integration timeout.
Global CTest timeout default is also `21600` seconds and can be overridden with `-DSCRATCHBIRD_CTEST_TIMEOUT=<seconds>`.
For the emulation evidence gate command-level timeout, configure `-DSCRATCHBIRD_EMULATION_GATE_COMMAND_TIMEOUT=<seconds>` (default `7200`).

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

### 4. Generate Expanded/Full CTest Lists

```bash
./scripts/generate_ctest_lists.py
```

This writes:

- `tests/compatibility/<engine>/config/ctest_list_expanded.txt`
- `tests/compatibility/<engine>/config/ctest_list_full.txt`
- `tests/compatibility/CTEST_LIST_SUMMARY.md`

### 5. Run Tests (CTest)

Once the dedicated ISQL clients are built (see [Plan 06](/docs/planning/PLAN_06_DEDICATED_ISQL_CLIENTS.md)):

```bash
# Firebird compatibility subset (CTest)
ctest -R CompatibilityFirebird --test-dir build

# PostgreSQL compatibility subset (CTest)
ctest -R CompatibilityPostgreSQL --test-dir build

# MySQL compatibility subset (CTest)
ctest -R CompatibilityMySQL --test-dir build

# Upstream emulation evidence bundle (gate + wire capture)
ctest -R CompatibilityEmulationEvidence --test-dir build

# Expanded compatibility lane selection (all engines)
SCRATCHBIRD_COMPAT_CTEST_LIST_MODE=expanded ctest -R "Compatibility(Firebird|MySQL|PostgreSQL)" --test-dir build

# Full compatibility lane selection (all engines)
SCRATCHBIRD_COMPAT_CTEST_LIST_MODE=full ctest -R "Compatibility(Firebird|MySQL|PostgreSQL)" --test-dir build
```

You can also set per-engine modes:

- `SCRATCHBIRD_FB_CTEST_LIST_MODE=curated|expanded|full`
- `SCRATCHBIRD_MY_CTEST_LIST_MODE=curated|expanded|full`
- `SCRATCHBIRD_PG_CTEST_LIST_MODE=curated|expanded|full`

### Unified Example Database Harness

Compatibility lanes can run against a single seeded example database managed by:

```bash
# Dynamic CTest fixture database (recreated each run)
./scripts/example_db_manager.sh dynamic-setup
ctest -R "CompatibilityExampleDbSetup|CompatibilityScratchBirdNative|CompatibilityPostgreSQL|CompatibilityMySQL" --test-dir build
./scripts/example_db_manager.sh dynamic-teardown

# Static shared database for GUI/driver groups
./scripts/example_db_manager.sh static-up
./scripts/example_db_manager.sh static-status
./scripts/example_db_manager.sh static-refresh
./scripts/example_db_manager.sh static-down
```

The bootstrap and post-bootstrap seed SQL are at:

- `tests/compatibility/scratchbird/example_sql/00_bootstrap_seed.sql`
- `tests/compatibility/scratchbird/example_sql/01_post_bootstrap_seed.sql`

Cross-engine default auth identities for harness runs are seeded into:

- `compat_identity_user_map_contract` (canonical identity + per-engine login aliases/auth method/policy/profile metadata; contract fixture)

When the unified example harness runs, the generated profile files also export:

- `SCRATCHBIRD_EXAMPLE_COMPAT_*` variables for canonical + per-engine alias credentials.

Standardized development-window credential defaults are documented in:

- `docs/DEFAULT_TEST_ENGINE_CREDENTIALS.md`

Current limitation:

- Alias principals are represented in the contract table for deterministic test assertions; runtime auth resolution is still finalized independently.

Native chain scripts (including deterministic test-data inserts) are listed in:

- `tests/compatibility/scratchbird/config/example_ctest_list.txt`

### 5. Run Full Emulation Verification Bundle

To regenerate the standardized gate and wire-capture evidence from in-tree suites:

```bash
./scripts/verify_required_emulation_tests.sh
```

This writes reports under `tests/compatibility/results/emulation/`.

To generate or execute the required upstream harness command set for EPFC-025/026/027:

```bash
# command templates only (default)
./scripts/run_required_upstream_harnesses.sh plan

# execute MySQL upstream MTR + PostgreSQL upstream pg_regress + Firebird upstream gate commands
./scripts/run_required_upstream_harnesses.sh execute
```

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

Compatibility lanes require protocol-accurate clients (`sb_my_isql`, `sb_pg_isql`, and Firebird-compatible `isql-fb`/`sb_fb_isql`).
Generic `sb_isql` is intentionally rejected for emulated protocol parity runs.

Result separation by lane is mandatory:

- Native core proof: `tests/compatibility/scratchbird/results/ctest/<run_id>/`
- PostgreSQL emulation proof: `tests/compatibility/postgresql/results/ctest/<run_id>/`
- MySQL emulation proof: `tests/compatibility/mysql/results/ctest/<run_id>/`
- Firebird emulation proof: `tests/compatibility/firebird/results/ctest/<run_id>/`

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
