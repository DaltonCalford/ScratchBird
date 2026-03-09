# Specification: Index Metadata

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:655`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:730`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4963`

## Synopsis

This specification defines index metadata storage for all 28+ index types supported by ScratchBird, including the IndexInfo structure, index states, expression and partial indexes, and index access methods.

## Scope

### In Scope

- Index metadata structures (IndexInfo)
- All 28 index types and their characteristics
- Index states (BUILDING, ACTIVE, RETIRED, etc.)
- Expression indexes
- Partial (filtered) indexes
- Index column mappings
- Index storage parameters

### Out of Scope

- Physical index page layouts (see specific index specs)
- Index access methods implementation
- Index build algorithms

## Specification

### Index Types

**Source:** `include/scratchbird/core/catalog_manager.h:655`

ScratchBird supports 28+ index types:

```cpp
enum class IndexType : uint8_t {
    // Standard relational indexes (0x00-0x0F)
    BTREE = 0x00,           // B-tree (default)
    HASH = 0x01,            // Hash index
    HNSW = 0x02,            // Hierarchical Navigable Small World
    VECTOR = 0x02,          // Alias for HNSW
    FULLTEXT = 0x03,        // Full-text search (GIN-based)
    GIN = 0x04,             // Generalized Inverted Index
    GIST = 0x05,            // Generalized Search Tree
    BRIN = 0x06,            // Block Range Index
    RTREE = 0x07,           // R-tree spatial index
    SPGIST = 0x08,          // Space-Partitioned GiST
    BITMAP = 0x09,          // Bitmap index
    COLUMNSTORE = 0x0A,     // Columnstore index
    LSM = 0x0B,             // LSM-Tree
    IVF = 0x0C,             // Inverted File (vector)
    ZONEMAP = 0x0D,         // Zone map (min/max)
    ART = 0x0E,             // Adaptive Radix Tree
    BLOOM = 0x0F,           // Bloom filter range
    
    // Vector indexes (0x10-0x1F)
    VECTOR_FLAT = 0x10,     // Brute-force float vector
    VECTOR_BIN_FLAT = 0x11, // Brute-force binary vector
    IVF_FLAT = 0x12,        // IVF flat variant
    BIN_IVF_FLAT = 0x13,    // IVF binary variant
    IVF_PQ = 0x14,          // IVF product quantization
    IVF_SQ8 = 0x15,         // IVF scalar quantization
    IVF_SQ8_HYBRID = 0x16,  // IVF SQ8 hybrid routing
    RHNSW_PQ = 0x17,        // HNSW with PQ payload
    RHNSW_SQ = 0x18,        // HNSW with SQ payload
    ANNOY = 0x19,           // Approximate Nearest Neighbors Oh Yeah
    NSG = 0x1A,             // Navigating Spreading-out Graph
    DISKANN = 0x1B,         // Disk-based ANN
    SCANN = 0x1C,           // ScaNN partitioned ANN
    GPU_CAGRA = 0x1D,       // GPU CAGRA graph ANN
    MINHASH_LSH = 0x1E,     // MinHash LSH
    SPARSE_INVERTED = 0x1F, // Sparse inverted
    SPARSE_WAND = 0x20,     // Sparse WAND
    
    // Specialized indexes (0x21-0x2F)
    TRIE = 0x21,            // Radix trie
    INVERTED = 0x22,        // Generic inverted
    STL_SORT = 0x23,        // Sorted-list (B-tree runtime)
    NGRAM = 0x24,           // N-gram index
    
    // MongoDB indexes (0x25-0x2A)
    MONGODB_2D = 0x25,              // Planar 2d geospatial
    MONGODB_2DSPHERE = 0x26,        // Spherical 2dsphere
    MONGODB_2DSPHERE_BUCKET = 0x27, // 2dsphere bucket (time-series)
    MONGODB_GEO_HAYSTACK = 0x28,    // geoHaystack
    MONGODB_WILDCARD = 0x29,        // Wildcard path/value
    MONGODB_ENCRYPTED_RANGE = 0x2A, // Encrypted range
    
