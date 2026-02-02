# Testing and Audit

**Purpose:** Documents ScratchBird's testing infrastructure, audit framework, and quality assurance processes.

**Last Updated:** 2026-01-30

---

## Overview

ScratchBird maintains rigorous testing and audit practices to ensure code quality and specification compliance.

```
┌─────────────────────────────────────────────────────────────┐
│                    QUALITY ASSURANCE                         │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐    │
│  │              TESTING PYRAMID                         │    │
│  │                    ▲                                 │    │
│  │                   /│\                                │    │
│  │                  / │ \  E2E Tests                    │    │
│  │                 /  │  \                              │    │
│  │                /   │   \                             │    │
│  │               /────┼────\ Integration Tests          │    │
│  │              /     │     \                           │    │
│  │             /──────┼──────\                          │    │
│  │            /       │       \ Unit Tests              │    │
│  │           └────────┴────────┘                        │    │
│  └─────────────────────────────────────────────────────┘    │
│                          │                                  │
│                          ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              AUDIT FRAMEWORK                         │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Test Organization

### Directory Structure

```
tests/
├── unit/                      # Unit tests (100+ test files)
│   ├── test_parser_v2*.cpp    # Parser tests
│   ├── test_bytecode_*.cpp    # Bytecode generation tests
│   ├── test_btree_*.cpp       # B-tree index tests (compression, GC, MGA, vacuum, etc.)
│   ├── test_bitmap_*.cpp      # Bitmap index tests
│   ├── test_brin_*.cpp        # BRIN index tests
│   ├── test_catalog_*.cpp     # Catalog manager tests
│   ├── test_buffer_*.cpp      # Buffer pool tests
│   ├── test_cache_*.cpp       # Cache tests
│   ├── test_audit_logger.cpp  # Audit logging tests
│   ├── gin/                   # GIN index test suite
│   ├── domains/               # Domain validation tests
│   └── ...
├── integration/               # Integration tests (50+ test files)
│   ├── test_bitmap_dml.cpp    # Bitmap DML integration
│   ├── test_brin_*.cpp        # BRIN DML, GC, MVCC
│   ├── test_columnstore_*.cpp # Columnstore end-to-end tests
│   ├── test_domain_*.cpp      # Domain validation E2E
│   ├── test_concurrent_*.cpp  # Concurrency tests
│   ├── test_cte_basic.cpp     # CTE tests
│   ├── test_check_constraints.cpp
│   └── ...
├── stress/                    # Stress tests
│   ├── test_columnstore_load*.cpp
│   ├── test_lsm_tree_stress.cpp
│   ├── test_multithreaded_stress.cpp
│   └── test_toast_concurrency.cpp
├── compatibility/             # Database compatibility tests
│   ├── README.md
│   └── firebird/
│       └── results/
└── CMakeLists.txt
```

---

## Unit Tests

Unit tests verify individual components in isolation.

### Running Unit Tests

```bash
# Build and run all unit tests
cd build
ctest --output-on-failure -R "^test_"

# Run specific unit test
./tests/unit/test_parser_v2
./tests/unit/test_bytecode_generator_v2
```

### Writing Unit Tests

```cpp
#include <gtest/gtest.h>
#include "scratchbird/parser/parser_v2.h"

TEST(ParserV2, ParseSimpleSelect) {
    ParserV2 parser;
    auto ast = parser.parse("SELECT 1");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, AST_SELECT);
}

TEST(ParserV2, ParseCreateTable) {
    ParserV2 parser;
    auto ast = parser.parse("CREATE TABLE test (id INT PRIMARY KEY)");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, AST_CREATE_TABLE);
}
```

---

## Integration Tests

Integration tests verify component interactions.

### Running Integration Tests

```bash
# Run all integration tests
ctest --output-on-failure -R "^test_integration"

# Run specific integration test
./tests/integration/test_bitmap_dml
./tests/integration/test_concurrent_page_access
```

### Writing Integration Tests

```cpp
#include <gtest/gtest.h>
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/sblr/executor.h"

TEST(BitmapIntegration, InsertAndQuery) {
    // Setup storage engine
    StorageEngine engine("test_db");
    engine.initialize();

    // Create table with bitmap index
    Executor exec(&engine);
    exec.execute("CREATE TABLE test (status INT)");
    exec.execute("CREATE INDEX idx_status ON test USING BITMAP (status)");

    // Insert data
    exec.execute("INSERT INTO test VALUES (1)");
    exec.execute("INSERT INTO test VALUES (2)");

    // Query using bitmap index
    auto result = exec.execute("SELECT * FROM test WHERE status = 1");
    EXPECT_EQ(result.row_count(), 1);

    engine.shutdown();
}
```

---

## Compatibility Tests

Verify behavior matches reference databases.

### Location

```
tests/compatibility/
├── README.md
├── firebird/
│   ├── test_scripts/
│   └── results/
│       └── ctest/
│           └── 20260114_133122/
├── postgresql/
└── mysql/
```

### Running Compatibility Tests

```bash
# Run Firebird compatibility tests
cd tests/compatibility/firebird
./run_compatibility_tests.sh

