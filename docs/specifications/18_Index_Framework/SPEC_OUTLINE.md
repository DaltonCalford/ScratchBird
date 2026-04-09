# Spec Outline - 18_Index_Framework

## Purpose
Define index types, binary layouts, maintenance operations, rewrite closure, relocation cleanup, and dialect mappings with no ambiguity.

## Detailed Specifications
- INDEX_ARCHITECTURE.md
- INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md
- INDEX_CATALOG_AND_METADATA.md
- INDEX_KEY_ENCODING.md
- INDEX_METRICS_AND_COSTING.md
- INDEX_FAMILY_METRICS_AND_CALIBRATION.md
- INDEX_DDL_AND_SEMANTICS.md
- INDEX_MANAGEMENT_SQL.md
- INDEX_BUILD_AND_MAINTENANCE.md
- INDEX_CONCURRENCY_AND_VISIBILITY.md
- INDEX_MGA_PUBLICATION_AND_RECLAIM.md
- INDEX_RELOCATION_CLEANUP_HOOK_REGISTRY.md
- INDEX_ROLLBACK_REWRITE_FAMILY_CLOSURE.md
- INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md
- INDEX_VERSION_SEMANTICS_AND_DEAD_ENTRY_LIFECYCLE.md
- OPCLASS_DEFINITIONS.md
- ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md
- SUMMARY_BITMAP_COLUMNSTORE_PLANNER_SPEC.md
- GENERALIZED_SEARCH_AND_SPATIAL_PLANNER_SPEC.md
- INVERTED_TEXT_AND_RANKING_PLANNER_SPEC.md
- VECTOR_ANN_PLANNER_SPEC.md
- BTREE_SPEC.md
- BTREE_STRUCTURAL_MODIFICATION_DURABILITY_PROTOCOL.md
- BTREE_COMPRESSED_PAGE_SEARCH_ACCELERATION.md
- BTREE_PIVOT_TUPLE_AND_SEPARATOR_KEYS.md
- BTREE_CONCURRENCY_AND_SPLIT_TOLERANT_DESCENT.md
- BTREE_PAGE_DELETION_MERGE_AND_RECLAMATION.md
- BTREE_DUPLICATE_KEY_AND_POSTING_LIST_MANAGEMENT.md
- BTREE_MGA_VERSION_CHURN_MANAGEMENT.md
- BTREE_PERSISTENT_METADATA_AND_ROOT_MANAGEMENT.md
- BTREE_BULK_BUILD_AND_REBUILD_PROTOCOL.md
- BTREE_OPTIONAL_WRITE_AND_LOOKUP_ACCELERATION.md
- BTREE_VERIFICATION_AND_HARDENING_FRAMEWORK.md
- HASH_SPEC.md
- GIN_SPEC.md
- GIST_SPEC.md
- SPGIST_SPEC.md
- BRIN_SPEC.md
- FULLTEXT_SPEC.md
- FULLTEXT_PG_TSCONFIG.md
- FULLTEXT_PG_STOPWORDS_DATA.md
- FULLTEXT_RANKING_MODES.md
- SPATIAL_SPEC.md
- BITMAP_SPEC.md
- COLUMNSTORE_SPEC.md
- LSM_TREE_SPEC.md
- HNSW_SPEC.md
- IVF_SPEC.md
- VECTOR_FLAT_SPEC.md
- IVF_VARIANTS_SPEC.md
- ART_SPEC.md
- BLOOM_SPEC.md
- ANNOY_SPEC.md
- NSG_SPEC.md
- DISKANN_SPEC.md
- SCANN_SPEC.md
- GPU_CAGRA_SPEC.md
- MINHASH_LSH_SPEC.md
- SPARSE_INVERTED_SPEC.md
- SPARSE_WAND_SPEC.md
- TRIE_SPEC.md
- INVERTED_SPEC.md
- STL_SORT_SPEC.md
- NGRAM_SPEC.md
- MONGODB_2D_SPEC.md
- MONGODB_2DSPHERE_SPEC.md
- MONGODB_2DSPHERE_BUCKET_SPEC.md
- MONGODB_GEO_HAYSTACK_SPEC.md
- MONGODB_WILDCARD_SPEC.md
- MONGODB_ENCRYPTED_RANGE_SPEC.md
- NEO4J_LOOKUP_SPEC.md
- NEO4J_TEXT_SPEC.md
- NEO4J_RANGE_SPEC.md
- NEO4J_POINT_SPEC.md
- NEO4J_VECTOR_SPEC.md
- CASSANDRA_SASI_SPEC.md
- CASSANDRA_SAI_SPEC.md
- REDIS_DATA_STRUCTURES_SPEC.md
- DIALECT_COMPATIBILITY_MATRIX.md