    // Neo4j indexes (0x2B-0x2F)
    NEO4J_LOOKUP = 0x2B,    // Label/reltype token map
    NEO4J_TEXT = 0x2C,      // Text search (contains/startsWith/endsWith)
    NEO4J_RANGE = 0x2D,     // Range index
    NEO4J_POINT = 0x2E,     // Point index (space-filling curve)
    NEO4J_VECTOR = 0x2F,    // Vector index
    
    // Cassandra indexes (0x30-0x31)
    CASSANDRA_SASI = 0x30,  // SSTable Attached Secondary Index
    CASSANDRA_SAI = 0x31,   // Storage Attached Index
    
    // Redis structure indexes (0x32-0x3A)
    REDIS_STRING = 0x32,    // String structure
    REDIS_HASH = 0x33,      // Hash structure
    REDIS_LIST = 0x34,      // List structure
    REDIS_SET = 0x35,       // Set structure
    REDIS_ZSET = 0x36,      // Sorted-set structure
    REDIS_STREAM = 0x37,    // Stream structure
    REDIS_BITMAP = 0x38,    // Bitmap structure
    REDIS_HLL = 0x39,       // HyperLogLog structure
    REDIS_GEO = 0x3A        // Geo structure
};
```

### Index Type Categories

| Category | Types | Primary Use |
|----------|-------|-------------|
| **Relational** | BTREE, HASH, GIN, GIST, BRIN, SPGIST | Standard SQL queries |
| **Vector** | HNSW, IVF_*, ANNOY, NSG, DISKANN, SCANN | Similarity search |
| **Spatial** | RTREE, MONGODB_2D, MONGODB_2DSPHERE | Geographic data |
| **Text** | FULLTEXT, NGRAM, NEO4J_TEXT | Text search |
| **NoSQL** | MONGODB_*, NEO4J_*, CASSANDRA_*, REDIS_* | Emulation |
| **Specialized** | BITMAP, COLUMNSTORE, LSM, TRIE | Analytics, time-series |

### Index States

**Source:** `include/scratchbird/core/catalog_manager.h:720`

```cpp
enum class IndexState : uint8_t {
    BUILDING = 0,   // Index is being built (not yet visible)
    ACTIVE = 1,     // Index is active and available
    RETIRED = 2,    // Old version after rebuild
    FAILED = 3,     // Index build failed
    INACTIVE = 4    // Index disabled via ALTER INDEX
};
```

**State Transitions:**

```
                    ┌──────────┐
     CREATE INDEX   │          │
    ┌──────────────▶│ BUILDING │
    │               │          │
    │               └────┬─────┘
    │                    │ Build complete
    │                    ▼
    │               ┌──────────┐
    │               │          │◄───────┐
    └───────────────│  ACTIVE  │        │ ALTER INDEX ENABLE
                    │          │────────┘
                    └────┬─────┘        │ ALTER INDEX DISABLE
                         │
         ┌───────────────┼───────────────┐
         │               │               │
    Rebuild         DROP INDEX       Build failed
         │               │               │
         ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐   ┌──────────┐
    │ RETIRED  │    │ DELETED  │   │  FAILED  │
    │(old ver) │    │(is_valid=│   │          │
    └──────────┘    │    0)    │   └──────────┘
                    └──────────┘
```

### IndexInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:730`

