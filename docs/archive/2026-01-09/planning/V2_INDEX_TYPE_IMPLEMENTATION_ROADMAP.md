# V2 Parser Index Type Implementation Roadmap

**Task:** Implement complete index type support in V2 parser (6 missing types)
**Priority:** HIGH - Alpha Blocker
**Total Effort:** 6 days
**Target Completion:** Before Alpha release

---

## Quick Reference

**Missing Index Types:**
1. SPGIST (Space-Partitioned GiST)
2. RTREE (R-Tree spatial index)
3. HNSW (Vector similarity search)
4. BITMAP (Low cardinality columns)
5. COLUMNSTORE (Column-oriented storage)
6. LSM (Log-Structured Merge-Tree)

**Files to Modify:**
1. `include/scratchbird/parser/ast_v2.h` - AST enum
2. `src/parser/parser_v2.cpp` - Parser implementation
3. `src/sblr/semantic_analyzer_v2.cpp` - Semantic analysis
4. `src/sblr/bytecode_generator_v2.cpp` - Bytecode generation
5. `tests/integration/test_v2_all_index_types.cpp` - New test file

---

## Implementation Checklist

### Day 1: AST and Semantic Analyzer

**Goal:** Extend AST enum and fix dead code

#### Task 1.1: Update AST Enum (30 min)

**File:** `include/scratchbird/parser/ast_v2.h:486-491`

```cpp
enum class IndexType : uint8_t {
    BTREE,
    HASH,
    GIN,
    GIST,
    BRIN,
    SPGIST,      // ✅ ADD
    RTREE,       // ✅ ADD
    HNSW,        // ✅ ADD
    BITMAP,      // ✅ ADD
    COLUMNSTORE, // ✅ ADD
    LSM,         // ✅ ADD
};
```

**Verification:**
- [ ] Enum compiles without errors
- [ ] No breaking changes to existing code

---

#### Task 1.2: Update Semantic Analyzer (1 hour)

**File:** `src/sblr/semantic_analyzer_v2.cpp:3204-3211`

**Before (with dead code):**
```cpp
switch (stmt->index_type) {
    case IndexType::BTREE: resolved->index_method = internString("btree"); break;
    case IndexType::HASH: resolved->index_method = internString("hash"); break;
    case IndexType::GIN: resolved->index_method = internString("gin"); break;
    case IndexType::GIST: resolved->index_method = internString("gist"); break;
    case IndexType::BRIN: resolved->index_method = internString("brin"); break;
    case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;  // Was dead code
}
```

**After (complete):**
```cpp
switch (stmt->index_type) {
    case IndexType::BTREE: resolved->index_method = internString("btree"); break;
    case IndexType::HASH: resolved->index_method = internString("hash"); break;
    case IndexType::GIN: resolved->index_method = internString("gin"); break;
    case IndexType::GIST: resolved->index_method = internString("gist"); break;
    case IndexType::BRIN: resolved->index_method = internString("brin"); break;
    case IndexType::SPGIST: resolved->index_method = internString("spgist"); break;
    case IndexType::RTREE: resolved->index_method = internString("rtree"); break;
    case IndexType::HNSW: resolved->index_method = internString("hnsw"); break;
    case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;
    case IndexType::COLUMNSTORE: resolved->index_method = internString("columnstore"); break;
    case IndexType::LSM: resolved->index_method = internString("lsm"); break;
}
```

**Verification:**
- [ ] All enum cases handled
- [ ] No compiler warnings for missing cases
- [ ] Semantic analyzer builds successfully

---

#### Task 1.3: Build and Test (30 min)

```bash
cd /home/dcalford/CliWork/ScratchBird
cmake --build build
```

**Expected:** Clean build with no errors

---

### Day 2: Parser Implementation

**Goal:** Accept all 11 index types in V2 parser

#### Task 2.1: Update Parser (1 hour)

**File:** `src/parser/parser_v2.cpp:1154-1159`

**Before (5 types):**
```cpp
if (match(TokenType::KW_USING)) {
    if (matchContextual("BTREE")) stmt->index_type = IndexType::BTREE;
    else if (matchContextual("HASH")) stmt->index_type = IndexType::HASH;
    else if (matchContextual("GIN")) stmt->index_type = IndexType::GIN;
    else if (matchContextual("GIST")) stmt->index_type = IndexType::GIST;
    else if (matchContextual("BRIN")) stmt->index_type = IndexType::BRIN;
    else error("Unknown index type");
}
```

