# V2 Parser Index Type Completeness Specification

**Document Version:** 1.0
**Date:** 2026-01-07
**Status:** Implementation Required
**Priority:** HIGH - Alpha Blocker

---

## Executive Summary

The V2 parser's CREATE INDEX implementation is **incomplete**. While ScratchBird's storage engine supports 11 production-ready index types, the V2 parser only accepts **5 index types** (BTREE, HASH, GIN, GIST, BRIN). This creates a critical gap where users cannot create 6 index types through V2 SQL syntax:

- **Missing from V2 parser:** SPGIST, RTREE, HNSW, BITMAP, COLUMNSTORE, LSM
- **Missing from bytecode generator:** RTREE, HNSW, COLUMNSTORE, LSM
- **Missing from AST enum:** SPGIST, RTREE, HNSW, BITMAP, COLUMNSTORE, LSM

Additionally, there is **dead code** in the semantic analyzer that references BITMAP index type (line 3210) even though the parser never produces this AST node.

---

## Current State Analysis

### Layer 1: V2 Parser AST (ast_v2.h)

**File:** `include/scratchbird/parser/ast_v2.h`
**Lines:** 486-491

```cpp
enum class IndexType : uint8_t {
    BTREE,
    HASH,
    GIN,
    GIST,
    BRIN,
};
```

**Supported:** 5 index types
**Missing:** SPGIST, RTREE, HNSW, BITMAP, COLUMNSTORE, LSM (6 types)

---

### Layer 2: V2 Parser Implementation (parser_v2.cpp)

**File:** `src/parser/parser_v2.cpp`
**Lines:** 1154-1159

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

**Behavior:** Any index type not in the list triggers a parse error "Unknown index type"
**Gap:** Users cannot specify missing index types even though storage engine supports them

---

### Layer 3: Semantic Analyzer (semantic_analyzer_v2.cpp)

**File:** `src/sblr/semantic_analyzer_v2.cpp`
**Lines:** 3204-3211

```cpp
switch (stmt->index_type) {
    case IndexType::BTREE: resolved->index_method = internString("btree"); break;
    case IndexType::HASH: resolved->index_method = internString("hash"); break;
    case IndexType::GIN: resolved->index_method = internString("gin"); break;
    case IndexType::GIST: resolved->index_method = internString("gist"); break;
    case IndexType::BRIN: resolved->index_method = internString("brin"); break;
    case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;  // ❌ DEAD CODE
}
```

**CRITICAL ISSUE:** Line 3210 contains dead code. BITMAP is not in the AST enum and parser never produces it, so this case is unreachable.

---

### Layer 4: Bytecode Generator (bytecode_generator_v2.cpp)

**File:** `src/sblr/bytecode_generator_v2.cpp`
**Lines:** 766-786

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

**Supported:** 7 index types (BTREE, HASH, GIN, GIST, BRIN, SPGIST, BITMAP)
**Missing:** RTREE, HNSW, COLUMNSTORE, LSM
**Note:** Bytecode generator accepts SPGIST and BITMAP but parser doesn't produce them

---

### Layer 5: SBLR Opcodes (opcodes.h)

**File:** `include/scratchbird/sblr/opcodes.h`
**Lines:** 1412-1425

```cpp
enum class IndexType : uint8_t
{
    BTREE = 0x00,          // B-Tree index - General purpose, sorted data
    HASH = 0x01,           // Hash index - Equality searches only
    GIN = 0x02,            // GIN index - Multi-value columns (arrays, JSONB, text search)
    GIST = 0x03,           // GiST index - Extensible, spatial data, custom types
    SPGIST = 0x04,         // SP-GiST index - Space-partitioned, non-balanced trees
    BRIN = 0x05,           // BRIN index - Block range index, large tables
    RTREE = 0x06,          // R-Tree index - Spatial data, bounding boxes
    HNSW = 0x07,           // HNSW index - Vector similarity search (ANN)
    BITMAP = 0x08,         // Bitmap index - Low cardinality columns
    COLUMNSTORE = 0x09,    // Columnstore index - Column-oriented storage
    LSM = 0x0A,            // LSM-Tree index - Write-optimized, append-heavy workloads
};
```