## Dependencies
- 14_Base_Scalar_Types
- 13_Operator_Model_and_Coercion
- 10_GC_and_Sweep
- 05_Page_Taxonomy_and_Binary_Layouts
- 08_Transaction_Core
- 31_Conformance_Performance_and_Reliability_Gates

## Definitions
- Index: An auxiliary structure to accelerate queries and enforce constraints.
- Index tuple: Key data plus TID pointer(s) to heap versions.
- TID: Tuple identifier referencing a specific row version.
- Operator class: Mapping between operators and indexable types.
- High key: Sentinel key that defines the upper bound of a B-tree page.

## Canonical Index Catalog
- `index` fields:
- `index_uuid` (UUID)
- `table_uuid` (UUID)
- `index_name` (string)
- `index_type` (enum)
- `root_page_id` (u32)
- `unique_flag` (bool)
- `nulls_distinct` (bool)
- `fillfactor` (u16)
- `predicate_expr` (nullable)
- `expression_expr` (nullable)
- `created_at` (timestamp)
- `state` (active, building, invalid)
- `index_column` fields:
- `index_uuid`
- `column_uuid`
- `position`
- `opclass_uuid`
- `sort_order` (ASC, DESC)
- `null_order` (FIRST, LAST)
- `index_opclass` fields:
- `opclass_uuid`
- `name`
- `index_type`
- `strategy_functions` (json)

## Canonical Index Page Types
- `PAGE_TYPE_BTREE_META`
- `PAGE_TYPE_BTREE_INTERNAL`
- `PAGE_TYPE_BTREE_LEAF`
- `PAGE_TYPE_HASH_META`
- `PAGE_TYPE_HASH_BUCKET`
- `PAGE_TYPE_HASH_OVERFLOW`
- `PAGE_TYPE_HASH_BITMAP`
- `PAGE_TYPE_GIN_META`
- `PAGE_TYPE_GIN_ENTRY`
- `PAGE_TYPE_GIN_DATA`
- `PAGE_TYPE_GIN_PENDING`
- `PAGE_TYPE_GIST_INTERNAL`
- `PAGE_TYPE_GIST_LEAF`
- `PAGE_TYPE_SPGIST_META`
- `PAGE_TYPE_SPGIST_INNER`
- `PAGE_TYPE_SPGIST_LEAF`
- `PAGE_TYPE_BRIN_META`
- `PAGE_TYPE_BRIN_REVMAP`
- `PAGE_TYPE_BRIN_DATA`
- `PAGE_TYPE_FTS_META`
- `PAGE_TYPE_FTS_DICT`
- `PAGE_TYPE_FTS_POSTINGS`
- `PAGE_TYPE_SPATIAL_META`
- `PAGE_TYPE_SPATIAL_NODE`
- `PAGE_TYPE_BITMAP_META`
- `PAGE_TYPE_BITMAP_DICT`
- `PAGE_TYPE_BITMAP_CONTAINER`
- `PAGE_TYPE_COLUMNSTORE_META`
- `PAGE_TYPE_COLUMNSTORE_SEGMENT`
- `PAGE_TYPE_COLUMNSTORE_DICT`
- `PAGE_TYPE_COLUMNSTORE_RLE`
- `PAGE_TYPE_COLUMNSTORE_BITPACK`
- `PAGE_TYPE_LSM_META`
- `PAGE_TYPE_LSM_SSTABLE`
- `PAGE_TYPE_LSM_INDEX`
- `PAGE_TYPE_LSM_FILTER`
- `PAGE_TYPE_HNSW_META`
- `PAGE_TYPE_HNSW_NODE`
- `PAGE_TYPE_IVF_META`
- `PAGE_TYPE_IVF_CENTROID`
- `PAGE_TYPE_IVF_LIST`
- `PAGE_TYPE_ART_NODE`
- `PAGE_TYPE_BLOOM_META`
- `PAGE_TYPE_BLOOM_RANGE`
- `PAGE_TYPE_VECTOR_FLAT_META`
- `PAGE_TYPE_VECTOR_FLAT_SEGMENT`
- `PAGE_TYPE_ANNOY_META`
- `PAGE_TYPE_ANNOY_NODE`
- `PAGE_TYPE_NSG_META`
- `PAGE_TYPE_NSG_NODE`
- `PAGE_TYPE_DISKANN_META`
- `PAGE_TYPE_DISKANN_GRAPH`
- `PAGE_TYPE_DISKANN_VECTOR_BLOCK`
- `PAGE_TYPE_SCANN_META`
- `PAGE_TYPE_SCANN_CENTROID`
- `PAGE_TYPE_SCANN_PARTITION`
- `PAGE_TYPE_CAGRA_META`
- `PAGE_TYPE_CAGRA_NODE`
- `PAGE_TYPE_MINHASH_META`
- `PAGE_TYPE_MINHASH_BUCKET`
- `PAGE_TYPE_SPARSE_META`
- `PAGE_TYPE_SPARSE_DICT`
- `PAGE_TYPE_SPARSE_POSTINGS`
- `PAGE_TYPE_TRIE_META`
- `PAGE_TYPE_TRIE_NODE`
- `PAGE_TYPE_INVERTED_META`
- `PAGE_TYPE_INVERTED_DICT`
- `PAGE_TYPE_INVERTED_POSTINGS`
- `PAGE_TYPE_SORT_META`
- `PAGE_TYPE_SORT_RUN`
- `PAGE_TYPE_REDIS_META`
- `PAGE_TYPE_REDIS_HASH`
- `PAGE_TYPE_REDIS_LIST`
- `PAGE_TYPE_REDIS_SET`
- `PAGE_TYPE_REDIS_ZSET`
- `PAGE_TYPE_REDIS_STREAM`
- `PAGE_TYPE_REDIS_BITMAP`
- `PAGE_TYPE_REDIS_HLL`
- `PAGE_TYPE_REDIS_GEO`