**After (11 types):**
```cpp
if (match(TokenType::KW_USING)) {
    if (matchContextual("BTREE")) stmt->index_type = IndexType::BTREE;
    else if (matchContextual("HASH")) stmt->index_type = IndexType::HASH;
    else if (matchContextual("GIN")) stmt->index_type = IndexType::GIN;
    else if (matchContextual("GIST")) stmt->index_type = IndexType::GIST;
    else if (matchContextual("BRIN")) stmt->index_type = IndexType::BRIN;
    else if (matchContextual("SPGIST")) stmt->index_type = IndexType::SPGIST;
    else if (matchContextual("RTREE")) stmt->index_type = IndexType::RTREE;
    else if (matchContextual("HNSW")) stmt->index_type = IndexType::HNSW;
    else if (matchContextual("BITMAP")) stmt->index_type = IndexType::BITMAP;
    else if (matchContextual("COLUMNSTORE")) stmt->index_type = IndexType::COLUMNSTORE;
    else if (matchContextual("LSM")) stmt->index_type = IndexType::LSM;
    else error("Unknown index type");
}
```

**Verification:**
- [ ] Parser accepts all 11 index type names
- [ ] Parser still rejects invalid index type names
- [ ] Case-insensitive matching works (BTREE, btree, BTree all accepted)

---

#### Task 2.2: Build and Manual Test (1 hour)

```bash
cd /home/dcalford/CliWork/ScratchBird
cmake --build build

# Test each index type manually
./build/scratchbird_cli << EOF
CREATE TABLE test_indexes (
    id INTEGER,
    name VARCHAR(100),
    geom VARCHAR(100),
    embedding VARCHAR(100),
    status VARCHAR(20)
);

CREATE INDEX btree_idx ON test_indexes USING BTREE (id);
CREATE INDEX hash_idx ON test_indexes USING HASH (name);
CREATE INDEX gin_idx ON test_indexes USING GIN (name);
CREATE INDEX gist_idx ON test_indexes USING GIST (name);
CREATE INDEX brin_idx ON test_indexes USING BRIN (id);
CREATE INDEX spgist_idx ON test_indexes USING SPGIST (name);
CREATE INDEX rtree_idx ON test_indexes USING RTREE (geom);
CREATE INDEX hnsw_idx ON test_indexes USING HNSW (embedding);
CREATE INDEX bitmap_idx ON test_indexes USING BITMAP (status);
CREATE INDEX columnstore_idx ON test_indexes USING COLUMNSTORE (id);
CREATE INDEX lsm_idx ON test_indexes USING LSM (id);
EOF
```

**Expected Results:**
- [ ] All CREATE INDEX statements parse successfully
- [ ] No "Unknown index type" errors
- [ ] Indexes created in catalog (verify with SELECT * FROM pg_indexes;)

---

### Day 3: Bytecode Generator

**Goal:** Emit correct bytecode for all index types

#### Task 3.1: Update Bytecode Generator (1 hour)

**File:** `src/sblr/bytecode_generator_v2.cpp:766-786`

**Before (7 types):**
```cpp
uint8_t index_type = 0xFF;
std::string_view method = getString(stmt->index_method);
if (!method.empty()) {
    std::string lower(method);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lower == "btree") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
    } else if (lower == "hash") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
    } else if (lower == "gin") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIN);
    } else if (lower == "gist") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIST);
    } else if (lower == "brin") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BRIN);
    } else if (lower == "spgist") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::SPGIST);
    } else if (lower == "bitmap") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BITMAP);
    }
}
```

**After (11 types):**
```cpp
uint8_t index_type = 0xFF;
std::string_view method = getString(stmt->index_method);
if (!method.empty()) {
    std::string lower(method);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (lower == "btree") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
    } else if (lower == "hash") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
    } else if (lower == "gin") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIN);
    } else if (lower == "gist") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIST);
    } else if (lower == "brin") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BRIN);
    } else if (lower == "spgist") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::SPGIST);
    } else if (lower == "bitmap") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BITMAP);
    } else if (lower == "rtree") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
    } else if (lower == "hnsw") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HNSW);
    } else if (lower == "columnstore") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::COLUMNSTORE);
    } else if (lower == "lsm") {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::LSM);
    }
}
```

