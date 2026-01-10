# SQL Compatibility Test Repositories - FirebirdSQL, MySQL, PostgreSQL

**Research Date:** 2025-12-28
**Purpose:** Identify official SQL test repositories for compatibility testing
**Focus:** Recursive CTE/WITH tests and general SQL compatibility

---

## Executive Summary

This document identifies the official test repositories and test suites for FirebirdSQL, MySQL, and PostgreSQL that can be incorporated into ScratchBird's test regime to verify cursor compatibility and SQL compliance.

### Repository Overview

| Database | Repository | Test Count | Test Format | Status |
|----------|-----------|------------|-------------|--------|
| **FirebirdSQL** | fbt-repository | 8,000+ | .fbt (Python) | ✅ Active |
| **FirebirdSQL Legacy** | fbtcs | 390+ tests | C/ESQL/Shell | ⚠️ Deprecated |
| **MySQL** | mysql-server/mysql-test | 1,000s | .test/.result | ✅ Active |
| **PostgreSQL** | postgres/src/test/regress | 200+ files | .sql/.out | ✅ Active |

---

## FirebirdSQL Test Repositories

### Primary Repository: fbt-repository

**Repository:** [github.com/FirebirdSQL/fbt-repository](https://github.com/FirebirdSQL/fbt-repository)

**Description:** Modern Firebird test suite with Python-based framework

**Statistics:**
- **Commits:** 8,019+
- **Test Format:** .fbt files (text files with test definitions)
- **Test Framework:** fbtest (Python-based)
- **Status:** Active development

**Repository Structure:**
```
fbt-repository/
├── tests/
│   ├── functional/        # Functional tests
│   │   ├── basic/
│   │   ├── isql/
│   │   ├── replication/
│   │   └── ...
│   └── bugs/             # Bug regression tests
├── configs/              # Test configurations
└── resources/            # Test resources
```

**Recursive CTE Tests:**
- Tests for recursive EXECUTE STATEMENT (Issue [#3126](https://github.com/FirebirdSQL/firebird/issues/3126))
- Recursive CTE query result tests (Issue [#1791](https://github.com/FirebirdSQL/firebird/issues/1791))
- Recursive CTE error message tests (Issue [#4337](https://github.com/FirebirdSQL/firebird/issues/4337))
- Deep tree recursive CTE tests (Issue [#4776](https://github.com/FirebirdSQL/firebird/issues/4776))

**Key Features:**
- Each test definition stored as single text file with .fbt extension
- Test tracking system via QA ticket numbers
- Comprehensive functional and bug regression coverage
- Focus on Firebird 2.1+ features including recursive CTEs

**References:**
- [Firebird Test Suite Overview](https://www.firebirdsql.org/en/firebird-test-suite)
- [How to Implement New Tests](https://firebirdsql.org/en/how-to-implement-new-tests)
- [Firebird CTE Documentation](https://github.com/FirebirdSQL/firebird/blob/master/doc/sql.extensions/README.common_table_expressions)

### Legacy Repository: fbtcs (Firebird Test Compatibility Suite)

**Repository:** [github.com/FirebirdSQL/fbtcs](https://github.com/FirebirdSQL/fbtcs)

**Description:** Legacy TCS (Test Compatibility Suite) from InterBase/Borland

**Statistics:**
- **Original Tests:** 390 from Borland InterBase
- **Last Updated:** December 13, 2025
- **Language:** C/C++, ESQL, shell scripts
- **Status:** ⚠️ Deprecated (maintained for legacy compatibility)

**Repository Structure:**
```
fbtcs/
├── GTCS/
│   └── tests/           # General Test Compatibility Suite
│       ├── *.script     # Test scripts
│       └── *.output     # Expected outputs
└── DA_LTCS/
    └── tests/           # Database Administration tests
```

**Note:** This test harness is deprecated. All new QA development focuses on fbt-repository and firebird-qa.

**Additional Tools:**
- [firebird-qa](https://github.com/FirebirdSQL/firebird-qa) - Python tools for Firebird QA
- [fbtest](https://github.com/FirebirdSQL/fbtest) - Firebird QA test framework

---

## MySQL Test Repository

### Primary Repository: mysql-server/mysql-test

**Repository:** [github.com/mysql/mysql-server](https://github.com/mysql/mysql-server)

**Test Directory:** `/mysql-test`

**Description:** Official MySQL regression test suite included in MySQL server distribution

**Statistics:**
- **Test Count:** 1,000s of test cases
- **Test Format:** .test files (SQL with directives)
- **Result Format:** .result files (expected output)
- **Status:** Active development

**Repository Structure:**
```
mysql-server/
└── mysql-test/
    ├── README                 # Test suite documentation
    ├── mysql-test-run.pl      # Test runner
    ├── t/                     # Test files (.test)
    │   ├── *.test
    │   └── ...
    ├── r/                     # Result files (.result)
    │   ├── *.result
    │   └── ...
    ├── suite/                 # Test suites
    │   ├── rpl/              # Replication tests
    │   ├── perfschema/       # Performance schema
    │   └── ...
    └── include/              # Test includes/utilities
```

**Recursive CTE Support:**
- **Feature:** WITH RECURSIVE (MySQL 8.0+)
- **Recursion Limit:** Configurable via `cte_max_recursion_depth` (default 1000, max 4294967295)
- **Debug Feature:** LIMIT clause support in recursive CTEs (MySQL 8.0.19+)

**Recursive CTE Test Files (Expected Locations):**
- `mysql-test/t/with_recursive.test`
- `mysql-test/t/cte_recursive.test`
- `mysql-test/suite/*/t/*cte*.test`

**Test Execution:**
```bash
cd mysql-test
./mysql-test-run.pl [test_name]
```

**Key Features:**
- Comprehensive regression test coverage
- SQL with embedded test directives
- Automated comparison of actual vs expected output
- Suite-based organization for different features

**References:**
- [MySQL Test Suite Documentation (8.0)](https://dev.mysql.com/doc/extending-mysql/8.0/en/mysql-test-suite.html)
- [MySQL Test Suite Documentation (8.4)](https://dev.mysql.com/doc/extending-mysql/8.4/en/mysql-test-suite.html)
- [MySQL Test README](https://github.com/mysql/mysql-server/blob/trunk/mysql-test/README)
- [MySQL WITH RECURSIVE Documentation](https://dev.mysql.com/doc/refman/8.0/en/with.html)
- [MySQL Worklog WL#3634](https://dev.mysql.com/worklog/task/?id=3634) - Recursive WITH implementation

**Constraints on Recursive CTEs:**
- Recursive member must NOT contain:
  - Aggregate functions (MAX, MIN, SUM, AVG, COUNT)
  - GROUP BY
  - DISTINCT
  - Window functions
  - Subqueries

---

## PostgreSQL Test Repository

### Primary Repository: postgres/src/test/regress

**Repository:** [github.com/postgres/postgres](https://github.com/postgres/postgres)

**Test Directory:** `/src/test/regress`

**Description:** Comprehensive regression test suite for PostgreSQL

**Statistics:**
- **Test Files:** 200+ SQL test files
- **Test Format:** .sql files (pure SQL)
- **Result Format:** .out files (expected output)
- **Status:** Active development

**Repository Structure:**
```
postgres/
└── src/test/regress/
    ├── pg_regress.c          # Test driver
    ├── sql/                  # SQL test files
    │   ├── with.sql         # WITH/CTE tests (including recursive)
    │   ├── select.sql
    │   ├── join.sql
    │   └── ...
    ├── expected/             # Expected output files
    │   ├── with.out         # Expected output for WITH tests
    │   └── ...
    └── results/             # Actual test output (generated)
        └── ...
```

**Recursive CTE Test File:**

**Primary File:** [with.sql](https://github.com/postgres/postgres/blob/master/src/test/regress/sql/with.sql)

**Expected Output:** [with.out](https://github.com/postgres/postgres/blob/master/src/test/regress/expected/with.out)

**Test Coverage:**
- Recursive CTEs with WITH RECURSIVE syntax
- Re-ordering WITH items to remove forward references
- Data-modifying statements in WITH queries
- CTE optimization and materialization
- Complex hierarchical queries
- Edge cases and error conditions

**Test Execution:**
```bash
cd src/test/regress
make check
# Or run specific test:
./pg_regress with
```

**Test Comparison:**
- Test script runs SQL files
- Uses `diff` to compare actual output (`results/`) vs expected output (`expected/`)
- Reports any differences as failures

**Key Features:**
- Pure SQL test files (no framework directives)
- Comprehensive standard SQL and PostgreSQL extension tests
- Simple diff-based verification
- Well-documented test structure

**References:**
- [PostgreSQL Regression Tests Documentation](https://www.postgresql.org/docs/7.1/regress.html)
- [Regression Test Authoring Wiki](https://wiki.postgresql.org/wiki/Regression_test_authoring)
- [SQL Regression Tests Blog](https://tapoueh.org/blog/2017/08/sql-regression-tests/)
- [PostgreSQL with.sql](https://github.com/postgres/postgres/blob/master/src/test/regress/sql/with.sql)

---

## Recursive CTE Feature Comparison

| Feature | FirebirdSQL | MySQL | PostgreSQL |
|---------|-------------|-------|------------|
| **Syntax** | WITH RECURSIVE | WITH RECURSIVE | WITH RECURSIVE |
| **First Version** | 2.1 | 8.0 | 8.4 |
| **Max Recursion** | 1024 (hard-coded) | 4,294,967,295 (configurable) | Unlimited (stack depth limit) |
| **LIMIT in CTE** | ❌ No | ✅ Yes (8.0.19+) | ✅ Yes |
| **Aggregate in Recursive** | ❌ No | ❌ No | ✅ Yes (limited) |
| **Data-Modifying CTEs** | ❌ No | ❌ No | ✅ Yes (INSERT/UPDATE/DELETE) |

---

## Integration Plan for ScratchBird

### Phase 1: Repository Setup (1-2 hours)

**Action Items:**
1. Vendor test repository snapshots (one-way updates)
2. Create `/tests/compatibility/` directory structure:
   ```
   tests/compatibility/
   ├── firebird/
   │   ├── fbt-repository/     # Vendored snapshot
   │   └── selected_tests/     # Curated test subset
   ├── mysql/
   │   ├── mysql-test/         # Vendored snapshot (sparse checkout via update script)
   │   └── selected_tests/
   └── postgresql/
       ├── regress/            # Vendored snapshot (sparse checkout via update script)
       └── selected_tests/
   ```

3. Document licensing and attribution for each test suite

### Phase 2: Test Format Conversion (20-40 hours)

**Challenges:**
- Each database uses different test formats
- Need conversion to ScratchBird test framework

**Conversion Strategy:**

#### FirebirdSQL (.fbt → ScratchBird)
- **Input:** Python-based .fbt files
- **Output:** C++ GTest cases or SQL script tests
- **Complexity:** Medium (Python parsing required)

#### MySQL (.test → ScratchBird)
- **Input:** .test files with directives
- **Output:** C++ GTest cases or SQL script tests
- **Complexity:** Medium (directive parsing required)

#### PostgreSQL (.sql → ScratchBird)
- **Input:** Pure SQL files
- **Output:** Direct SQL execution tests
- **Complexity:** Low (straightforward SQL)

**Recommended Approach:**
1. **Start with PostgreSQL** (easiest - pure SQL)
2. Parse and convert key recursive CTE tests
3. Create test adapter layer for each database syntax
4. Implement test runner that executes through appropriate parser

### Phase 3: Curated Test Selection (10-20 hours)

**Priority Test Categories:**

#### 1. Recursive CTE Tests (High Priority)
- Basic recursive queries
- Hierarchical data traversal
- Graph traversal
- Fibonacci sequences
- Depth limits and error handling
- **Estimated:** 50-100 tests per database

#### 2. Core SQL Compatibility (Medium Priority)
- SELECT statements with joins
- Aggregate functions
- Window functions
- Subqueries
- **Estimated:** 200-300 tests per database

#### 3. Firebird-Specific Features (High Priority for Firebird Emulation)
- EXECUTE STATEMENT
- RETURNING clause
- UPDATE OR INSERT
- **Estimated:** 100-150 tests

#### 4. PostgreSQL-Specific Features (High Priority for PostgreSQL Emulation)
- Data-modifying CTEs
- LATERAL joins
- JSON/JSONB operations
- **Estimated:** 100-150 tests

#### 5. MySQL-Specific Features (High Priority for MySQL Emulation)
- Optimizer hints
- JSON functions
- Window functions (8.0+)
- **Estimated:** 100-150 tests

### Phase 4: Test Execution Framework (30-50 hours)

**Requirements:**
1. **Multi-Parser Test Runner**
   - Execute same test through V2, Firebird, PostgreSQL, MySQL parsers
   - Compare results for consistency

2. **Result Comparison Engine**
   - Handle different output formats
   - Normalize results for comparison
   - Report differences

3. **Test Categorization**
   - Tag tests by feature (CTE, JOIN, AGGREGATE, etc.)
   - Tag tests by parser compatibility
   - Enable selective test execution

**Implementation:**
```cpp
class CompatibilityTestRunner {
public:
    // Run test through specific parser
    TestResult runTest(const std::string& sql, Parser parser);

    // Run test through all compatible parsers
    MultiParserResult runCrossParser(const std::string& sql);

    // Compare results across parsers
    ComparisonReport compareResults(MultiParserResult results);
};
```

### Phase 5: Continuous Integration (10-15 hours)

**CI/CD Integration:**
1. Add compatibility test suite to CTest
2. Create nightly builds running full compatibility suite
3. Create PR checks running curated subset
4. Generate compatibility reports

**Test Organization:**
```cmake
# CMakeLists.txt for compatibility tests
add_test(NAME compat_postgres_recursive_cte
         COMMAND compat_test_runner
                 --suite=postgresql
                 --test=with_recursive
                 --parser=v2,postgresql)

add_test(NAME compat_firebird_cte
         COMMAND compat_test_runner
                 --suite=firebird
                 --test=recursive_cte
                 --parser=v2,firebird)
```

---

## Effort Estimates

| Phase | Estimated Hours | Priority |
|-------|-----------------|----------|
| Phase 1: Repository Setup | 1-2 | High |
| Phase 2: Test Format Conversion | 20-40 | High |
| Phase 3: Curated Test Selection | 10-20 | High |
| Phase 4: Test Execution Framework | 30-50 | High |
| Phase 5: Continuous Integration | 10-15 | Medium |
| **Total** | **71-127 hours** | - |

**With 3 developers in parallel:** 24-42 hours (3-5 days)

---

## Immediate Next Steps

### Step 1: Refresh Test Repositories (1 hour)

```bash
cd /home/dcalford/CliWork/ScratchBird
./tests/compatibility/scripts/update_test_repos.sh
```

This script performs shallow clones and sparse checkouts, then syncs the
relevant test paths into the vendored `tests/compatibility/*/repos/` trees.

### Step 2: Extract Recursive CTE Tests (2-4 hours)

**PostgreSQL:**
```bash
# Copy with.sql as starting point
cp postgresql/postgres/src/test/regress/sql/with.sql \
   postgresql/selected_tests/with_recursive.sql

# Extract recursive CTE tests only
grep -A 50 "WITH RECURSIVE" with_recursive.sql > postgres_recursive_cte_tests.sql
```

**MySQL:**
```bash
# Find CTE test files
find mysql/mysql-server/mysql-test -name "*cte*" -o -name "*with*"

# Copy relevant tests
cp mysql/mysql-server/mysql-test/t/with_recursive.test \
   mysql/selected_tests/
```

**FirebirdSQL:**
```bash
# Search for recursive CTE tests in fbt-repository
grep -r "WITH RECURSIVE" firebird/fbt-repository/tests/

# Copy relevant .fbt files
find firebird/fbt-repository/tests -name "*recursive*" -o -name "*cte*"
```

### Step 3: Create Test Adapter Prototype (8-12 hours)

**Proof of Concept:**
1. Take 5 recursive CTE tests from PostgreSQL `with.sql`
2. Execute through ScratchBird V2 parser
3. Execute through ScratchBird PostgreSQL parser
4. Compare results
5. Document differences and compatibility gaps

---

## Success Metrics

### Coverage Metrics
- [ ] 50+ recursive CTE tests from each database
- [ ] 200+ core SQL compatibility tests from each database
- [ ] 90%+ pass rate for ScratchBird V2 parser
- [ ] 95%+ compatibility for emulated parsers (Firebird, PostgreSQL, MySQL)

### Quality Metrics
- [ ] All tests documented with source attribution
- [ ] Automated test execution in CI/CD
- [ ] Nightly compatibility reports
- [ ] Regression detection for parser changes

### Documentation Metrics
- [ ] Test coverage matrix (features × parsers)
- [ ] Known compatibility gaps documented
- [ ] Conversion methodology documented
- [ ] Test execution guide for developers

---

## Licensing Considerations

### FirebirdSQL
- **License:** IDPL (InterBase Public License) and IPL (Initial Developer's Public License)
- **Test Suite License:** Mozilla Public License v1.1
- **Attribution Required:** Yes
- **Commercial Use:** Allowed with attribution

### MySQL
- **License:** GPL v2
- **Test Suite License:** GPL v2
- **Attribution Required:** Yes
- **Commercial Use:** Requires GPL compliance or commercial license
- **Note:** Oracle MySQL may have different licensing for tests

### PostgreSQL
- **License:** PostgreSQL License (BSD-like)
- **Test Suite License:** PostgreSQL License
- **Attribution Required:** Yes (minimal)
- **Commercial Use:** Fully allowed with attribution

**Recommendation:** Clearly document test origins and maintain separate attribution file for each database's tests.

---

## References

### FirebirdSQL
- [Firebird Test Suite](https://www.firebirdsql.org/en/firebird-test-suite)
- [fbt-repository on GitHub](https://github.com/FirebirdSQL/fbt-repository)
- [fbtcs on GitHub](https://github.com/FirebirdSQL/fbtcs)
- [Firebird CTE Documentation](https://github.com/FirebirdSQL/firebird/blob/master/doc/sql.extensions/README.common_table_expressions)
- [How to Implement New Tests](https://firebirdsql.org/en/how-to-implement-new-tests)

### MySQL
- [MySQL Server on GitHub](https://github.com/mysql/mysql-server)
- [MySQL Test Suite Documentation 8.0](https://dev.mysql.com/doc/extending-mysql/8.0/en/mysql-test-suite.html)
- [MySQL Test Suite Documentation 8.4](https://dev.mysql.com/doc/extending-mysql/8.4/en/mysql-test-suite.html)
- [MySQL WITH RECURSIVE](https://dev.mysql.com/doc/refman/8.0/en/with.html)
- [MySQL Worklog WL#3634](https://dev.mysql.com/worklog/task/?id=3634)
- [mysql-test README](https://github.com/mysql/mysql-server/blob/trunk/mysql-test/README)

### PostgreSQL
- [PostgreSQL on GitHub](https://github.com/postgres/postgres)
- [PostgreSQL with.sql](https://github.com/postgres/postgres/blob/master/src/test/regress/sql/with.sql)
- [PostgreSQL with.out (expected)](https://github.com/postgres/postgres/blob/master/src/test/regress/expected/with.out)
- [Regression Test Authoring](https://wiki.postgresql.org/wiki/Regression_test_authoring)
- [SQL Regression Tests Blog](https://tapoueh.org/blog/2017/08/sql-regression-tests/)
- [PostgreSQL Regression Tests](https://www.postgresql.org/docs/7.1/regress.html)

---

**Document Created:** 2025-12-28
**Last Updated:** 2025-12-28
**Status:** ✅ Ready for implementation planning
**Next Review:** After Phase 1 completion