## Canonical Index Tuple and TID Format
- TID (16 bytes):
- `page_id` (u32)
- `slot_id` (u16)
- `version_id` (u16)
- `table_uuid_hash` (u32)
- `reserved` (u32)
- Index tuple header (12 bytes):
- `flags` (u16)
- `key_len` (u16)
- `tid_count` (u16)

## Update 2026-03-28: Current authority ordering

For implementation reality, section `18` currently resolves in this order:
1. `CatalogManager::IndexType` and `IndexInfo`
2. `IndexFactory` registry, storage-model, and runtime-class mapping
3. executor family routing and fail-closed generic operator handling
4. garbage-collector runtime-class cleanup handling
5. concrete family implementations such as `btree.cpp`, `hash_index.cpp`, `rtree.cpp`, `columnstore.cpp`, and vector or ANN backends

Current code-backed section `18` authority surfaces are:
- family enum and registry exposure
- catalog create and open lifecycle
- runtime-class sharing and alias lowering
- executor lookup, delete, and range-routing boundaries
- GC cleanup-family boundaries

The following documents remain canonical but broader than the currently re-proven code surface and need deeper family passes:
- `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- `INDEX_BUILD_AND_MAINTENANCE.md`
- family-specific specs outside the currently re-audited runtime classes

## Update 2026-03-28: current contradiction map

Current section `18` authority now maps as follows:
- runtime authority:
  - `INDEX_ARCHITECTURE.md`
  - `INDEX_DDL_AND_SEMANTICS.md`
- planner and exactness authority:
  - `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- metadata authority:
  - `INDEX_CATALOG_AND_METADATA.md`
- maintenance boundary:
  - `INDEX_BUILD_AND_MAINTENANCE.md`
- shared-backend family boundary:
  - `README.md`
  - `INDEX_ARCHITECTURE.md`
  - family-specific specs as they are re-audited
- `reserved` (u16)
- `child_page_id` (u32) for internal nodes
- Index tuple payload:
- `key_bytes` (key_len), see `INDEX_KEY_ENCODING.md`.
- `tid_list` (tid_count * 16 bytes) for leaf nodes

## Common Rules
- All index updates are transactional and follow MGA visibility.
- Index entries reference specific row versions by TID.
- Index entries for uncommitted rows are visible only to the owning transaction.
- GC removes index entries for dead row versions.
- Updates that change indexed key semantics must perform index maintenance even
  when logical row identity remains stable.