**Verification:**
- [ ] All 11 index types mapped correctly
- [ ] Bytecode emitted matches catalog enum values
- [ ] No default 0xFF value emitted for known types

---

#### Task 3.2: Verify Bytecode Output (1 hour)

Add debug logging to verify bytecode:

```cpp
// After writeByte(index_type) at line 789
#ifdef DEBUG_BYTECODE
std::cerr << "CREATE INDEX: type=" << static_cast<int>(index_type)
          << " (" << method << ")" << std::endl;
#endif
```

**Build and test:**
```bash
cmake --build build -DCMAKE_BUILD_TYPE=Debug
./build/scratchbird_cli < test_all_indexes.sql
```

**Expected output:**
```
CREATE INDEX: type=0 (btree)
CREATE INDEX: type=1 (hash)
CREATE INDEX: type=4 (gin)
CREATE INDEX: type=5 (gist)
CREATE INDEX: type=6 (brin)
CREATE INDEX: type=8 (spgist)
CREATE INDEX: type=7 (rtree)
CREATE INDEX: type=2 (hnsw)
CREATE INDEX: type=9 (bitmap)
CREATE INDEX: type=10 (columnstore)
CREATE INDEX: type=11 (lsm)
```

---

### Days 4-5: Comprehensive Testing

**Goal:** Verify end-to-end functionality for all index types

#### Task 4.1: Create Test File (2 hours)

**File:** `tests/integration/test_v2_all_index_types.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include "scratchbird/core/database.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/executor.h"

using namespace scratchbird;

TEST_CASE("V2 Parser: All 11 Index Types", "[parser][index][v2]") {
    // Initialize database
    core::Database db("test_v2_indexes.sbdb");
    sblr::QueryCompilerV2 compiler(&db);
    sblr::Executor executor(&db);

    // Create test table
    REQUIRE_NOTHROW(compiler.compile(
        "CREATE TABLE test_table ("
        "  id INTEGER, "
        "  name VARCHAR(100), "
        "  data TEXT, "
        "  geom VARCHAR(100), "
        "  embedding VARCHAR(100), "
        "  status VARCHAR(20), "
        "  timestamp BIGINT, "
        "  value DOUBLE PRECISION"
        ")"
    ));

    SECTION("BTREE index") {
        auto result = compiler.compile("CREATE INDEX btree_idx ON test_table USING BTREE (id)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));

        // Verify catalog entry
        auto catalog_result = compiler.compile(
            "SELECT indexname, indextype FROM pg_indexes WHERE indexname = 'btree_idx'"
        );
        REQUIRE(catalog_result.valid());
        // Execute and verify indextype = 0 (BTREE)
    }

    SECTION("HASH index") {
        auto result = compiler.compile("CREATE INDEX hash_idx ON test_table USING HASH (name)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("GIN index") {
        auto result = compiler.compile("CREATE INDEX gin_idx ON test_table USING GIN (data)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("GIST index") {
        auto result = compiler.compile("CREATE INDEX gist_idx ON test_table USING GIST (geom)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("BRIN index") {
        auto result = compiler.compile("CREATE INDEX brin_idx ON test_table USING BRIN (timestamp)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("SPGIST index") {
        auto result = compiler.compile("CREATE INDEX spgist_idx ON test_table USING SPGIST (name)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("RTREE index") {
        auto result = compiler.compile("CREATE INDEX rtree_idx ON test_table USING RTREE (geom)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("HNSW index") {
        auto result = compiler.compile("CREATE INDEX hnsw_idx ON test_table USING HNSW (embedding)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("BITMAP index") {
        auto result = compiler.compile("CREATE INDEX bitmap_idx ON test_table USING BITMAP (status)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("COLUMNSTORE index") {
        auto result = compiler.compile("CREATE INDEX columnstore_idx ON test_table USING COLUMNSTORE (timestamp, value)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("LSM index") {
        auto result = compiler.compile("CREATE INDEX lsm_idx ON test_table USING LSM (id)");
        REQUIRE(result.valid());
        REQUIRE_NOTHROW(executor.execute(result.bytecode()));
    }

    SECTION("Invalid index type") {
        auto result = compiler.compile("CREATE INDEX bad_idx ON test_table USING INVALID (id)");
        REQUIRE_FALSE(result.valid());
        REQUIRE(result.error().find("Unknown index type") != std::string::npos);
    }
}

TEST_CASE("V2 Parser: Index Type Case Insensitivity", "[parser][index][v2]") {
    core::Database db("test_v2_indexes_case.sbdb");
    sblr::QueryCompilerV2 compiler(&db);

    REQUIRE_NOTHROW(compiler.compile("CREATE TABLE test (id INTEGER)"));

    // All of these should succeed
    REQUIRE(compiler.compile("CREATE INDEX idx1 ON test USING BTREE (id)").valid());
    REQUIRE(compiler.compile("CREATE INDEX idx2 ON test USING btree (id)").valid());
    REQUIRE(compiler.compile("CREATE INDEX idx3 ON test USING BTree (id)").valid());
    REQUIRE(compiler.compile("CREATE INDEX idx4 ON test USING RTREE (id)").valid());
    REQUIRE(compiler.compile("CREATE INDEX idx5 ON test USING rtree (id)").valid());
    REQUIRE(compiler.compile("CREATE INDEX idx6 ON test USING RTree (id)").valid());
}
```

