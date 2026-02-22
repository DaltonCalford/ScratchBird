# DDL INDEX: CREATE
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [Object README](README.md)
- Next: [ALTER](alter.md)

## Coverage
- Status: Supported
- Parser coverage: native v3 `CREATE INDEX` supports method selection, key expressions, sort/null ordering, `INCLUDE`, partial predicate, tablespace, and `WITH (...)` options.
- Runtime coverage: method/option behavior is enforced by executor and catalog validation. This document reflects `src/parser/parser_v3.cpp`, `src/sblr/executor.cpp`, and the audit inventory in `docs/audit/SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`.

## Parser Surface
```sql
CREATE [CONCURRENTLY] [IF NOT EXISTS] [<index_name>]
ON <table_path>
[USING <index_method>]
(
  <column_or_expr> [ASC|DESC] [NULLS FIRST|LAST]
  [, ...]
)
[INCLUDE (<column> [, ...])]
[WHERE <predicate>]
[TABLESPACE <tablespace_path>]
[WITH (<option_name> = <scalar_value> [, ...])];
```

## Method Inventory (What You Can Name)

### Native parser method tokens (59)
`BTREE`, `HASH`, `HNSW`, `FULLTEXT`, `GIN`, `GIST`, `BRIN`, `RTREE`, `SPGIST`, `BITMAP`, `COLUMNSTORE`, `LSM`, `IVF`, `ZONEMAP`, `ART`, `BLOOM`, `VECTOR_FLAT`, `VECTOR_BIN_FLAT`, `IVF_FLAT`, `BIN_IVF_FLAT`, `IVF_PQ`, `IVF_SQ8`, `IVF_SQ8_HYBRID`, `RHNSW_PQ`, `RHNSW_SQ`, `ANNOY`, `NSG`, `DISKANN`, `SCANN`, `GPU_CAGRA`, `MINHASH_LSH`, `SPARSE_INVERTED`, `SPARSE_WAND`, `TRIE`, `INVERTED`, `STL_SORT`, `NGRAM`, `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `MONGODB_GEO_HAYSTACK`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`, `NEO4J_LOOKUP`, `NEO4J_TEXT`, `NEO4J_RANGE`, `NEO4J_POINT`, `NEO4J_VECTOR`, `CASSANDRA_SASI`, `CASSANDRA_SAI`, `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`.

### Audit canonical inventory (Beta 0.1.0 snapshot)
The audit inventory (`SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`) records 58 canonical names plus alias mappings. Use this `CREATE INDEX` document as the operational contract for native v3 behavior.

## Per-Method Purpose and Differences

### Core relational methods
- `BTREE`: ordered comparisons (`=`, `<`, `<=`, `>`, `>=`), stable general-purpose default.
- `HASH`: equality-focused lookups; useful when order scans are not required.
- `GIN`: inverted token/container indexing patterns (multi-value membership).
- `GIST`: extensible generalized search tree for non-scalar operator classes.
- `SPGIST`: space-partitioned GiST-style partitioning for non-balanced partitioned search spaces.
- `BRIN`: block-range summaries for very large append-heavy data.
- `RTREE`: geometric bounding-box style spatial indexing.
- `ZONEMAP`: min/max zone summaries for segment/page pruning.
- `BITMAP`: bitmap-style filtering, often low-cardinality analytics filters.
- `COLUMNSTORE`: columnar projection/filter acceleration.
- `LSM`: write-optimized merge-tree behavior for high ingest/compaction workflows.
- `FULLTEXT`: lexical/text search indexing.

### Vector and ANN methods
- `HNSW`: graph ANN for high-recall vector search.
- `IVF`: inverted-file centroid partitioned ANN.
- `VECTOR_FLAT`: exact float-vector scan baseline.
- `VECTOR_BIN_FLAT`: exact binary-vector scan baseline.
- `IVF_FLAT`: IVF coarse partition + flat residual search.
- `BIN_IVF_FLAT`: IVF variant for binary vectors.
- `IVF_PQ`: IVF with product quantization compression.
- `IVF_SQ8`: IVF with scalar quantization (8-bit).
- `IVF_SQ8_HYBRID`: IVF SQ8 + hybrid/GPU-threshold pathing.
- `RHNSW_PQ`: HNSW-relative family with PQ compression.
- `RHNSW_SQ`: HNSW-relative family with SQ8 compression.