```cpp
struct IndexInfo {
    // Identity
    ID index_id;                    // UUIDv7 index identifier
    ID table_id;                    // Parent table
    std::string index_name;         // Index name
    bool name_is_delimited = false; // Quoted identifier flag
    ID owner_id;                    // Owner UUID
    
    // Physical storage
    GPID root_gpid = 0;             // Root page of index
    uint16_t tablespace_id = 0;     // Tablespace ID
    ID tablespace_uuid{};           // Tablespace UUID reference
    
    // Index characteristics
    IndexType index_type = IndexType::BTREE;
    bool is_unique = false;
    std::vector<ID> column_ids;     // Key columns
    std::vector<ID> include_column_ids;  // Include columns (covering index)
    
    // Parameters
    ID index_params_oid{};          // TOAST for index parameters
    uint32_t collation_id = 101;    // Default: utf8_general_ci
    
    // R-tree specific
    uint32_t rtree_max_entries = 50;  // Maximum entries per node (M)
    
    // Expression and partial indexes
    bool is_expression_index = false;   // Index on expression
    bool is_partial_index = false;      // Index with WHERE clause
    ID expression_oid{};                // TOAST for expression tree
    ID predicate_oid{};                 // TOAST for WHERE predicate
    std::vector<std::string> expression_strings;  // Original SQL
    std::string predicate_string;       // Original WHERE clause
    std::vector<uint8_t> expression_data;   // Serialized expression
    std::vector<uint8_t> predicate_data;    // Serialized predicate
    
    // Dependency tracking
    ID dependency_id;               // Dependency: index → table
    
    // Versioning and state (Plan 01 Task E)
    ID logical_index_id;            // Stable logical ID across rebuilds
    uint8_t state = 1;              // IndexState enum value
    uint64_t valid_from_xid = 0;    // XID when new txns can use
    uint64_t retired_xid = 0;       // XID after which not used
    uint64_t build_started_time = 0;
    uint64_t build_completed_time = 0;
    
    // Metadata
    uint64_t created_time = 0;
};
```

### sb_indexes Catalog Table

**Source:** `src/core/catalog_manager.cpp:4963`

```cpp
struct IndexRecord {
    // Primary key
    ID index_id;
    
    // Parent table
    ID table_id;
    char index_name[512];
    ID owner_id;
    
    // Physical storage
    uint64_t root_gpid;
    ID tablespace_id;
    
    // Index type and flags
    uint8_t index_type;
    uint8_t is_unique;
    uint8_t is_expression;
    uint8_t is_partial;
    uint8_t name_is_delimited;
    uint8_t reserved[3];
    
    // Column references
    uint16_t column_count;
    ID column_ids[16];              // Up to 16 key columns
    uint16_t include_column_count;
    ID include_column_ids[16];      // Up to 16 include columns
    
    // Parameters
    ID index_params_oid;
    uint32_t collation_id;
    uint32_t rtree_max_entries;
    
    // Expression/partial index data
    ID expression_oid;
    ID predicate_oid;
    
    // Versioning
    ID logical_index_id;
    uint8_t state;
    uint8_t reserved2[7];
    uint64_t valid_from_xid;
    uint64_t retired_xid;
    uint64_t build_started_time;
    uint64_t build_completed_time;
    
    // Dependency
    ID dependency_id;
    
    // Metadata
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Expression Index

```sql
-- Expression index example
CREATE INDEX idx_upper_name ON employees (UPPER(last_name));

-- Implementation details:
-- is_expression_index = true
-- expression_strings = ["UPPER(last_name)"]
-- expression_oid → TOAST with serialized expression tree
-- column_ids = [] (empty, not a simple column index)
```

### Partial Index

```sql
-- Partial index example
CREATE INDEX idx_active_orders ON orders (order_date) 
WHERE status = 'ACTIVE';

-- Implementation details:
-- is_partial_index = true
-- predicate_string = "status = 'ACTIVE'"
-- predicate_oid → TOAST with serialized WHERE clause
-- Only rows matching predicate are indexed
```

### Covering Index (INCLUDE)

```sql
-- Covering index example
CREATE INDEX idx_covering ON orders (customer_id) 
INCLUDE (order_date, total_amount);

-- Implementation details:
-- column_ids = [customer_id_column_id]
-- include_column_ids = [order_date_column_id, total_amount_column_id]
-- Index-only scans can satisfy queries using included columns
```

### Index Type-Specific Parameters

#### R-tree Parameters

```cpp
struct RTreeIndexParams {
    uint32_t max_entries;       // M parameter (default 50)
    uint32_t min_entries;       // m = M/2 (default 25)
    uint8_t split_algorithm;    // QUADRATIC, LINEAR, etc.
};
```

#### HNSW Parameters

```cpp
struct HNSWIndexParams {
    uint32_t m;                 // Number of edges per node
    uint32_t ef_construction;   // Size of dynamic candidate list
    uint32_t ef_search;         // Size of search candidate list
    uint8_t metric_type;        // L2, IP, COSINE
};
```

#### IVF Parameters

```cpp
struct IVFIndexParams {
    uint32_t nlist;             // Number of clusters
    uint32_t nprobe;            // Clusters to search
    uint8_t metric_type;        // Distance metric
    uint8_t quantization;       // FLAT, PQ, SQ8
};
```

## Algorithms

### Algorithm: Create Index

```
Input:  Table ID, index definition, build options
Output: Index ID

