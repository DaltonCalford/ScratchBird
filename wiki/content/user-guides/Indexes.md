# Indexes

**Last Updated:** 2026-01-30

ScratchBird supports multiple index types, all designed to respect MGA visibility
(xmin/xmax + TIP checks) and stable tuple identifiers (TIDs). This page focuses
on how each index works, when to use it instead of a B-tree, and what to expect
from its storage and behavior.

## Core DDL

```
CREATE [UNIQUE] INDEX [CONCURRENTLY] [IF NOT EXISTS] index_name
    ON table_name [USING method]
    (column_or_expression [ASC|DESC] [NULLS FIRST|LAST], ...)
    [INCLUDE (column_list)]
    [WHERE predicate]
    [TABLESPACE tablespace_name];
```

Examples:

```
CREATE INDEX idx_users_email ON users (email);
CREATE INDEX idx_users_email_lower ON users (LOWER(email));
CREATE INDEX idx_orders_active ON orders (order_date) WHERE status = 'ACTIVE';
```

## Index type guide

### B-tree (default)

How it works:
- Balanced tree with ordered keys and linked leaf pages.
- Supports range scans and ordered retrieval efficiently.

Use instead of B-tree when:
- You do not need ordering or range scans and want faster equality lookups (Hash).
- You need multi-value containment or text search (GIN/Inverted).
- The table is physically correlated and very large (BRIN).

Notes:
- Best general-purpose index for mixed workloads.

### Hash

How it works:
- Extendible hashing with bucket pages and overflow chains.

Use instead of B-tree when:
- You only need equality lookups on high-cardinality columns.

Notes:
- No range scans or ORDER BY support.

### GIN (Generalized Inverted Index)

How it works:
- Maps keys to posting lists of TIDs (entry tree + posting trees).

Use instead of B-tree when:
- You need containment on arrays or JSON, or multi-key membership queries.

Notes:
- Higher write amplification; best for multi-value columns.

### Inverted index (full-text)

How it works:
- Token dictionary plus posting lists for terms; tuned for text search.

Use instead of B-tree when:
- You need relevance-ranked full-text search or token-based filtering.

Notes:
- Expect larger index size than B-tree; optimized for text workloads.

### GiST (Generalized Search Tree)

How it works:
- Extensible tree with custom predicates and consistent/penalty/picksplit logic.

Use instead of B-tree when:
- You need custom predicates (spatial, geometric, range-like types).

Notes:
- Predicate quality dictates performance.

### SP-GiST (Space-Partitioned GiST)

How it works:
- Space-partitioning tree with non-overlapping partitions.

Use instead of B-tree when:
- You need prefix, trie, quadtree, or point spatial indexing.

Notes:
- Useful for partitionable key spaces (points, prefixes, IP ranges).

### BRIN (Block Range Index)

How it works:
- Stores min/max summaries per block range.

Use instead of B-tree when:
- Data is physically correlated (time-series, append-only logs).

Notes:
- Lossy (returns candidate blocks); requires heap recheck.
- Extremely small compared to B-tree.

### Bitmap

How it works:
- One bitmap per distinct value; combines via AND/OR.

Use instead of B-tree when:
- Column has low cardinality (status flags, dimensions) and analytic filters.

Notes:
- Great for warehousing-style queries with many predicates.

### LSM-Tree (Log-Structured Merge Tree)

How it works:
- Writes go to a memtable and are flushed to SSTables with compaction.

Use instead of B-tree when:
- You need very high write throughput or time-series ingest.

Notes:
- Reads can touch multiple levels; compaction handles cleanup.

### Columnstore

How it works:
- Column-oriented segments with compression and per-segment statistics.

Use instead of B-tree when:
- OLAP workloads scan a few columns over many rows.

Notes:
- Append-oriented; best for analytics and reporting.

### Zone maps

How it works:
- Min/max metadata per block or segment for data skipping.

Use instead of B-tree when:
- Large scans benefit from pruning with minimal overhead.

Notes:
- Often paired with columnstore or sorted data.

### Bloom filter (auxiliary)

How it works:
- Probabilistic membership test (fast negative checks).

Use instead of B-tree when:
- You want to reduce wasted lookups in another index (B-tree, Hash, GIN, LSM).

Notes:
- Not a primary index; can return false positives but no false negatives.

### Full-text index

How it works:
- Dedicated full-text search index using tsvector and tsquery types.
- Integrates with GIN for posting list storage and fast lookups.

Use instead of B-tree when:
- You need ranked full-text search with tsvector/tsquery operations.

Notes:
- Works with the text search configuration system (`ts_config`).
- Implementation: `fulltext_index.cpp`, `tsvector.cpp`, `tsquery.cpp`, `ts_operations.cpp`.

### HNSW (Hierarchical Navigable Small World)

How it works:
- Multi-layer proximity graph for approximate nearest neighbor search.

Use instead of B-tree when:
- You need high-recall vector search with low latency.

Notes:
- Higher memory overhead; excellent query speed.

### IVF (Inverted File)

How it works:
- Partitions vector space into clusters with centroid-based lookup.
- Two-phase search: find nearest centroids, then search within partitions.

Use instead of B-tree when:
- You need vector search with lower memory overhead than HNSW.
- Dataset is very large and approximate results are acceptable.

Notes:
- Requires training phase to establish centroids.
- Trade-off between recall and search speed via nprobe parameter.

### R-Tree

How it works:
- Tree of minimum bounding rectangles (MBRs).

Use instead of B-tree when:
- Spatial data uses rectangle overlap/containment queries.

Notes:
- Works well for GIS and bounding-box searches.

## MGA integration (shared behavior)

All index types are required to:

- Use TIP-based visibility checks (no snapshot-based visibility).
- Reference stable TIDs (indexes only update when indexed columns change).
- Support logical deletion and cooperative garbage collection via `index_gc_interface.h`.
- Bloom filters can be attached to B-tree, Hash, and GIN indexes for accelerated negative lookups.

## Implementation

All index types listed above are implemented in the Alpha codebase. The index factory
(`src/core/index_factory.cpp`) handles creation and loading. Key infrastructure includes
`index_params.cpp` for parameter parsing, `index_key_extractor.cpp` for key extraction,
and `global_uniqueness_index.cpp` for cross-partition uniqueness enforcement.

## References

- `docs/specifications/ddl/DDL_INDEXES.md`
- `docs/specifications/indexes/INDEX_ARCHITECTURE.md`
- `docs/specifications/indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `docs/specifications/indexes/BloomFilterIndex.md`
- `docs/specifications/indexes/ZoneMapsIndex.md`
- `docs/specifications/indexes/IVFIndex.md`
- `docs/specifications/indexes/InvertedIndex.md`
- `docs/specifications/indexes/LSM_TREE_SPEC.md`
- `docs/specifications/indexes/COLUMNSTORE_SPEC.md`