### Advanced ANN methods
- `ANNOY`: random projection tree ANN.
- `NSG`: navigable sparse graph ANN.
- `DISKANN`: disk-oriented ANN graph/search pipeline.
- `SCANN`: partition + quantization + reorder ANN.
- `GPU_CAGRA`: GPU graph ANN profile.

### Symbolic/sparse/search methods
- `ART`: adaptive radix tree for prefix/exact key paths.
- `BLOOM`: probabilistic membership filter index profile.
- `TRIE`: prefix/token trie indexing.
- `NGRAM`: n-gram tokenized text lookup.
- `SPARSE_INVERTED`: sparse vector/inverted retrieval profile.
- `SPARSE_WAND`: weighted-and style sparse retrieval.
- `MINHASH_LSH`: locality-sensitive hashing for similarity candidate generation.

### Emulation-surface methods
- Mongo family: `MONGODB_2D`, `MONGODB_2DSPHERE`, `MONGODB_2DSPHERE_BUCKET`, `MONGODB_GEO_HAYSTACK`, `MONGODB_WILDCARD`, `MONGODB_ENCRYPTED_RANGE`.
- Neo4j family: `NEO4J_LOOKUP`, `NEO4J_TEXT`, `NEO4J_RANGE`, `NEO4J_POINT`, `NEO4J_VECTOR`.
- Cassandra family: `CASSANDRA_SASI`, `CASSANDRA_SAI`.
- Redis family: `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`.

### Native v3 method semantics
- `INVERTED`: generic inverted-index profile in native v3 runtime.
- `STL_SORT`: sorted-list profile routed through B-tree runtime semantics.

## WITH (...) Option Reference

### Global option
- `ARRAY_UNIQUENESS = WHOLE|ELEMENT`
- Purpose: defines uniqueness scope for `UNIQUE` indexes on ARRAY data.
- `WHOLE`: array value as a full unit.
- `ELEMENT`: uniqueness across elements.
- Constraints:
  - requires `UNIQUE`
  - requires exactly one key column
  - key column must be ARRAY type
  - not allowed for expression indexes

### Vector/ANN core options (`HNSW`, `IVF*`, `VECTOR_*`, `RHNSW_*`)
- `METRIC`: distance metric; float vectors allow `L2|COSINE|INNER_PRODUCT`; binary vectors allow `HAMMING|JACCARD|TANIMOTO`.
- `VECTOR_DIM`: declared vector dimension (`1..65535`).
- `M`: graph connectivity / PQ segment count depending on method (`1..1024` generally, `1..64` for PQ-specific fallback path).
- `EF_CONSTRUCTION`: HNSW build candidate depth (`>=1`).
- `EF_SEARCH`: HNSW query candidate depth (`>=1`).
- `NLIST`: IVF centroid list count (`>=1`).
- `NPROBE`: IVF probed-list count (`>=1`).
- `PQ_M`: product-quantizer sub-vector count (`1..64` in vector family paths).
- `PQ_BITS` / `BITS_PER_CODE`: PQ bit width; only `4` or `8`.
- `SQ_BITS`: scalar quantization bit width; SQ families require `8`.
- `GPU_SEARCH_THRESHOLD`: hybrid offload cutoff (`>=0`, used by `IVF_SQ8_HYBRID`).

### Advanced ANN options
- `ANNOY`: `N_TREES`, `LEAF_SIZE`, `SEARCH_K`, plus `METRIC`, `VECTOR_DIM`, `SEED`.
- `NSG`: `R`, `L`, `C`, `SEARCH_L`, plus `METRIC`, `VECTOR_DIM`, `SEED`.
- `DISKANN`: `MAX_DEGREE`, `BUILD_L`, `SEARCH_L`, `BEAM_WIDTH`, `PQ_BYTES`, `ALPHA`, `TRAINING_SAMPLE_SIZE`, `ENTRYPOINT_STRATEGY`, `USE_DISK_STORAGE`, plus `METRIC`, `VECTOR_DIM`, `SEED`.
  - `ENTRYPOINT_STRATEGY`: `MEDOID_SAMPLE|FIRST_VECTOR`.