**Supported:** 11 index types (complete)
**Status:** ✅ Complete opcode definitions

---

### Layer 6: Catalog Manager (catalog_manager.h)

**File:** `include/scratchbird/core/catalog_manager.h`
**Lines:** 543-558

```cpp
enum class IndexType : uint8_t
{
    BTREE = 0,        // B-tree index (default)
    HASH = 1,         // Hash index
    HNSW = 2,         // Vector similarity index (renamed from VECTOR)
    VECTOR = 2,       // Alias for HNSW (backward compatibility)
    FULLTEXT = 3,     // Full-text search index (GIN-based)
    GIN = 4,          // Generalized Inverted Index
    GIST = 5,         // Generalized Search Tree
    BRIN = 6,         // Block Range Index
    RTREE = 7,        // R-tree spatial index
    SPGIST = 8,       // Space-Partitioned GiST
    BITMAP = 9,       // Bitmap index
    COLUMNSTORE = 10, // Columnstore index
    LSM = 11          // LSM-Tree (Log-Structured Merge-Tree)
};
```

**Supported:** 12 distinct index types (BTREE, HASH, HNSW, FULLTEXT, GIN, GIST, BRIN, RTREE, SPGIST, BITMAP, COLUMNSTORE, LSM)
**Status:** ✅ Complete catalog definitions
**Note:** Includes FULLTEXT (GIN-based) not in opcodes enum

---

### Layer 7: Storage Engine (verified via index_cache.cpp)

**File:** `src/sblr/index_cache.cpp`
**Lines:** 274-320

All 11 index types have complete C++ implementations:
- ✅ `core::BTree`
- ✅ `core::HashIndex`
- ✅ `core::GinIndex`
- ✅ `core::GiSTIndex` (incomplete type issues noted)
- ✅ `core::SPGiSTIndex`
- ✅ `core::BrinIndex`
- ✅ `core::RTreeIndex`
- ✅ `core::HnswIndex`
- ✅ `core::BitmapIndex`
- ✅ `core::ColumnstoreIndexSimple`
- ✅ `core::LSMTreeIndex`

**Status:** ✅ All index types have storage implementations

---

## Gap Summary

### Complete Feature Matrix

| Index Type | V2 AST | V2 Parser | Semantic Analyzer | Bytecode Gen | SBLR Opcodes | Catalog | Storage Engine | Status |
|------------|--------|-----------|-------------------|--------------|--------------|---------|----------------|--------|
| **BTREE** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **COMPLETE** |
| **HASH** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **COMPLETE** |
| **GIN** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **COMPLETE** |
| **GIST** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ⚠️ (incomplete) | **MOSTLY COMPLETE** |
| **BRIN** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | **COMPLETE** |
| **SPGIST** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | **PARSER GAP** |
| **RTREE** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | **PARSER+BYTECODE GAP** |
| **HNSW** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | **PARSER+BYTECODE GAP** |
| **BITMAP** | ❌ | ❌ | ⚠️ (dead) | ✅ | ✅ | ✅ | ✅ | **PARSER GAP + DEAD CODE** |
| **COLUMNSTORE** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | **PARSER+BYTECODE GAP** |
| **LSM** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | **PARSER+BYTECODE GAP** |
| **FULLTEXT** | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ (GIN) | **ALIAS/SYNTACTIC SUGAR** |

---

## Critical Issues

### Issue 1: Dead Code in Semantic Analyzer

**File:** `src/sblr/semantic_analyzer_v2.cpp:3210`

```cpp
case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;
```

**Problem:** BITMAP is not in the AST enum (ast_v2.h:486-491), so this case is unreachable.

**Impact:** Maintenance burden, code bloat, misleading for future developers

**Fix:** Either:
1. Add BITMAP to AST enum and parser (recommended)
2. Remove this dead case from semantic analyzer

---

### Issue 2: Parser Rejects Valid Index Types

**Example:**

```sql
-- This FAILS even though storage engine supports it
CREATE INDEX spatial_idx ON locations USING RTREE (geom);
-- Error: "Unknown index type"

-- This FAILS even though storage engine supports it
CREATE INDEX vector_idx ON embeddings USING HNSW (embedding);
-- Error: "Unknown index type"
```