**Add to CMakeLists.txt:**
```cmake
add_executable(test_v2_all_index_types tests/integration/test_v2_all_index_types.cpp)
target_link_libraries(test_v2_all_index_types PRIVATE scratchbird Catch2::Catch2)
add_test(NAME test_v2_all_index_types COMMAND test_v2_all_index_types)
```

---

#### Task 4.2: Run Test Suite (2 hours)

```bash
cd /home/dcalford/CliWork/ScratchBird
cmake --build build
cd build
ctest -R test_v2_all_index_types -V
```

**Expected Results:**
- [ ] All 11 index type sections pass
- [ ] Invalid index type section correctly rejects
- [ ] Case insensitivity test passes
- [ ] No segmentation faults or crashes

---

#### Task 4.3: Integration Testing (4 hours)

Test each index type with actual data operations:

```sql
-- 1. BTREE: General purpose
CREATE TABLE users (id INTEGER, name VARCHAR(100));
CREATE INDEX users_id_idx ON users USING BTREE (id);
INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob'), (3, 'Charlie');
SELECT * FROM users WHERE id = 2;  -- Should use index
DROP INDEX users_id_idx;

-- 2. HASH: Equality searches
CREATE INDEX users_name_hash ON users USING HASH (name);
SELECT * FROM users WHERE name = 'Bob';  -- Should use hash index
DROP INDEX users_name_hash;

-- 3. GIN: Multi-value columns
CREATE TABLE documents (id INTEGER, content TEXT);
CREATE INDEX documents_content_gin ON documents USING GIN (content);
-- Insert and search

-- 4. GIST: Spatial data
CREATE TABLE locations (id INTEGER, geom VARCHAR(100));
CREATE INDEX locations_geom_gist ON locations USING GIST (geom);
-- Spatial queries

-- 5. BRIN: Large tables with natural ordering
CREATE TABLE sensor_data (timestamp BIGINT, value DOUBLE PRECISION);
CREATE INDEX sensor_timestamp_brin ON sensor_data USING BRIN (timestamp);
-- Range queries

-- 6. SPGIST: Space-partitioned
CREATE INDEX users_name_spgist ON users USING SPGIST (name);
-- Prefix searches

-- 7. RTREE: Spatial bounding boxes
CREATE INDEX locations_rtree ON locations USING RTREE (geom);
-- Spatial overlap queries

-- 8. HNSW: Vector similarity
CREATE TABLE embeddings (id INTEGER, vector VARCHAR(1000));
CREATE INDEX embeddings_hnsw ON embeddings USING HNSW (vector);
-- Nearest neighbor searches

-- 9. BITMAP: Low cardinality
CREATE TABLE orders (id INTEGER, status VARCHAR(20));
CREATE INDEX orders_status_bitmap ON orders USING BITMAP (status);
-- Status queries

-- 10. COLUMNSTORE: Analytics
CREATE TABLE analytics (timestamp BIGINT, metric VARCHAR(50), value DOUBLE PRECISION);
CREATE INDEX analytics_columnstore ON analytics USING COLUMNSTORE (timestamp, value);
-- Aggregate queries

-- 11. LSM: Write-heavy workloads
CREATE TABLE logs (timestamp BIGINT, message TEXT);
CREATE INDEX logs_lsm ON logs USING LSM (timestamp);
-- Insert-heavy operations
```

