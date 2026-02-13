# ALPHA-003 Implementation Plan: Advanced Index Types

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date:** October 13, 2025
**Status:** In Progress
**Target:** Implement all missing index types for complete index support

---

## Overview

ScratchBird currently has B-Tree and Hash indexes implemented. ALPHA-003 will add 5 additional index types:

1. **GIN (Generalized Inverted Index)** - HIGHEST PRIORITY
2. **Bitmap Index**
3. **GIST (Generalized Search Tree)**
4. **BRIN (Block Range Index)**
5. **VECTOR Index (HNSW)**

**Strategy:** Implement GIN first (highest priority, enables ARRAY/JSONB/full-text), then proceed with other types.

---

## Phase 1: GIN Index - Core Structure (GIN-1)

**Estimated:** 1-2 days (original: 6-8 weeks)
**Priority:** HIGHEST

### Goals
- GIN entry tree (B-tree mapping terms → posting lists)
- Basic posting list structure
- Entry tree page format
- Core insertion/search operations

### Deliverables
1. `include/scratchbird/core/gin_index.h` - GIN index interface
2. `src/core/gin_index.cpp` - Entry tree implementation
3. Entry page format (GinEntryPage structure)
4. Basic insert() and search() operations
5. Tests for entry tree operations

### Key Concepts
- **Entry Tree**: B-tree mapping terms/keys to posting lists
- **Posting List**: List of TIDs where a term appears
- **Entry Tuple**: (key, posting_list_pointer)

---

## Phase 2: GIN Index - Posting Trees (GIN-2)

**Estimated:** 1 day
**Priority:** HIGH

### Goals
- Posting tree structure for large posting lists
- Posting tree page format
- Posting tree insertion/search

### Deliverables
1. Posting tree page structures
2. Posting tree B-tree operations
3. Conversion from array to posting tree when list grows
4. Tests for posting trees

### Key Concepts
- **Posting Tree**: B-tree of TIDs for a single term (when list is large)
- **Threshold**: When to convert from array to tree (e.g., >64 TIDs)

---

## Phase 3: GIN Index - Pending List (GIN-3)

**Estimated:** 1 day
**Priority:** MEDIUM

### Goals
- Pending list for fast inserts
- Background cleanup mechanism
- Merge pending list into main index

### Deliverables
1. Pending list structure (unsorted list of entries)
2. Fast insertion to pending list
3. Background cleanup process
4. Tests for pending list operations

### Key Concepts
- **Pending List**: Unsorted list of new entries for fast insertion
- **Cleanup**: Periodic merge of pending list into main index
- **fastupdate** option: Enable/disable pending list

---

## Phase 4: GIN Index - Advanced Features (GIN-4)

**Estimated:** 1 day
**Priority:** MEDIUM

### Goals
- Partial match support
- Multi-key support (for arrays)
- GIN operator support
- Statistics and maintenance

### Deliverables
1. Partial match operations
2. Array element extraction
3. GIN-specific operators (@>, &&, etc.)
4. Vacuum support
5. Comprehensive test suite

---

## Phase 5: Bitmap Index (BITMAP-1)

**Estimated:** 1-2 days (original: 3-4 weeks)
**Priority:** MEDIUM

### Goals
- Bitmap structure for low-cardinality columns
- Bitmap compression (e.g., WAH compression)
- Bitmap operations (AND, OR, NOT)

### Deliverables
1. `include/scratchbird/core/bitmap_index.h`
2. `src/core/bitmap_index.cpp`
3. Bitmap page format
4. Compression algorithms
5. Set operations
6. Tests

### Key Concepts
- **Bitmap**: One bit per row for each distinct value
- **Compression**: Word-Aligned Hybrid (WAH) or Run-Length Encoding
- **Low Cardinality**: Best for columns with few distinct values (e.g., gender, status)

---

## Phase 6: BRIN Index (BRIN-1)

**Estimated:** 1 day (original: 2-3 weeks)
**Priority:** MEDIUM

### Goals
- Block range index for very large tables
- Min/max ranges per block range
- Minimal storage overhead

### Deliverables
1. `include/scratchbird/core/brin_index.h`
2. `src/core/brin_index.cpp`
3. Range summary structure
4. Range-based search
5. Tests

### Key Concepts
- **Block Range**: Group of consecutive pages (e.g., 128 pages)
- **Summary**: Min/max values for each range
- **Use Case**: Very large tables with correlated data (e.g., time-series)

---

## Phase 7: GIST Index (GIST-1)

**Estimated:** 2-3 days (original: 6-8 weeks)
**Priority:** MEDIUM

### Goals
- Generalized Search Tree framework
- Extensibility API for custom types
- Basic geometric operations

### Deliverables
1. `include/scratchbird/core/gist_index.h`
2. `src/core/gist_index.cpp`
3. GIST extensibility API
4. Basic operator classes
5. Tests

### Key Concepts
- **Extensibility**: Framework for custom index types
- **Operator Classes**: Predicates, penalties, unions, etc.
- **Use Cases**: Spatial data, range types, full-text

---

## Phase 8: VECTOR Index (VECTOR-1)

**Estimated:** 2 days (original: 4-6 weeks)
**Priority:** MEDIUM

### Goals
- HNSW (Hierarchical Navigable Small World) index
- Approximate nearest neighbor search
- Distance functions (L2, cosine, etc.)

### Deliverables
1. `include/scratchbird/core/vector_index.h`
2. `src/core/vector_index.cpp`
3. HNSW graph structure
4. kNN search operations
5. Tests

### Key Concepts
- **HNSW**: Multi-layer graph for ANN search
- **kNN**: k-nearest neighbor queries
- **Distance Metrics**: L2 (Euclidean), cosine similarity, inner product

---

## Implementation Notes

### Common Infrastructure

All index types share:
- Index catalog integration (already exists)
- Page allocation (already exists)
- Buffer pool integration (already exists)
- Lock manager integration (already exists)

### Testing Strategy

For each index type:
1. **Unit tests**: Core operations (insert, search, delete)
2. **Integration tests**: With storage engine and catalog
3. **Performance tests**: Compare with existing indexes
4. **Stress tests**: Large datasets, concurrent access

### Phased Delivery

- **Week 1**: GIN Phases 1-4 (entry tree, posting trees, pending list, advanced features)
- **Week 2**: Bitmap (Phase 5) + BRIN (Phase 6)
- **Week 3**: GIST (Phase 7) + VECTOR (Phase 8)

**Total Estimated Time:** 3 weeks (vs 21-29 weeks original estimate)

---

## Success Criteria

For each index type:
- ✅ Core data structures implemented
- ✅ Insert, search, delete operations working
- ✅ Catalog integration complete
- ✅ Test suite passing (10+ tests per type)
- ✅ Documentation complete
- ✅ Performance benchmarks available

---

## Dependencies

- **ALPHA-001** (Type System): ARRAY, JSONB types needed for GIN full functionality
- **Buffer Pool**: Already implemented ✓
- **Catalog Manager**: Already implemented ✓
- **Lock Manager**: Already implemented ✓

---

## Notes

- GIN is highest priority because it enables ARRAY indexing, JSONB indexing, and full-text search
- Implementation focuses on core functionality first, optimizations later
- Following same rapid implementation approach as ALPHA-002
- Each phase is designed to be completable in 1-2 days

---

**Let's start with GIN Phase 1: Entry Tree!**
