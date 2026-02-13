# ScratchBird Index Architecture


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version:** 1.0
**Date:** November 20, 2025
**Status:** Production Documentation

---

## Contradictions vs Normative Index Specs (2026-02-07)

- This document previously stated inconsistent index counts. The authoritative count is 28, and all index types are core.
- Authoritative algorithm sections now live in per-index specs (e.g., `BTREE_SPEC.md`, `HASH_SPEC.md`, `HNSW_SPEC.md`, etc.). If any detail conflicts, those per-index specs override this architecture doc.

---

## Table of Contents

1. [Overview](#overview)
2. [Index Type Reference](#index-type-reference)
3. [MGA Compliance Patterns](#mga-compliance-patterns)
4. [DML Integration Architecture](#dml-integration-architecture)
5. [Choosing the Right Index](#choosing-the-right-index)
6. [Performance Characteristics](#performance-characteristics)
7. [Implementation Status](#implementation-status)

---

## Overview

ScratchBird implements **28 core index types** designed for diverse workloads ranging from OLTP to OLAP. All indexes follow **Firebird Multi-Generational Architecture (MGA)** principles with TIP-based visibility and stable record UUIDs.

### Design Principles

1. **MGA Compliance:** Index entries reference record versions (creator `rhd_transaction`, back-version chains)
2. **Stable Record Identity:** Record UUIDs are canonical; page/slot addresses are cache hints only
3. **Logical Deletion:** Deletes create `RHD_DELETED` record versions; old index entries persist until sweep
4. **TIP Visibility:** Visibility uses TIP state checks and MGA rules; no PostgreSQL-style snapshots

### Architecture Components

```
┌─────────────────────────────────────────────────────┐
│ SQL Layer (Parser + Query Planner)                 │
│  - Index selection via cost estimation              │
│  - Index hints (USING INDEX clause)                │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ SBLR Bytecode Layer                                 │
│  - CREATE/DROP INDEX opcodes                        │
│  - Index maintenance opcodes (INSERT/UPDATE/DELETE) │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ Index Factory & Storage Engine                      │
│  - Index type dispatch                              │
│  - DML integration hooks                            │
│  - Garbage collection interface                     │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ Index Implementations (28 core index types)        │
│  - B-Tree, Hash, GIN, Fulltext, HNSW, GiST         │
│  - SP-GiST, BRIN, Bitmap, LSM, R-Tree              │
│  - Columnstore, IVF, ZoneMap, Z-Order, Geohash, S2 │
│  - Quadtree, Octree, FST, Suffix, CMS, HLL         │
│  - ART, Learned, LSM-TTL, JSON_PATH                │
└─────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────┐
│ Storage Layer (Buffer Pool + Page Manager)         │
│  - Page-level storage                               │
│  - Optional write-after log integration (optional extension) │
│  - TIP-based visibility                             │
└─────────────────────────────────────────────────────┘
```

---

## Index Type Reference

### Canonical Index Type Names (Parser/Catalog)

**All Core Index Types:**
- BTREE, HASH, FULLTEXT, GIN, GIST, SPGIST, BRIN
- BITMAP, RTREE, HNSW, COLUMNSTORE, LSM
- IVF, ZONEMAP, ZORDER, GEOHASH, S2
- QUADTREE, OCTREE, FST, SUFFIX_ARRAY, SUFFIX_TREE
- COUNT_MIN_SKETCH, HYPERLOGLOG, ART, LEARNED, LSM_TTL, JSON_PATH

**DDL Note:** SQL uses lowercase keywords in `USING` clauses (e.g., `USING zorder`). Bloom filters are an index option (`WITH (bloom_filter = true)`), not a standalone index type.

**Total Index Types:** 28 (all core)

### 1. B-Tree Index

**Purpose:** General-purpose sorted index for range queries and equality lookups

**Use Cases:**
- Primary keys and unique constraints
- Range queries (`WHERE age BETWEEN 18 AND 65`)
- Sorted retrieval (`ORDER BY last_name`)
- Prefix matching (`WHERE name LIKE 'Smith%'`)

**Data Structure:**
- Balanced tree with configurable branching factor
- Leaf pages linked for fast sequential scans
- Prefix/suffix compression to reduce space
- Multi-level structure (typical: 3-4 levels for millions of rows)

**File:** `src/core/btree.cpp` (2,836 lines)

**Key Features:**
- ✅ Full MGA compliance with record-version metadata
- ✅ TIP-based visibility checks in search
- ✅ Logical deletion (creates `RHD_DELETED` record versions)
- ✅ Active DML integration (maintained during INSERT/UPDATE/DELETE)
- ✅ Key compression (prefix and suffix)
- ✅ Support for NULL values and infinity keys

**API Example:**
```cpp
// Search for key
std::vector<UUID> results;
Status status = btree->search(key, snapshot, tm, &results, &ctx);

// Insert key-value pair
status = btree->insert(key, value, record_uuid, &ctx);

// Logical delete
status = btree->remove(key, record_uuid, &ctx);
```

**Performance:**
- Search: O(log N)
- Insert: O(log N) + page split cost
- Range Scan: O(log N + K) where K = result size
- Space: ~1.2-1.5x data size (with compression)

---

### 2. Hash Index

**Purpose:** Fast equality lookups using hash-based lookup

**Use Cases:**
- Exact match queries (`WHERE user_id = 12345`)
- High-cardinality columns (UUIDs, email addresses)
- Cache lookups and fast equality checks
- Join conditions on equality predicates

**Data Structure:**
- Extendible hashing with directory and bucket pages
- Dynamic growth via bucket splitting
- Overflow chaining for hash collisions
- MurmurHash3 hash function

**File:** `src/core/hash_index.cpp`

**Key Features:**
- ✅ Full MGA compliance with record-version metadata
- ✅ TIP-based visibility during bucket scan
- ✅ Logical deletion (creates `RHD_DELETED` record versions)
- ✅ Active DML integration
- ✅ Dynamic resizing (global depth: 4-20)
- ✅ Overflow chain management

**API Example:**
```cpp
// Search for exact key
std::vector<UUID> results;
Status status = hash_index->search(key, snapshot, tm, &results, &ctx);

// Insert key
status = hash_index->insert(key, record_uuid, &ctx);

// Remove key
status = hash_index->remove(key, record_uuid, &ctx);
```

**Performance:**
- Search: O(1) average, O(N) worst case (rare)
- Insert: O(1) amortized
- Range Scan: Not supported (hash indexes don't maintain order)
- Space: ~1.1-1.3x data size

**Limitations:**
- Cannot perform range queries
- Cannot support ORDER BY
- Hash collisions may degrade performance

---

### 3. GIN Index (Generalized Inverted Index)

**Purpose:** Indexing composite values (arrays, JSON, full-text)

**Use Cases:**
- Array containment (`WHERE tags @> ARRAY['postgresql', 'database']`)
- JSON key/value search (`WHERE metadata @> '{"status": "active"}'`)
- Full-text search (with tsvector)
- Multi-value column indexing

**Data Structure:**
- Entry tree (B-tree) mapping keys → posting lists
- Posting lists: Compressed record UUID arrays for small sets
- Posting trees: B-trees for large posting sets (>64 entries)
- Pending list: Fast-path buffer for recent inserts

**File:** `src/core/gin_index.cpp`

**Key Features:**
- ✅ Key extraction framework (default, array, custom extractors)
- ✅ Posting tree and posting list support
- ✅ Pending list for fast inserts
- ⚠️ **MGA VIOLATION:** Physical removal instead of versioned delete (Line 241)
- ❌ No DML integration (returns NOT_IMPLEMENTED)

**API Example:**
```cpp
// Register custom key extractor
GINIndex::registerExtractor("custom_type", my_extractor);

// Search for keys
std::vector<UUID> results;
status = gin_index->search(keys, snapshot, tm, &results, &ctx);

// Insert with key extraction
status = gin_index->insert(value, record_uuid, &ctx);
```

**Performance:**
- Search: O(K × log N) where K = number of keys
- Insert: O(K × log N) + pending list flush cost
- Space: 1.5-3x data size (depends on key cardinality)

**Known Issues:**
- ⚠️ Posting list entries use physical removal (should use MGA versioned delete)
- Requires remediation for full MGA compliance (see TASK-CRITICAL-1)

---

### 4. HNSW Index (Hierarchical Navigable Small World)

**Purpose:** Approximate nearest neighbor search for high-dimensional vectors

**Use Cases:**
- Semantic search (text embeddings from OpenAI, Cohere, etc.)
- Image similarity search
- Recommendation systems
- Anomaly detection
- Document clustering

**Data Structure:**
- Multi-layer graph (typical: 4-6 layers)
- Bi-directional links between nodes
- Entry point at top layer
- Layer 0 (base) contains all vectors
- Higher layers provide coarse navigation

**File:** `src/core/hnsw_index.cpp`

**Key Features:**
- ✅ Full MGA compliance with record-version metadata on nodes
- ✅ TIP-based visibility during graph traversal
- ✅ Logical deletion (creates deleted record versions)
- ❌ No DML integration (returns NOT_IMPLEMENTED)
- ✅ Configurable M (max connections per node, default: 16)
- ✅ Configurable ef_construction (build quality, default: 200)
- ✅ Multiple distance metrics (Euclidean, Cosine, Dot Product)

**API Example:**
```cpp
// K-NN search
std::vector<std::pair<UUID, float>> results;
status = hnsw_index->search(query_vector, k, ef_search, snapshot, tm, &results, &ctx);

// Insert vector
status = hnsw_index->insert(vector, record_uuid, &ctx);

// Logical delete
status = hnsw_index->remove(record_uuid, &ctx);
```

**Performance:**
- Search: O(log N) average for approximate search
- Insert: O(M × log N)
- Recall@10: 95%+ for production workloads
- Space: ~1.5-2x data size (depends on M parameter)

**Tuning Parameters:**
- `M`: Higher = better recall, more memory (8-64)
- `ef_construction`: Higher = better quality, slower build (100-400)
- `ef_search`: Higher = better recall, slower search (10-200)

---

### 5. GiST Index (Generalized Search Tree)

**Purpose:** Extensible tree structure for custom predicates

**Use Cases:**
- Geometric data (points, rectangles, polygons)
- Custom data types with containment predicates
- Nearest neighbor queries
- Range queries on non-standard types

**Data Structure:**
- R-tree-like structure with pluggable predicates
- Internal nodes: Predicate summaries
- Leaf nodes: Actual indexed values
- Customizable via callback functions (consistent, penalty, picksplit)

**File:** `src/core/gist_index.cpp`

**Key Features:**
- ✅ Full MGA compliance with record-version metadata
- ✅ TIP-based visibility checks
- ✅ Logical deletion
- ❌ No DML integration
- ✅ Pluggable predicate callbacks
- ✅ Support for custom data types

**API Example:**
```cpp
// Search with predicate
std::vector<UUID> results;
status = gist_index->search(predicate, snapshot, tm, &results, &ctx);

// Insert with predicate
status = gist_index->insert(predicate, value, record_uuid, &ctx);
```

**Performance:**
- Search: O(log N) for well-balanced predicates
- Insert: O(log N) + split cost
- Space: 1.3-2x data size (depends on predicate complexity)

---

### 6. SP-GiST Index (Space-Partitioned GiST)

**Purpose:** Space-partitioning trees for non-overlapping regions

**Use Cases:**
- Quadtree/octree indexing
- 2D/3D spatial data
- Phone number prefix trees
- IP address range lookups

**Data Structure:**
- Space-partitioning tree (quadrants for 2D points)
- Non-overlapping partitions (unlike GiST)
- Unbalanced tree (depth varies by data distribution)
- Choice function determines partitioning strategy

**File:** `src/core/spgist_index.cpp`

**Key Features:**
- ✅ Full MGA compliance
- ✅ TIP-based visibility
- ✅ Logical deletion
- ❌ No DML integration
- ✅ Quadrant-based partitioning for 2D
- ✅ Custom partitioning strategies

**API Example:**
```cpp
// Search in spatial region
std::vector<UUID> results;
status = spgist_index->search(region, snapshot, tm, &results, &ctx);

// Insert spatial value
status = spgist_index->insert(point, record_uuid, &ctx);
```

**Performance:**
- Search: O(log N) for balanced partitions
- Insert: O(log N)
- Space: 1.2-1.5x data size

---

### 7. BRIN Index (Block Range Index)

**Purpose:** Lossy index for very large tables with physical correlation

**Use Cases:**
- Time-series data (insertion order = time order)
- Log tables with monotonic timestamps
- Append-only tables
- Large fact tables in data warehouses

**Data Structure:**
- Block range summaries (min/max per range)
- Typically 128-512 pages per range
- Small index size (~0.01% of table size)
- Lossy: May return false positives (requires heap verification)

**File:** `src/core/brin_index.cpp`

**Key Features:**
- ✅ Full MGA compliance
- ✅ TIP-based visibility
- ❌ No DML integration
- ✅ Min/max range tracking
- ✅ Automatic range summarization

**API Example:**
```cpp
// Range query (returns candidate blocks)
std::vector<uint32_t> candidate_blocks;
status = brin_index->search(min_val, max_val, snapshot, tm, &candidate_blocks, &ctx);

// Update block range summary
status = brin_index->updateRange(block_num, min_val, max_val, &ctx);
```

**Performance:**
- Search: O(num_ranges) - very fast for large tables
- Insert: O(1) amortized
- Space: 0.01-0.1% of table size (extremely compact)

**Best Practices:**
- Use only on tables with physical correlation (e.g., timestamps)
- Avoid on randomly distributed data
- Set pages_per_range based on table size and selectivity

---

### 8. Bitmap Index

**Purpose:** Bitmap-based indexing for low-cardinality columns

**Use Cases:**
- Low-cardinality columns (gender, status, country)
- Data warehouse star schema dimensions
- Boolean flags
- Multi-condition queries with AND/OR operations

**Data Structure:**
- One bitmap per distinct value
- Bit-per-row representation
- Efficient AND/OR/NOT operations
- Compressed bitmaps (RLE encoding)

**File:** `src/core/bitmap_index.cpp`

**Key Features:**
- ⚠️ **INCOMPLETE:** Insert/remove operations are stubbed
- ⚠️ **MGA INCOMPLETE:** No record-version metadata on bitmap entries
- ❌ No DML integration
- ✅ Bitmap AND/OR operations implemented
- ✅ Heap scan integration

**API Example:**
```cpp
// Search returns bitmap (not record UUIDs directly)
Bitmap* result;
status = bitmap_index->search(value, &result, &ctx);

// Combine bitmaps
Bitmap* combined = Bitmap::AND(bitmap1, bitmap2);
```

**Performance:**
- Search: O(1) for single value
- AND/OR: O(N / 64) for bitmap operations
- Space: Varies (0.1-10x depending on cardinality)

**Known Issues:**
- ⚠️ Insert/remove are stubbed (TASK-CRITICAL-2)
- Requires completion before production use

---

### 9. LSM-Tree Index (Log-Structured Merge Tree)

**Purpose:** Write-optimized index for high-throughput inserts

**Use Cases:**
- High-volume write workloads
- Time-series databases
- Event logging systems
- Metrics and monitoring data

**Data Structure:**
- Memtable (in-memory Red-Black tree)
- Immutable SSTables (Sorted String Tables) on disk
- Multiple levels with size-tiered compaction
- Write-after log (WAL, optional optional extension) for durability

**File:** `src/core/lsm_tree_index.cpp`

**Key Features:**
- ✅ Full MGA compliance with record-version metadata in entries
- ✅ TIP-based visibility
- ✅ Logical deletion (tombstones)
- ❌ No DML integration
- ✅ Background compaction
- ✅ Memtable flushing

**API Example:**
```cpp
// Insert (always fast - goes to memtable)
status = lsm_index->put(key, value, current_xid, &ctx);

// Search (merges memtable + SSTables)
std::vector<uint8_t> value;
bool found;
status = lsm_index->get(key, current_xid, &value, &found, &ctx);

// Range scan
std::vector<std::pair<Key, Value>> entries;
status = lsm_index->scan(start_key, end_key, snapshot, tm, &entries, &ctx);
```

**Performance:**
- Insert: O(log N_mem) - very fast (in-memory)
- Search: O(log N_mem + L × log N_sst) where L = number of levels
- Range Scan: Efficient for sorted order
- Space: 1.3-2x data size (compaction reduces over time)

**Compaction Strategy:**
- Size-tiered: Merge SSTables of similar size
- Triggered when level reaches threshold
- Background thread (non-blocking)

---

### 10. R-Tree Index

**Purpose:** Spatial indexing for multi-dimensional rectangles

**Use Cases:**
- Geographic Information Systems (GIS)
- Bounding box queries (map tiles)
- Spatial joins
- Nearest neighbor searches

**Data Structure:**
- Tree of Minimum Bounding Rectangles (MBRs)
- Internal nodes: MBR unions of children
- Leaf nodes: Actual rectangles with record UUIDs
- Quadratic split algorithm

**Files:**
- Wrapper: `src/core/rtree_index.cpp`
- Implementation: `src/core/rtree.cpp`

**Key Features:**
- ✅ MGA compliance (assumed, needs audit - TASK-AUDIT-1)
- ❌ No DML integration
- ✅ MBR-based spatial queries
- ✅ Overlap and containment predicates

**API Example:**
```cpp
// Search for overlapping rectangles
std::vector<UUID> results;
status = rtree_index->search(query_rect, snapshot, tm, &results, &ctx);

// Insert rectangle
status = rtree_index->insert(rect, record_uuid, &ctx);
```

**Performance:**
- Search: O(log N) average
- Insert: O(log N) + split cost
- Space: 1.5-2x data size

**Note:** Underlying `rtree.cpp` implementation requires MGA compliance audit.

---

### 11. Columnstore Index

**Purpose:** Columnar storage for analytical workloads

**Use Cases:**
- OLAP queries (aggregations over columns)
- Data warehouse fact tables
- Wide tables with selective column access
- Compression-friendly workloads

**Data Structure:**
- Column-oriented storage (not row-oriented)
- Column segments with compression
- RLE and dictionary compression
- Min/max statistics per segment

**File:** `src/core/columnstore_index.cpp`

**Key Features:**
- ✅ Full MGA compliance with TIP integration
- ✅ Schema integration (supports all 86 data types)
- ❌ No DML integration
- ✅ RLE compression
- ✅ Dictionary compression
- ✅ Multi-page segments (no size limits)
- ✅ Disk persistence for scans

**API Example:**
```cpp
// Insert column data
status = columnstore->insertColumn(column_id, row_count, column_data, &ctx);

// Scan column range
std::vector<uint8_t> data;
status = columnstore->scanColumn(column_id, start_row, end_row, &data, &ctx);
```

**Performance:**
- Column Scan: O(N_rows / compression_ratio)
- Insert: O(N_rows) - append-only
- Space: 0.3-0.7x data size (with compression)

**Compression:**
- RLE: 50-90% reduction for sorted/repeated values
- Dictionary: 50-70% reduction for strings
- Auto-selects best compression per segment

---

### 12. Fulltext (Inverted) Index

**Purpose:** Full-text search and token-based retrieval

**Use Cases:**
- Full-text search (match terms, phrases, proximity)
- Tokenized text columns
- Fuzzy/prefix dictionary integration via FST (optional)

**Data Structure:**
- Term dictionary + posting lists
- Segment-based immutable postings
- Compression for doc IDs and positions

**Spec:** `docs/specifications/parser/v3/indexes/InvertedIndex.md`

**Key Features:**
- ✅ Core inverted index structures and scoring
- ✅ DML integration via storage engine (FULLTEXT index type)
- ❌ Index GC pending (`InvertedIndex::removeDeadEntries`)

---

### Advanced Core Index Types

These index types are defined and ready for implementation and are part of the core index scope:

- **IVF** (vector similarity) - `docs/specifications/parser/v3/indexes/IVFIndex.md`
- **ZONEMAP** (min/max pruning) - `docs/specifications/parser/v3/indexes/ZoneMapsIndex.md`
- **ZORDER** (Morton) - `docs/specifications/parser/v3/indexes/ZOrderIndex.md`
- **GEOHASH**, **S2** - `docs/specifications/parser/v3/indexes/GeohashS2Index.md`
- **QUADTREE**, **OCTREE** - `docs/specifications/parser/v3/indexes/QuadtreeOctreeIndex.md`
- **FST** - `docs/specifications/parser/v3/indexes/FSTIndex.md`
- **SUFFIX_ARRAY**, **SUFFIX_TREE** - `docs/specifications/parser/v3/indexes/SuffixIndex.md`
- **COUNT_MIN_SKETCH** - `docs/specifications/parser/v3/indexes/CountMinSketchIndex.md`
- **HYPERLOGLOG** - `docs/specifications/parser/v3/indexes/HyperLogLogIndex.md`
- **ART** - `docs/specifications/parser/v3/indexes/AdaptiveRadixTreeIndex.md`
- **LEARNED** - `docs/specifications/parser/v3/indexes/LearnedIndex.md`
- **LSM_TTL** - `docs/specifications/parser/v3/indexes/LSMTimeSeriesIndex.md`
- **JSON_PATH** - `docs/specifications/parser/v3/indexes/JSONPathIndex.md`

---

## MGA Compliance Patterns

All ScratchBird indexes follow Firebird MGA semantics with UUID-based identity. These rules are mandatory.

### 1. Required Index Entry Metadata


**Logical Fields:**

- `record_uuid` (UUID): Stable record UUID (logical identity)
- `record_ptr` (SBRecordPtr): Physical locator (page, slot), cache hint
- `record_txn` (uint64_t): rhd_transaction (creator of this version)
- `record_flags` (uint32_t): RHD_DELETED, RHD_CHAIN, etc.
- `back_version_uuid` (UUID): Optional: rhd_back_version for validation


**Rules:**
- `record_uuid` is authoritative. `record_ptr` may be stale and must be validated.
- `record_txn` and `record_flags` mirror the on-page `SBRecordHeader`.
- Index entries are **per record version**. Old entries remain until sweep.

### 2. TIP-Based Visibility (Record-Centric)

Indexes do not implement PostgreSQL snapshot visibility. They must validate visibility against the record header and TIP:

```cpp
bool index_entry_visible(const SBIndexEntryMeta& meta,
                         const SBTransactionSnapshot* snap,
                         SBTransactionManager* tm)
{
    const SBRecordHeader* rhd = resolve_record_header(meta.record_uuid, meta.record_ptr);
    if (rhd == NULL) return false;

    if (!record_uuid_matches(rhd, meta.record_uuid)) {
        rhd = resolve_record_header_by_uuid(meta.record_uuid);
        if (rhd == NULL) return false;
    }

    const SBRecordHeader* visible = sb_find_visible_version(rhd, snap, tm);
    return visible != NULL && (visible->rhd_flags & RHD_DELETED) == 0;
}
```

**Index-only scans** must still validate the record header (or a validated version cache).

### 3. Insert / Update / Delete Semantics

- **Insert:** create record version; insert index entries for that version.
- **Update (non-key):** no index change.
- **Update (key change):** insert new entries for new version; old entries remain.
- **Delete:** create `RHD_DELETED` version; old index entries remain until sweep.

### 4. Garbage Collection (Index Sweep)

Index GC removes entries **only** when the referenced record version is dead:
- `record_txn` is COMMITTED and `record_txn < OIT`, and
- record version is deleted or superseded, and
- no active transaction can see it.

---

## DML Integration Architecture

Indexes must be maintained during INSERT, UPDATE, and DELETE operations.

### DML Hook Locations

All DML hooks are in `src/core/storage_engine.cpp` and use `IndexKeyExtractor`
plus centralized dispatch helpers (`insertIntoIndex`, `removeFromIndex`).
Columnstore is handled via a row-buffered insert path.

```cpp
// INSERT hook (storage_engine.cpp)
Status StorageEngine::insertTuple(...) {
    // ... insert into heap ...

    // Build column layout + extract index key (with detoast)
    IndexKeyExtractor extractor;
    extractor.extractKey(..., &key, ...);

    // Columnstore special-case (row buffering)
    if (index_type == IndexType::COLUMNSTORE) {
        columnstore->insert(column_id, record_uuid, value, value_len, is_null, ctx);
        continue;
    }

    // Key-based indexes (BTREE/HASH/GIN/GIST/SPGIST/BRIN/RTREE/HNSW/BITMAP/LSM/FULLTEXT)
    insertIntoIndex(index_type, index_ptr, key, record_uuid, ctx);
}

// UPDATE hook
Status StorageEngine::updateTuple(...) {
    // Only update indexes when indexed columns change
    removeFromIndex(index_type, index_ptr, old_key, record_uuid, ctx);
    insertIntoIndex(index_type, index_ptr, new_key, record_uuid, ctx);
}

// DELETE hook
Status StorageEngine::deleteTuple(...) {
    removeFromIndex(index_type, index_ptr, key, record_uuid, ctx);
}
```

### Current DML Integration Status

| Index Type   | INSERT | UPDATE | DELETE | Status |
|-------------|--------|--------|--------|--------|
| B-Tree      | ✅     | ✅     | ✅     | Active |
| Hash        | ✅     | ✅     | ✅     | Active |
| LSM-Tree    | ✅     | ✅     | ✅     | Active |
| GIN         | ✅     | ✅     | ✅     | Active |
| HNSW        | ✅     | ✅     | ✅     | Active |
| GiST        | ✅     | ✅     | ✅     | Active |
| SP-GiST     | ✅     | ✅     | ✅     | Active |
| BRIN        | ✅     | ✅     | ✅     | Active |
| Bitmap      | ✅     | ✅     | ✅     | Active |
| R-Tree      | ✅     | ✅     | ✅     | Active |
| Columnstore | ✅     | ✅     | ✅     | Active (row-buffered) |
| Fulltext    | ✅     | ✅     | ✅     | Active (InvertedIndex) |

**Note:** FULLTEXT uses length-prefixed column payloads (see `InvertedIndex`), and Columnstore is row-buffered on OLTP inserts.

---

## Choosing the Right Index

### Decision Matrix

| Workload Type | Recommended Index | Alternative |
|--------------|-------------------|-------------|
| Equality lookups | Hash | B-Tree |
| Range queries | B-Tree | LSM-Tree |
| Sorted retrieval | B-Tree | LSM-Tree |
| Array containment | GIN | - |
| JSON queries | GIN | - |
| Vector similarity | HNSW | - |
| Spatial queries (rectangles) | R-Tree | GiST |
| Spatial queries (points) | SP-GiST | GiST |
| Time-series (append-only) | BRIN | LSM-Tree |
| Low-cardinality columns | Bitmap | - |
| High-write throughput | LSM-Tree | B-Tree |
| Analytical queries | Columnstore | - |
| Custom predicates | GiST | - |

### By Data Type

| Data Type | Best Index | Use Case |
|-----------|-----------|----------|
| INT, BIGINT | B-Tree, Hash | General purpose |
| UUID | Hash, B-Tree | Fast equality |
| VARCHAR, TEXT | B-Tree, GIN | Prefix search, full-text |
| TIMESTAMP | B-Tree, BRIN | Time-series |
| BOOLEAN | Bitmap | Flags |
| ARRAY | GIN | Containment queries |
| JSON, JSONB | GIN | Key/value search |
| VECTOR | HNSW | Similarity search |
| POINT, POLYGON | R-Tree, GiST, SP-GiST | Spatial queries |

### By Query Pattern

**Equality Queries:**
```sql
WHERE user_id = 12345;
```
→ Use **Hash** or **B-Tree**

**Range Queries:**
```sql
WHERE created_at BETWEEN '2025-01-01' AND '2025-12-31';
```
→ Use **B-Tree** or **BRIN** (if physically correlated)

**Array Containment:**
```sql
WHERE tags @> ARRAY['postgresql', 'database'];
```
→ Use **GIN**

**K-NN Similarity:**
```sql
SELECT * FROM docs ORDER BY embedding <-> query_vector LIMIT 10;
```
→ Use **HNSW**

**Spatial Overlap:**
```sql
WHERE ST_Overlaps(geometry, search_region);
```
→ Use **R-Tree** or **GiST**

**Aggregations:**
```sql
SELECT SUM(revenue), AVG(price) FROM sales;
```
→ Use **Columnstore**

---

## Performance Characteristics

### Space Overhead

| Index Type | Space vs. Data Size | Notes |
|-----------|-------------------|-------|
| B-Tree | 1.2-1.5x | With compression |
| Hash | 1.1-1.3x | Depends on load factor |
| GIN | 1.5-3x | Depends on key cardinality |
| HNSW | 1.5-2x | Depends on M parameter |
| GiST | 1.3-2x | Depends on predicates |
| SP-GiST | 1.2-1.5x | Depends on partitioning |
| BRIN | 0.01-0.1x | Extremely compact |
| Bitmap | 0.1-10x | Depends on cardinality |
| LSM-Tree | 1.3-2x | Reduces with compaction |
| R-Tree | 1.5-2x | Depends on overlap |
| Columnstore | 0.3-0.7x | With compression |

### Write Amplification

| Index Type | Write Amplification | Notes |
|-----------|-------------------|-------|
| B-Tree | 1-3x | Page splits |
| Hash | 1-2x | Bucket splits |
| GIN | 2-5x | Posting list updates |
| HNSW | 3-8x | Graph updates |
| GiST | 2-4x | Tree rebalancing |
| SP-GiST | 1-3x | Partition splits |
| BRIN | 0.1-0.5x | Minimal writes |
| Bitmap | 1-2x | Bitmap updates |
| LSM-Tree | 2-10x | Compaction overhead |
| R-Tree | 2-4x | Tree updates |
| Columnstore | 1-2x | Append-only |

### Read Performance

| Index Type | Point Lookup | Range Scan | Notes |
|-----------|-------------|-----------|-------|
| B-Tree | O(log N) | O(log N + K) | Excellent |
| Hash | O(1) avg | N/A | No range support |
| GIN | O(K × log N) | N/A | K = key count |
| HNSW | O(log N) | N/A | Approximate |
| GiST | O(log N) | O(log N + K) | Good |
| SP-GiST | O(log N) | O(log N + K) | Good |
| BRIN | O(blocks) | O(blocks) | Fast for large tables |
| Bitmap | O(1) | O(N/64) | Bitmap operations |
| LSM-Tree | O(L × log N) | O(log N + K) | L = levels |
| R-Tree | O(log N) | O(log N + K) | Spatial |
| Columnstore | O(N) | O(N) | Column scans |

---

## Implementation Status

### Completion Summary

| Index Type | Core Ops | MGA | DML | Bytecode | Grade |
|-----------|---------|-----|-----|----------|-------|
| B-Tree | ✅ | ✅ | ✅ | ✅ | A+ |
| Hash | ✅ | ✅ | ✅ | ✅ | A+ |
| LSM-Tree | ✅ | ✅ | ✅ | ✅ | A |
| HNSW | ✅ | ✅ | ✅ | ✅ | A |
| GiST | ✅ | ✅ | ✅ | ✅ | A- |
| SP-GiST | ✅ | ✅ | ✅ | ✅ | A- |
| BRIN | ✅ | ✅ | ✅ | ✅ | A |
| Columnstore | ✅ | ✅ | ✅ | ✅ | A |
| R-Tree | ✅ | ✅ | ✅ | ✅ | A- |
| GIN | ✅ | ✅ | ✅ | ✅ | A- |
| Bitmap | ✅ | ✅ | ✅ | ✅ | A- |
| Fulltext (Inverted) | ⧗ | ✅ | ✅ | ✅ | B+ |

**Legend:**
- ✅ Complete
- ⧗ Partial or needs audit
- ❌ Not implemented
- ⚠️ Issues found

### Critical Issues

1. **FULLTEXT GC Stub** (Priority: P1)
   - File: `src/core/inverted_index.cpp:3795`
   - Issue: `InvertedIndex::removeDeadEntries` returns NOT_IMPLEMENTED
   - Impact: GC cannot purge dead inverted entries; index bloat risk

2. **Index GC Wiring Incomplete** (Priority: P1)
   - File: `src/core/garbage_collector.cpp:870-946`
   - Issue: GC only opens BTREE/HASH/GIN/BRIN/HNSW; other indexes not swept
   - Impact: Dead entries in GiST/SPGiST/RTREE/BITMAP/COLUMNSTORE/FULLTEXT/LSM not reclaimed

3. **GiST Cache Cleanup Leak** (Priority: P2)
   - File: `src/sblr/index_cache.cpp:270-290`
   - Issue: GiST deletion skipped due to incomplete type cleanup
   - Impact: Memory leak when evicting GiST indexes from cache

### Roadmap to 100% Completion

See `docs/audit/INDEX_SYSTEM_REMEDIATION_PLAN.md` for detailed roadmap.

**Estimated Effort:** 30-50 hours (GC wiring + FULLTEXT GC + cache cleanup)

---

## References

- **MGA Rules:** `/MGA_RULES.md`
- **Remediation Plan:** `/docs/audit/INDEX_SYSTEM_REMEDIATION_PLAN.md`
- **Agent Tasks:** `/docs/audit/INDEX_SYSTEM_AGENT_TASKS.md`
- **Comprehensive Audit:** `/docs/audit/INDEX_SYSTEM_COMPREHENSIVE_AUDIT.md`
- **Implementation Audit:** `/docs/IMPLEMENTATION_AUDIT.md`
- **Project Context:** `/PROJECT_CONTEXT.md`

---

**Document Version:** 1.0
**Last Updated:** January 22, 2026
**Status:** Production Documentation
**Next Review:** February 15, 2026 (post-remediation)

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