**Verification for each:**
- [ ] Index created successfully
- [ ] Data insertions work
- [ ] Queries execute (index used or not is implementation-dependent)
- [ ] Index can be dropped
- [ ] Catalog cleaned up

---

### Day 6: Documentation and Cleanup

**Goal:** Complete documentation and final verification

#### Task 6.1: Update User Documentation (2 hours)

**Create:** `/docs/user/INDEX_TYPES.md`

```markdown
# ScratchBird Index Types

ScratchBird supports 11 production-ready index types optimized for different use cases.

## Index Type Reference

### 1. BTREE (B-Tree Index)
**Use Case:** General-purpose, sorted data
**Best For:** Range queries, ORDER BY, equality searches
**Syntax:**
CREATE INDEX idx_name ON table_name USING BTREE (column);

### 2. HASH (Hash Index)
**Use Case:** Equality searches only
**Best For:** Exact match lookups (WHERE col = value)
**Syntax:**
CREATE INDEX idx_name ON table_name USING HASH (column);

### 3. GIN (Generalized Inverted Index)
**Use Case:** Multi-value columns (arrays, JSONB, text search)
**Best For:** Contains queries, full-text search, array operations
**Syntax:**
CREATE INDEX idx_name ON table_name USING GIN (column);

### 4. GIST (Generalized Search Tree)
**Use Case:** Extensible, spatial data, custom types
**Best For:** Geometric data, custom operators
**Syntax:**
CREATE INDEX idx_name ON table_name USING GIST (column);

### 5. BRIN (Block Range Index)
**Use Case:** Large tables with natural ordering
**Best For:** Timestamp columns, sequential data
**Syntax:**
CREATE INDEX idx_name ON table_name USING BRIN (column);

### 6. SPGIST (Space-Partitioned GiST)
**Use Case:** Space-partitioned, non-balanced trees
**Best For:** Prefix searches, phone numbers, IP addresses
**Syntax:**
CREATE INDEX idx_name ON table_name USING SPGIST (column);

### 7. RTREE (R-Tree Index)
**Use Case:** Spatial data, bounding boxes
**Best For:** Geographic queries, spatial overlaps
**Syntax:**
CREATE INDEX idx_name ON table_name USING RTREE (column);

### 8. HNSW (Hierarchical Navigable Small World)
**Use Case:** Vector similarity search (ANN)
**Best For:** ML embeddings, approximate nearest neighbors
**Syntax:**
CREATE INDEX idx_name ON table_name USING HNSW (column);

### 9. BITMAP (Bitmap Index)
**Use Case:** Low cardinality columns
**Best For:** Status codes, boolean flags, enums
**Syntax:**
CREATE INDEX idx_name ON table_name USING BITMAP (column);

### 10. COLUMNSTORE (Column-Oriented Index)
**Use Case:** Analytics, OLAP queries
**Best For:** Aggregates, column-heavy scans
**Syntax:**
CREATE INDEX idx_name ON table_name USING COLUMNSTORE (col1, col2, ...);

### 11. LSM (Log-Structured Merge-Tree)
**Use Case:** Write-optimized, append-heavy workloads
**Best For:** Time-series data, logging, event streams
**Syntax:**
CREATE INDEX idx_name ON table_name USING LSM (column);

## Performance Characteristics

| Index Type | Insert | Lookup | Range Scan | Space | Best Use Case |
|------------|--------|--------|------------|-------|---------------|
| BTREE | Medium | Fast | Fast | Medium | General purpose |
| HASH | Fast | Fastest | N/A | Small | Exact matches |
| GIN | Slow | Fast | Medium | Large | Multi-value data |
| GIST | Medium | Medium | Fast | Medium | Spatial data |
| BRIN | Fastest | Medium | Fast | Smallest | Large sequential tables |
| SPGIST | Medium | Fast | Fast | Medium | Prefix searches |
| RTREE | Medium | Fast | Fast | Medium | Spatial bounding boxes |
| HNSW | Slow | Fastest* | N/A | Large | Vector similarity |
| BITMAP | Medium | Fast | Fast | Small** | Low cardinality |
| COLUMNSTORE | Medium | Medium | Fastest | Medium | Analytics |
| LSM | Fastest | Fast | Fast | Medium | Write-heavy workloads |

*Approximate nearest neighbor
**For low cardinality columns

## Examples

See `/docs/examples/index_usage_examples.sql` for comprehensive usage examples.
```