- `SCANN`: `NLIST`, `NPROBE`, `REORDER_K`, `QUANTIZER`, `TRAINING_SAMPLE_SIZE`, `CENTROID_ITER`, optional PQ parameters.
  - `QUANTIZER`: `NONE|SQ8|PQ`.
  - If `QUANTIZER=PQ`, both `PQ_M` and `PQ_BITS` are required.
- `GPU_CAGRA`: `GRAPH_DEGREE`, `INTERMEDIATE_GRAPH_DEGREE`, `BUILD_ITERS`, `GPU_ID`, `ENTRYPOINT_STRATEGY`, `FALLBACK_CPU`, plus `METRIC`, `VECTOR_DIM`, `SEED`.
  - `INTERMEDIATE_GRAPH_DEGREE` must be `>= GRAPH_DEGREE` when both are set.
  - `ENTRYPOINT_STRATEGY`: `MEDOID_SAMPLE|FIRST_VECTOR`.

### Symbolic/sparse/search options
- `ART`: `PREFIX_INLINE_BYTES` (inline prefix bytes threshold).
- `BLOOM`: `PAGES_PER_RANGE`, `EXPECTED_ITEMS_PER_RANGE`, `FALSE_POSITIVE_RATE`, `SEED1`, `SEED2`.
- `TRIE`: `CASE_FOLD`, `MAX_NODE_BYTES`, `COMPRESS_CHAINS`.
- `NGRAM`: `MIN_N`, `MAX_N`, `CASE_FOLD`, `ANALYZER`.
  - `ANALYZER`: `UNICODE|ASCII`; `MIN_N <= MAX_N`.
- `SPARSE_INVERTED`: `MAX_DF`, `MIN_DF`, `NORM`, `TOPK_DEFAULT`.
  - `NORM`: `L1|L2|NONE`; `MIN_DF <= MAX_DF`.
- `SPARSE_WAND`: `BLOCK_SIZE`, `PIVOT_STRATEGY`, `TOPK_DEFAULT`.
  - `PIVOT_STRATEGY`: `MAX|AVG`.
- `MINHASH_LSH`: `NUM_PERM`, `BAND_COUNT`, `ROWS_PER_BAND`, `BLOOM_FP_RATE`, `ELEMENT_BIT_WIDTH`, `SEED`.
  - `NUM_PERM`, `BAND_COUNT`, `ROWS_PER_BAND` must be provided together.
  - `NUM_PERM = BAND_COUNT * ROWS_PER_BAND`.

### Mongo emulation options
- `MONGODB_2D`: `BITS`, `MIN`, `MAX`, `BUCKET_SIZE`.
- `MONGODB_2DSPHERE`: `MAX_CELLS`, `MIN_LEVEL`, `MAX_LEVEL`, `COARSEST_LEVEL`, `FINEST_LEVEL`.
- `MONGODB_2DSPHERE_BUCKET`: same as `2DSPHERE` plus `BUCKET_TIME_SPAN`.
- `MONGODB_GEO_HAYSTACK`: `BUCKET_SIZE` (required), `SECONDARY_KEY` (required), optional `MIN`, `MAX`.
- `MONGODB_WILDCARD`: `INCLUDE_PATHS`, `EXCLUDE_PATHS`, `MAX_PATH_DEPTH`.
- `MONGODB_ENCRYPTED_RANGE`: `RANGE_BITS`, `TOKEN_LEVELS`, `TOKEN_HMAC`, `RANGE_BUCKET_POLICY`.
  - `TOKEN_LEVELS <= RANGE_BITS`
  - `TOKEN_HMAC`: `HMAC_SHA256`
  - `RANGE_BUCKET_POLICY`: `EXACT|COARSE`

### Neo4j emulation options
- `NEO4J_LOOKUP`: `TOKEN_KIND`, `CACHE_MB`.
  - `TOKEN_KIND`: `LABEL|RELTYPE`
- `NEO4J_TEXT`: `MIN_N`, `MAX_N`, `CASE_FOLD`, `ANALYZER`.
  - `ANALYZER`: `UNICODE`; `MIN_N <= MAX_N`.
