# V2 Parser Index Type Completeness Specification

**Document Version:** 1.1
**Date:** January 22, 2026
**Status:** Complete (FULLTEXT/IVF/ZONEMAP supported in V2 parser)
**Priority:** MEDIUM - Alpha polish

---

## Executive Summary

The V2 parser now accepts **14 core index types**: BTREE, HASH, GIN, GIST, SPGIST, BRIN, RTREE, HNSW, BITMAP, COLUMNSTORE, LSM, FULLTEXT, IVF, ZONEMAP. Optional/advanced index types (ZORDER, JSON_PATH, etc.) are intentionally absent from V2.

---

## Current State Analysis

### Layer 1: V2 Parser AST (ast_v2.h)

**File:** `include/scratchbird/parser/ast_v2.h`

```cpp
enum class IndexType : uint8_t {
    BTREE,
    HASH,
    GIN,
    GIST,
    SPGIST,
    BRIN,
    RTREE,
    HNSW,
    BITMAP,
    COLUMNSTORE,
    LSM,
    FULLTEXT,
    IVF,
    ZONEMAP,
};
```

**Supported:** 14 index types
**Missing:** none

---

### Layer 2: V2 Parser Implementation (parser_v2.cpp)

**File:** `src/parser/parser_v2.cpp`

```cpp
if (match(TokenType::KW_USING)) {
    if (matchContextual("BTREE")) stmt->index_type = IndexType::BTREE;
    else if (matchContextual("HASH")) stmt->index_type = IndexType::HASH;
    else if (matchContextual("GIN")) stmt->index_type = IndexType::GIN;
    else if (matchContextual("GIST")) stmt->index_type = IndexType::GIST;
    else if (matchContextual("SPGIST")) stmt->index_type = IndexType::SPGIST;
    else if (matchContextual("BRIN")) stmt->index_type = IndexType::BRIN;
    else if (matchContextual("RTREE")) stmt->index_type = IndexType::RTREE;
    else if (matchContextual("HNSW")) stmt->index_type = IndexType::HNSW;
    else if (matchContextual("BITMAP")) stmt->index_type = IndexType::BITMAP;
    else if (matchContextual("COLUMNSTORE")) stmt->index_type = IndexType::COLUMNSTORE;
    else if (matchContextual("LSM")) stmt->index_type = IndexType::LSM;
    else if (matchContextual("IVF")) stmt->index_type = IndexType::IVF;
    else if (matchContextual("ZONEMAP") || matchContextual("ZONE_MAP"))
        stmt->index_type = IndexType::ZONEMAP;
    else if (matchContextual("FULLTEXT") || matchContextual("INVERTED"))
        stmt->index_type = IndexType::FULLTEXT;
    else error("Unknown index type");
}
```

**Supported:** 14 index types
**Missing:** none (FULLTEXT + INVERTED alias supported)

---

### Layer 3: Semantic Analyzer (semantic_analyzer_v2.cpp)

**File:** `src/sblr/semantic_analyzer_v2.cpp`

```cpp
switch (stmt->index_type) {
    case IndexType::BTREE: resolved->index_method = internString("btree"); break;
    case IndexType::HASH: resolved->index_method = internString("hash"); break;
    case IndexType::GIN: resolved->index_method = internString("gin"); break;
    case IndexType::GIST: resolved->index_method = internString("gist"); break;
    case IndexType::SPGIST: resolved->index_method = internString("spgist"); break;
    case IndexType::BRIN: resolved->index_method = internString("brin"); break;
    case IndexType::RTREE: resolved->index_method = internString("rtree"); break;
    case IndexType::HNSW: resolved->index_method = internString("hnsw"); break;
    case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;
    case IndexType::COLUMNSTORE: resolved->index_method = internString("columnstore"); break;
    case IndexType::LSM: resolved->index_method = internString("lsm"); break;
    case IndexType::FULLTEXT: resolved->index_method = internString("fulltext"); break;
    case IndexType::IVF: resolved->index_method = internString("ivf"); break;
    case IndexType::ZONEMAP: resolved->index_method = internString("zonemap"); break;
}
```

**Supported:** 14 index types
**Missing:** none

---

### Layer 4: Bytecode Generator (bytecode_generator_v2.cpp)

**File:** `src/sblr/bytecode_generator_v2.cpp`