1. Validate table exists
2. Validate column references exist
3. If expression index:
   a. Parse and validate expression
   b. Serialize to bytecode
   c. Store in TOAST if > 2KB
4. If partial index:
   a. Parse and validate predicate
   b. Serialize to bytecode
   c. Store in TOAST if > 2KB
5. Generate UUIDv7 for index_id
6. Allocate root GPID for index
7. Set state = BUILDING
8. Create IndexRecord
9. Begin index build:
   a. Scan table data
   b. Build index structure
   c. If unique, check for duplicates
10. Set state = ACTIVE
11. Commit transaction
```

### Algorithm: Rebuild Index (Shadow)

```
Input:  Index ID
Output: Success/Failure

1. Get current index info
2. Generate new logical_index_id if first rebuild
3. Create new physical index with:
   - Same logical_index_id
   - New index_id
   - state = BUILDING
4. Build new index structure
5. Set valid_from_xid = next_xid
6. Set old index retired_xid = next_xid
7. Set old index state = RETIRED
8. Set new index state = ACTIVE
9. Schedule old index cleanup
```

### Algorithm: Select Index for Query

```
Input:  Query conditions, available indexes
Output: Selected index or none

1. Filter indexes:
   a. state must be ACTIVE
   b. Check valid_from_xid / retired_xid
   c. Match index type to query type
2. For each candidate:
   a. Score match quality
   b. Consider selectivity
   c. Consider covering columns
3. Return highest scoring index
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `IDX_INV_001` | index_id is valid UUIDv7 | isUuidV7Local() check |
| `IDX_INV_002` | table_id references valid table | Foreign key check |
| `IDX_INV_003` | All column_ids reference valid columns | Validation |
| `IDX_INV_004` | Unique indexes have no duplicates | Build-time check |
| `IDX_INV_005` | root_gpid valid for ACTIVE indexes | State check |
| `IDX_INV_006` | logical_index_id stable across rebuilds | Version check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `INDEX_EXISTS` | Name conflict | Choose different name |
| `INVALID_INDEX_TYPE` | Unsupported type | Use valid type |
| `DUPLICATE_KEY` | Unique constraint violation | Remove duplicates |
| `INDEX_BUILD_FAILED` | Build error | Set state = FAILED |
| `INVALID_EXPRESSION` | Expression parse error | Fix expression |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_indexes.cpp` | Index CRUD |
| `tests/unit/test_btree_index.cpp` | B-tree operations |
| `tests/unit/test_hash_index.cpp` | Hash index |
| `tests/unit/test_vector_index.cpp` | HNSW/IVF indexes |
| `tests/unit/test_expression_index.cpp` | Expression indexes |
| `tests/unit/test_partial_index.cpp` | Partial indexes |

## Related Specifications

- [tables.md](./tables.md) - Parent table metadata
- [constraints.md](./constraints.md) - Unique constraints
- Specific index type specs (btree.md, hnsw.md, etc.)

## Appendix

### Index Type Summary Table

| Type | Code | Unique | Multi-col | Expressions | Best For |
|------|------|--------|-----------|-------------|----------|
| BTREE | 0x00 | Yes | Yes | Yes | Range queries, sorting |
| HASH | 0x01 | Yes | No | No | Equality only |
| HNSW | 0x02 | No | No | Yes | Vector similarity |
| GIN | 0x04 | No | Yes | Yes | Array, full-text |
| GIST | 0x05 | Yes | Yes | Yes | Custom types |
| BRIN | 0x06 | No | Yes | No | Large tables, block stats |
| RTREE | 0x07 | Yes | Yes | No | Spatial data |
| BITMAP | 0x09 | No | Yes | No | OLAP, low cardinality |
| LSM | 0x0B | Yes | Yes | Yes | Write-heavy workloads |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