# Compare results
diff expected/ results/
```

---

## Audit Framework

The audit framework tracks implementation status against specifications.

### Audit Locations

```
docs/audit/
├── languages/           # SQL language feature audits
│   ├── native/
│   │   ├── 01_databases_and_schemas.md
│   │   ├── 02_tables_and_constraints.md
│   │   └── 14_functions.md
│   ├── firebirdsql/
│   ├── postgresql/
│   └── mysql/
├── parsers/             # Parser implementation audits
│   ├── V2/
│   │   └── SUMMARY.md
│   └── CRITICAL_FINDINGS.md
└── README.md
```

### Audit Output Format

Each audit document follows this structure:

```markdown
# Feature Area - Audit

## Implemented
- Feature A: Fully implemented
- Feature B: Implemented with limitations

## Partially Implemented
- Feature C: Parser only, no executor support

## Not Implemented
- Feature D: Planned for Beta

## Critical Findings
- Finding 1: Description and remediation plan
```

---

## Test Requirements

### Before Marking Work Complete

All implementations must satisfy these requirements:

1. **Unit tests** for all new code paths
2. **Integration tests** for component interactions
3. **Restart/persistence tests** for stateful features
4. **Negative/error tests** for error handling

### Completion Verification Checklist

Before marking any task complete, verify against `/COMPLETION_VERIFICATION_CHECKLIST.md`:

- [ ] All tests pass
- [ ] No regressions in existing tests
- [ ] Coverage for new code paths
- [ ] Documentation updated
- [ ] Audit documents updated

---

## Running All Tests

### Full Test Suite

```bash
# Build in Debug mode for best test coverage
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run all tests
ctest --output-on-failure

# Run with verbose output
ctest --output-on-failure -V

# Run specific test pattern
ctest --output-on-failure -R "parser"
```

### Test Categories

```bash
# Unit tests only
ctest -R "^test_unit"

# Integration tests only
ctest -R "^test_integration"

# Storage tests only
ctest -R "storage"

# Parser tests only
ctest -R "parser"
```

---

## CI/CD Integration

### GitHub Actions Workflow

Tests run automatically on:
- Pull request creation
- Push to main branch
- Manual trigger

### Test Matrix

| Platform | Compiler | Build Type |
|----------|----------|------------|
| Ubuntu 22.04 | GCC 11 | Debug |
| Ubuntu 22.04 | GCC 11 | Release |
| Ubuntu 22.04 | Clang 14 | Debug |

---

## Performance Testing

### Stress Tests

```bash
# Run storage stress tests
./tests/unit/test_storage_stress

# Run concurrent access tests
./tests/integration/test_concurrent_page_access
```

### Benchmark Suite

```bash
# Run benchmarks (when available)
./benchmarks/run_all.sh
```

---

## Test Utilities

### Test Database Setup

```cpp
// Helper for setting up test databases
class TestDatabase {
public:
    TestDatabase(const std::string& name) {
        engine_ = std::make_unique<StorageEngine>(name);
        engine_->initialize();
    }

    ~TestDatabase() {
        engine_->shutdown();
        // Clean up test files
    }

    Executor& executor() { return *exec_; }

private:
    std::unique_ptr<StorageEngine> engine_;
    std::unique_ptr<Executor> exec_;
};
```

### Assertions

```cpp
// Custom assertions for ScratchBird testing
#define EXPECT_SBLR_VALID(bytecode) \
    EXPECT_TRUE(validate_sblr(bytecode)) << "Invalid SBLR bytecode"

#define EXPECT_RESULT_ROWS(result, count) \
    EXPECT_EQ(result.row_count(), count)
```

---

## Findings and Remediation

### Critical Findings Location

```
docs/audit/parsers/CRITICAL_FINDINGS.md
```

### Findings Format

```markdown
## Finding ID: F-XXX

**Severity:** Critical | High | Medium | Low

**Description:** What was found

**Impact:** How it affects the system

**Remediation:** Steps to fix

**Status:** Open | In Progress | Resolved
```

---

## Source Code Reference

| Component | Location |
|-----------|----------|
| Test CMake | `tests/CMakeLists.txt` |
| Unit tests | `tests/unit/` |
| Integration tests | `tests/integration/` |
| Compatibility tests | `tests/compatibility/` |
| Audit documents | `docs/audit/` |

---

## Related Documents

- [Core Engine](Core-Engine.md) - What the tests verify
- [Architecture](Architecture.md) - System under test
- [Storage](Storage.md) - Storage layer tests
- [Parsers](Parsers.md) - Parser tests