```cpp
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
} else if (lower == "rtree") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
} else if (lower == "hnsw") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HNSW);
} else if (lower == "bitmap") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BITMAP);
} else if (lower == "columnstore") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::COLUMNSTORE);
} else if (lower == "lsm") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::LSM);
} else if (lower == "fulltext" || lower == "inverted") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::FULLTEXT);
} else if (lower == "ivf") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::IVF);
} else if (lower == "zonemap" || lower == "zone_map") {
    index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::ZONEMAP);
}
```

**Supported:** 14 index types
**Missing:** none

---

### Layer 5: SBLR Opcodes (opcodes.h)

**File:** `include/scratchbird/sblr/opcodes.h`

```cpp
enum class IndexType : uint8_t
{
    BTREE = 0x00,
    HASH = 0x01,
    HNSW = 0x02,
    FULLTEXT = 0x03,
    GIN = 0x04,
    GIST = 0x05,
    BRIN = 0x06,
    RTREE = 0x07,
    SPGIST = 0x08,
    BITMAP = 0x09,
    COLUMNSTORE = 0x0A,
    LSM = 0x0B,
};
```

**Supported:** 12 core index types
**Note:** FULLTEXT is defined here but not exposed in V2 AST/parser.

---

### Layer 6: Catalog Manager (catalog_manager.h)

**File:** `include/scratchbird/core/catalog_manager.h`

```cpp
enum class IndexType : uint8_t
{
    BTREE = 0,
    HASH = 1,
    HNSW = 2,
    VECTOR = 2,
    FULLTEXT = 3,
    GIN = 4,
    GIST = 5,
    BRIN = 6,
    RTREE = 7,
    SPGIST = 8,
    BITMAP = 9,
    COLUMNSTORE = 10,
    LSM = 11
};
```

**Supported:** 12 core index types
**Note:** FULLTEXT exists in catalog but is not reachable from V2 SQL today.

---

### Layer 7: Storage Engine

**Core implementations present for all 12 core index types.**

- FULLTEXT uses `InvertedIndex` (DML + search implemented; GC stub in `removeDeadEntries`).
- DML integration for all core types is wired in `src/core/storage_engine.cpp`.

---

## Gap Summary (Core Types)

| Index Type | V2 AST | V2 Parser | Semantic Analyzer | Bytecode Gen | SBLR Opcodes | Catalog | Storage Engine | Status |
|------------|--------|-----------|-------------------|--------------|--------------|---------|----------------|--------|
| BTREE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| HASH | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| GIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| GIST | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| SPGIST | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| BRIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| RTREE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| HNSW | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| BITMAP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| COLUMNSTORE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| LSM | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |
| FULLTEXT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | COMPLETE |

---

## Doc-Only Plan: Add FULLTEXT (INVERTED alias) to V2 Stack

### Phase A: Parser Surface (AST + Parser) - Done

1. **AST update** (`include/scratchbird/parser/ast_v2.h`)
   - Add `FULLTEXT` to `enum class IndexType`.
   - Keep `INVERTED` as a parser alias (not a distinct enum value).

2. **Parser update** (`src/parser/parser_v2.cpp`)
   - Accept `USING FULLTEXT` and `USING INVERTED` in CREATE INDEX.
   - Map both to `IndexType::FULLTEXT`.
   - Keep existing types unchanged.

### Phase B: Semantic + Bytecode Mapping - Done

3. **Semantic analyzer** (`src/sblr/semantic_analyzer_v2.cpp`)
   - Map `IndexType::FULLTEXT` to `index_method = "fulltext"`.

4. **Bytecode generator** (`src/sblr/bytecode_generator_v2.cpp`)
   - Accept both `"fulltext"` and `"inverted"` method strings.
   - Map to `CatalogManager::IndexType::FULLTEXT`.

### Phase C: Tests (V2)

5. **Parser/DDL tests**
   - `CREATE INDEX ... USING FULLTEXT (col)`
   - `CREATE INDEX ... USING INVERTED (col)`
   - Verify catalog stores `FULLTEXT` and index exists.

6. **Execution tests**
   - Insert sample text, query with `@@` operator and confirm index usage.

### Phase D: Documentation Parity

7. **V2 language docs**
   - Add FULLTEXT/INVERTED syntax and constraints (single text column).
   - Clarify that FULLTEXT is backed by the inverted index engine.

---

## Optional (Beta Scope)

The following advanced index types are intentionally out of scope for V2 parser today:
IVF, ZONEMAP, ZORDER, GEOHASH, S2, QUADTREE, OCTREE, FST, SUFFIX_ARRAY, SUFFIX_TREE,
COUNT_MIN_SKETCH, HYPERLOGLOG, ART, LEARNED, LSM_TTL, JSON_PATH.