- Unique index checks must ignore rows not visible to the current snapshot.
- If a partial index predicate exists, only rows matching the predicate are indexed.

## Key Encoding Rules
- Keys are concatenations of per-segment encodings defined in `INDEX_KEY_ENCODING.md`.
- Each segment stores a null sort byte, flags, length, and bytes.
- Descending order is implemented by comparator inversion for the segment bytes.
- Comparisons are segment-aware using the encoded segment lengths.
- `desc_sort_mode` controls comparator inversion vs bytewise complement.

## B-tree (Canonical)
The B-tree family is split into:
- `BTREE_SPEC.md` for core page layout and baseline tree behavior
- the B-tree hardening bundle for crash safety, compressed search,
  separator rigor, concurrency, reclamation, duplicate pressure, MGA churn,
  metadata publication, build protocol, optional acceleration, and verification
### Meta Page
- `root_page_id` (u32)
- `tree_height` (u16)
- `key_type_id` (u16)
- `fillfactor` (u16)
- `reserved` (u16)

### Page Layout
- Page header: standard page header.
- B-tree header (32 bytes):
- `right_sibling` (u32)
- `left_sibling` (u32)
- `leftmost_child_page_id` (u32) (internal pages only)
- `prefix_total` (i32)
- `level` (u8) (0 = leaf)
- `flags` (u8) (bit0 = descending, bit1 = released)
- `jump_interval` (u16)
- `jump_size` (u16)
- `jump_count` (u8)
- `reserved` (u7)
- Node area:
- Jump table at start of node area.
- Node records packed after jump table.

### Node Format
- `prefix_len` (u16)
- `suffix_len` (u16)
- `flags` (u16) (bit0 = leaf, bit1 = end-of-page marker)
- `tid_count` (u16)
- `child_page_id` (u32) if internal
- `key_suffix` bytes (suffix_len)
- `tid_list` bytes if leaf
- Prefix compression:
- Each key stores a prefix length referencing the previous key on the page.
- Key reconstruction: `prev_key[0:prefix_len] + key_suffix`.

### Search
- Traverse from root to leaf using binary search and jump table.
- If key > high key, follow right sibling.

### Insert
- Insert key in sorted order.
- If no space, split page at byte median.
- Promote separator key to parent.
- Update sibling pointers and high keys.

### Delete and GC
- Mark node deleted.
- GC compacts pages and merges underfull pages.

### Concurrency
- Latch coupling with right-sibling B-link semantics.
- Page splits safe for concurrent readers.

## Hash (Canonical)
### Meta Page
- `hash_level`
- `split_bucket`
- `bucket_count`
- `overflow_page_count`
- `load_factor_threshold`

### Bucket Pages
- Store sorted hash codes and TID lists.
- Overflow pages chained when bucket full.

### Insert
- Compute hash and select bucket.
- If bucket full, attempt cleanup of dead tuples.
- If still full, allocate overflow.

### Split
- Linear hashing: split bucket when load factor exceeded.

## GIN (Canonical)
### Structure
- Entry tree: B-tree over keys.
- Posting list: list of TIDs for each key.
- Posting tree: B-tree for large posting lists.
- Pending list: unsorted entries for fast updates.

### Insert
- Extract keys from value.
- Insert key/TID pairs into pending list.
- Pending list flushed to main structure on vacuum, autoanalyze, explicit clean, or size threshold.

### Search
- Find key in entry tree.
- Scan posting list or posting tree.
- Combine postings per query strategy.
- Searches also scan pending list if fastupdate enabled.

## GiST (Canonical)
### Structure
- Balanced tree of bounding keys and downlinks.
- Operator class defines `consistent`, `union`, `penalty`, `picksplit`, `compress`, `decompress`, `same`.

### Insert
- Choose subtree with lowest penalty.
- Split page using `picksplit` when full.

### Search
- Use `consistent` to prune branches.
- If lossy, recheck row at heap.

## SP-GiST (Canonical)
### Structure
- Space-partitioned trees with inner tuples and nodes.
- Operator class defines `config`, `choose`, `picksplit`, `inner_consistent`, `leaf_consistent`.
- `allTheSame` inner tuples are allowed when picksplit cannot separate values.

### Insert
- Use `choose` to pick node or split tuple.
- Use `picksplit` to build new inner tuple when leaf page overflows.