---

#### Task 6.2: Update Developer Documentation (1 hour)

**Update:** `/docs/development/INDEX_IMPLEMENTATION.md`

Add section:
```markdown
## V2 Parser Index Type Support (Complete as of 2026-01-07)

The V2 parser now supports all 11 production-ready index types:

### AST Enum
File: `include/scratchbird/parser/ast_v2.h:486-491`
All 11 types defined in IndexType enum.

### Parser Implementation
File: `src/parser/parser_v2.cpp:1154-1159`
All 11 types accepted in USING clause.

### Semantic Analysis
File: `src/sblr/semantic_analyzer_v2.cpp:3204-3211`
All 11 types mapped to index method strings.

### Bytecode Generation
File: `src/sblr/bytecode_generator_v2.cpp:766-786`
All 11 types mapped to catalog enum values.

### Enum Value Mapping
| AST Enum | Catalog Enum | SBLR Opcode |
|----------|--------------|-------------|
| BTREE | 0 | 0x00 |
| HASH | 1 | 0x01 |
| GIN | 4 | 0x02 |
| GIST | 5 | 0x03 |
| BRIN | 6 | 0x05 |
| SPGIST | 8 | 0x04 |
| RTREE | 7 | 0x06 |
| HNSW | 2 | 0x07 |
| BITMAP | 9 | 0x08 |
| COLUMNSTORE | 10 | 0x09 |
| LSM | 11 | 0x0A |
```

---

#### Task 6.3: Final Verification (2 hours)

**Checklist:**
- [ ] All code changes committed
- [ ] No compiler warnings
- [ ] All tests passing
- [ ] Documentation complete and accurate
- [ ] CRITICAL_FINDINGS.md updated
- [ ] No dead code remaining
- [ ] Performance regression tests pass

**Run full test suite:**
```bash
cd /home/dcalford/CliWork/ScratchBird/build
ctest -j$(nproc)
```

**Expected:**
- [ ] 100% test pass rate
- [ ] No segmentation faults
- [ ] No memory leaks (run with valgrind if available)

---

## Success Criteria

Before marking this task complete, ALL of the following must be true:

1. ✅ AST enum contains all 11 index types
2. ✅ V2 parser accepts all 11 index types
3. ✅ Semantic analyzer handles all 11 index types
4. ✅ Bytecode generator emits correct types for all 11
5. ✅ Dead code in semantic analyzer removed
6. ✅ Test suite covers all 11 index types
7. ✅ All tests passing
8. ✅ User documentation complete
9. ✅ Developer documentation updated
10. ✅ CRITICAL_FINDINGS.md reflects completion

---

## Rollback Plan

If issues arise during implementation:

1. **AST changes breaking compilation:**
   - Revert `ast_v2.h` changes
   - Keep only first 5 enum values

2. **Parser changes causing failures:**
   - Revert `parser_v2.cpp` changes
   - Restore original 5-type implementation

3. **Bytecode issues:**
   - Revert `bytecode_generator_v2.cpp` changes
   - Use fallback to default index type

4. **Test failures:**
   - Disable new test file
   - Fix issues incrementally
   - Re-enable tests one by one

---

## Post-Implementation Tasks

After successful completion:

1. **Performance benchmarking:**
   - Compare index creation performance
   - Benchmark query performance with each index type
   - Document performance characteristics

2. **Additional testing:**
   - Stress testing with large datasets
   - Concurrent index creation
   - Index rebuild after crashes

3. **Future enhancements:**
   - Add index type hints to query planner
   - Implement FULLTEXT syntactic sugar
   - Add index usage statistics

---

**End of Roadmap**
**Status:** Ready for Implementation
**Next Step:** Begin Day 1 tasks