- `NEO4J_RANGE`: no method-specific `WITH` options.
- `NEO4J_POINT`: `SRID`, `DIMS`, `SPACE_FILLING_CURVE`, `CELL_SEMANTICS`, `CELL_CURVE`, `BITS_PER_DIM`, `HILBERT_VARIANT`, `AXIS_INVERT_MASK`.
  - `DIMS`: `2|3`
  - curve constraints and Hilbert variant sets are dimension-aware
  - `AXIS_INVERT_MASK` max is `3` for 2D and `7` for 3D
- `NEO4J_VECTOR`: `INDEX_IMPL`, `METRIC`, `VECTOR_DIM`, `M`, `EF_CONSTRUCTION`, `EF_SEARCH`, `PQ_M`, `PQ_BITS`, `BITS_PER_CODE`, `SQ_BITS`.
  - `INDEX_IMPL`: `HNSW|RHNSW_PQ|RHNSW_SQ`
  - `RHNSW_PQ` uses PQ controls (`PQ_BITS` in `4|8`)
  - `RHNSW_SQ` requires `SQ_BITS = 8`

### Cassandra emulation options
- `CASSANDRA_SASI`: `MODE`, `ANALYZER`, `MAX_TERM_LEN`, `SEGMENT_BYTES`.
  - `MODE`: `PREFIX|CONTAINS|SPARSE`
  - `ANALYZER`: `KEYWORD|WHITESPACE|SIMPLE|UNICODE`
- `CASSANDRA_SAI`: `INDEX_KIND`, `SEGMENT_BYTES`, `POSTING_BLOCK_ROWS`, `M`, `EF_CONSTRUCTION`.
  - `INDEX_KIND`: `STRING|NUMERIC|VECTOR`

### Redis emulation options
- Methods: `REDIS_STRING`, `REDIS_HASH`, `REDIS_LIST`, `REDIS_SET`, `REDIS_ZSET`, `REDIS_STREAM`, `REDIS_BITMAP`, `REDIS_HLL`, `REDIS_GEO`.
- Shared options:
  - `REDIS.PERSISTENCE`: `ALWAYS|SNAPSHOT|NEVER`
  - `REDIS.FLUSH_POLICY`: `IMMEDIATE|PERIODIC`
  - `REDIS.FLUSH_INTERVAL_MS`: integer `>=1`
  - `TTL_POLICY`: `LAZY|EAGER`

## Option Behavior Notes
- Executor validates method-specific option keys/values for vector, advanced ANN, symbolic/sparse, Mongo, Neo4j, Cassandra, and Redis families.
- Option values are scalar (`bool`, numeric, string) at parse time.
- Duplicate option keys are rejected.
- Method tokens in this page are parser and runtime supported in native v3.

## Examples

### Basic ordered index
```sql
CREATE INDEX idx_orders_created_at
ON sales.orders (created_at DESC);
```

### Vector ANN index
```sql
CREATE INDEX idx_items_embedding
ON app.items USING HNSW (embedding)
WITH (
  METRIC = COSINE,
  VECTOR_DIM = 1536,
  M = 32,
  EF_CONSTRUCTION = 200,
  EF_SEARCH = 80
);
```

### Unique ARRAY element-level index
```sql
CREATE UNIQUE INDEX uq_tags_per_element
ON app.docs (tags)
WITH (ARRAY_UNIQUENESS = ELEMENT);
```

### Mongo geo haystack emulation index
```sql
CREATE INDEX geo_idx
ON emulated.mongodb.shop.locations USING MONGODB_GEO_HAYSTACK (location)
WITH (
  BUCKET_SIZE = 1.5,
  SECONDARY_KEY = category,
  MIN = -180,
  MAX = 180
);
```

## Notes
- This page is the operational `CREATE INDEX` reference for native parser v3 in early beta `0.1.0`.
- For parser token snapshots and method-family listings, see:
  - `docs/audit/SYSTEM_DOMAIN_INDEX_CONTEXT_INVENTORY_BETA_0_1_0.md`
  - `docs/user-documentation/language-guide/data-types/index-methods/parser-accepted-methods.md`