### Search
- Traverse partitions using `inner_consistent` and `leaf_consistent`.

## BRIN (Canonical)
### Structure
- Summary tuples per block range.
- Revmap mapping page ranges to summary tuples.

### Build
- Scan heap and create summary per range.
- Range size determined by `pages_per_range`.

### Maintenance
- Update summaries for new pages in existing ranges.
- Unsummarized ranges are summarized by vacuum or explicit call.
- Autosummarize is off by default and triggers summarization on insert events.

### Search
- Compare query to summary; if consistent, return all pages in range (lossy).

## FULLTEXT (Canonical)
### Structure
- Dictionary of tokens.
- Posting lists of document IDs and positions.
- Ranking uses TF-IDF style scoring.

### Insert
- Tokenize input per parser rules.
- Add tokens to dictionary and posting lists.
- InnoDB-mode emulation applies updates at transaction commit time.

### Search
- Natural language and boolean modes.
- Stopword lists and min token sizes are configurable.

## SPATIAL (Canonical)
### Structure
- R-tree of bounding boxes.

### Insert
- Compute bounding box and insert.

### Search
- Bounding box overlap, with refinement checks.

## Dialect Index Type Matrix
- Firebird 5:
- B-tree only.
- Ascending and descending indexes supported.
- Computed indexes supported.
- Partial indexes supported with WHERE; usable only for exact boolean match or OR/IS NOT NULL cases.
- Indexes cannot be created on BLOB or ARRAY columns.
- Unique indexes allow duplicate NULLs.
- Maximum key length is one quarter of page size.
- Compound indexes allow at most 16 columns.
- Maximum indexes per table depend on page size and column count:
- 4096: 203 (1-col), 145 (2-col), 113 (3-col)
- 8192: 408 (1-col), 291 (2-col), 227 (3-col)
- 16384: 818 (1-col), 584 (2-col), 454 (3-col)
- 32768: 1637 (1-col), 1169 (2-col), 909 (3-col)
- Max indexed string length is key length minus 9 bytes; reduced by charset and collation.
- PostgreSQL 18:
- B-tree, hash, GiST, SP-GiST, GIN, BRIN.
- Hash indexes are equality-only; unique hash indexes are rejected in Alpha.
- GIN uses entry tree and posting list/tree with pending list fastupdate.
- BRIN stores summaries per block range; autosummarize per `BRIN_SPEC` defaults.
- SP-GiST uses operator class functions and supports allTheSame nodes.
- MySQL 8:
- FULLTEXT indexes only on InnoDB or MyISAM and only on CHAR/VARCHAR/TEXT columns.
- All columns in a FULLTEXT index must use the same charset and collation.
- InnoDB FULLTEXT updates are processed at transaction commit time.
- FULLTEXT min and max token lengths are controlled by configuration.
- SPATIAL indexes require NOT NULL spatial columns and use R-tree semantics.
- MySQL B-tree prefix lengths follow engine limits and are specified per column.

## Error Handling
- Index corruption triggers rebuild or error.
- Duplicate key errors for unique indexes.
- Unsupported index type returns explicit error per dialect.

## Persistence and On-Disk Format
- Index page headers defined in section 05.
- Page layout for each index type defined here and implemented in storage layer.
- Index metadata stored in catalog.

## Security and Permissions
- Index creation and drop require schema privileges.

## Configuration
- `index.default_type`
- `index.rebuild_threshold`
- `index.statistics_level`
- `index.fillfactor`
- `index.gin_pending_list_limit`
- `index.brin_pages_per_range`
- `index.brin_autosummarize`
- `index.fulltext_min_token_size`
- `index.fulltext_max_token_size`
- `index.fulltext_stopword_list`

## Compatibility Notes
- Emulated index types MUST map to canonical index structures and preserve semantics; otherwise the parser must reject the index.
- If semantics cannot be preserved, dedicated index implementation is required.

## Test Contract
- See `TEST_CONTRACT.md` for required verification.

## Legacy Mapping
- `docs/specifications_old/V2_PARSER_INDEX_TYPE_COMPLETENESS.md`

## Subsections
- Index Metadata and Catalog
- B-tree Specification
- Hash Specification
- GIN Specification
- GiST Specification
- SP-GiST Specification
- BRIN Specification
- FULLTEXT Specification
- SPATIAL Specification
- Dialect Compatibility Matrix

## Open Questions
- None.