**Impact:** Users cannot create 6 index types that the storage engine fully supports.

---

### Issue 3: Bytecode Generator Accepts Unreachable Inputs

**File:** `src/sblr/bytecode_generator_v2.cpp:782-785`

```cpp
} else if (lower == "spgist") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::SPGIST);
} else if (lower == "bitmap") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BITMAP);
}
```

**Problem:** Bytecode generator accepts "spgist" and "bitmap" strings, but V2 parser never produces them.

**Impact:** Dead code, confusing implementation

---

## Implementation Plan

### Phase 1: Fix Dead Code and Extend AST (1 day)

**Priority:** HIGH
**Files to modify:**
1. `include/scratchbird/parser/ast_v2.h` - Extend IndexType enum
2. `src/sblr/semantic_analyzer_v2.cpp` - Add missing cases

**Changes:**

```cpp
// include/scratchbird/parser/ast_v2.h:486-491
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

```cpp
// src/sblr/semantic_analyzer_v2.cpp:3204-3211
switch (stmt->index_type) {
    case IndexType::BTREE: resolved->index_method = internString("btree"); break;
    case IndexType::HASH: resolved->index_method = internString("hash"); break;
    case IndexType::GIN: resolved->index_method = internString("gin"); break;
    case IndexType::GIST: resolved->index_method = internString("gist"); break;
    case IndexType::BRIN: resolved->index_method = internString("brin"); break;
    case IndexType::SPGIST: resolved->index_method = internString("spgist"); break;    // ✅ ADD
    case IndexType::RTREE: resolved->index_method = internString("rtree"); break;      // ✅ ADD
    case IndexType::HNSW: resolved->index_method = internString("hnsw"); break;        // ✅ ADD
    case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;    // ✅ KEEP (no longer dead)
    case IndexType::COLUMNSTORE: resolved->index_method = internString("columnstore"); break; // ✅ ADD
    case IndexType::LSM: resolved->index_method = internString("lsm"); break;          // ✅ ADD
}
```

---

### Phase 2: Extend V2 Parser (1 day)

**Priority:** HIGH
**Files to modify:**
1. `src/parser/parser_v2.cpp` - Add index type parsing

**Changes:**

```cpp
// src/parser/parser_v2.cpp:1154-1159
if (match(TokenType::KW_USING)) {
    if (matchContextual("BTREE")) stmt->index_type = IndexType::BTREE;
    else if (matchContextual("HASH")) stmt->index_type = IndexType::HASH;
    else if (matchContextual("GIN")) stmt->index_type = IndexType::GIN;
    else if (matchContextual("GIST")) stmt->index_type = IndexType::GIST;
    else if (matchContextual("BRIN")) stmt->index_type = IndexType::BRIN;
    else if (matchContextual("SPGIST")) stmt->index_type = IndexType::SPGIST;          // ✅ ADD
    else if (matchContextual("RTREE")) stmt->index_type = IndexType::RTREE;            // ✅ ADD
    else if (matchContextual("HNSW")) stmt->index_type = IndexType::HNSW;              // ✅ ADD
    else if (matchContextual("BITMAP")) stmt->index_type = IndexType::BITMAP;          // ✅ ADD
    else if (matchContextual("COLUMNSTORE")) stmt->index_type = IndexType::COLUMNSTORE;// ✅ ADD
    else if (matchContextual("LSM")) stmt->index_type = IndexType::LSM;                // ✅ ADD
    else error("Unknown index type");
}
```

---

### Phase 3: Extend Bytecode Generator (1 day)

**Priority:** HIGH
**Files to modify:**
1. `src/sblr/bytecode_generator_v2.cpp` - Add missing index type mappings

**Changes:**

```cpp
// src/sblr/bytecode_generator_v2.cpp:766-786
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
    } else if (lower == "rtree") {                                                      // ✅ ADD
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
    } else if (lower == "hnsw") {                                                       // ✅ ADD
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HNSW);
    } else if (lower == "columnstore") {                                                // ✅ ADD
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::COLUMNSTORE);
    } else if (lower == "lsm") {                                                        // ✅ ADD
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::LSM);
    }
}
```

---

### Phase 4: Comprehensive Testing (2 days)

**Priority:** HIGH
**Test coverage required:**

#### Test File: `tests/integration/test_v2_all_index_types.cpp`

**Test cases:**

1. **CREATE INDEX for each type:**
   ```sql
   CREATE INDEX btree_idx ON test USING BTREE (col1);
   CREATE INDEX hash_idx ON test USING HASH (col2);
   CREATE INDEX gin_idx ON test USING GIN (col3);
   CREATE INDEX gist_idx ON test USING GIST (col4);
   CREATE INDEX brin_idx ON test USING BRIN (col5);
   CREATE INDEX spgist_idx ON test USING SPGIST (col6);
   CREATE INDEX rtree_idx ON test USING RTREE (col7);
   CREATE INDEX hnsw_idx ON test USING HNSW (col8);
   CREATE INDEX bitmap_idx ON test USING BITMAP (col9);
   CREATE INDEX columnstore_idx ON test USING COLUMNSTORE (col10);
   CREATE INDEX lsm_idx ON test USING LSM (col11);
   ```

2. **Verify catalog entries:**
   - Check `pg_indexes` system view
   - Verify index_type stored correctly

3. **Verify index functionality:**
   - Insert data
   - Query using index
   - Verify index is used (check query plan)

4. **DROP INDEX for each type:**
   - Ensure clean removal
   - Verify catalog cleanup

5. **Error cases:**
   - Invalid index type name
   - Index type incompatible with column type

---

### Phase 5: Documentation (1 day)

**Priority:** MEDIUM
**Documents to update:**

1. **User documentation:**
   - `/docs/user/INDEX_TYPES.md` - Document all 11 index types
   - Include use cases for each type
   - Include performance characteristics

2. **Developer documentation:**
   - `/docs/development/INDEX_IMPLEMENTATION.md` - Update with V2 parser completeness
   - Document enum value mappings across layers

3. **Update CRITICAL_FINDINGS.md:**
   - Add V2 parser index type gap to findings

---

## Timeline and Effort

| Phase | Effort | Dependencies | Priority |
|-------|--------|--------------|----------|
| **Phase 1: Fix Dead Code and Extend AST** | 1 day | None | **HIGH** |
| **Phase 2: Extend V2 Parser** | 1 day | Phase 1 | **HIGH** |
| **Phase 3: Extend Bytecode Generator** | 1 day | Phase 1 | **HIGH** |
| **Phase 4: Comprehensive Testing** | 2 days | Phase 2, 3 | **HIGH** |
| **Phase 5: Documentation** | 1 day | Phase 4 | **MEDIUM** |
| **TOTAL** | **6 days** | Sequential | **Alpha Blocker** |

---

## Additional Considerations

### FULLTEXT Index Type

**Status:** FULLTEXT is an alias for GIN-based full-text search

**Recommendation:** Add syntactic sugar in parser:

```cpp
else if (matchContextual("FULLTEXT")) {
    stmt->index_type = IndexType::GIN;
    stmt->fulltext_hint = true;  // Optional flag for optimizer
}
```

**Effort:** +0.5 days (include in Phase 2)

---

### Index Type Validation

**Current behavior:** Parser accepts any index type, executor may fail at runtime

**Recommendation:** Add validation in semantic analyzer to check:
1. Column type compatibility (e.g., HNSW requires VECTOR type)
2. Feature flags (e.g., HNSW requires vector extension enabled)

**Effort:** +1 day (Phase 4)

---

### Backward Compatibility

**Impact:** None - this is purely additive

**Migration:** No migration required, existing indexes continue to work

---

## Success Criteria

Before marking this task complete, verify:

1. ✅ All 11 index types parseable in V2 SQL syntax
2. ✅ Dead code removed from semantic analyzer
3. ✅ All 11 index types map correctly through entire stack
4. ✅ Comprehensive test suite passes
5. ✅ Documentation updated
6. ✅ No regression in existing index functionality

---

## Related Documents

- `/docs/specifications/INDEX_ARCHITECTURE.md` - Complete index type documentation
- `/docs/audit/parsers/CRITICAL_FINDINGS.md` - Parser audit findings
- `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md` - V2/Firebird parity

---

**End of Specification**
**Next Action:** Begin Phase 1 implementation
