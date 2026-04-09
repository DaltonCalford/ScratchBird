# Index DDL and Semantics

## Purpose
Define canonical DDL for index operations and dialect mapping rules.

## Dependencies
- 21_V3_Dialect_Surface
- 22_SBLR_Canonical_Model_and_Opcodes
- 18_Index_Framework/INDEX_CATALOG_AND_METADATA.md

## Canonical SBLR Operations
- `IDX_CREATE`
- `IDX_DROP`
- `IDX_RENAME`
- `IDX_SET_OPTIONS`
- `IDX_REBUILD`
- `IDX_ANALYZE`
- `IDX_INVALIDATE`
- `IDX_REBALANCE`
- `IDX_RELOCATE`
- `IDX_LIGHT_SCAN`
- `IDX_DIAGNOSTIC_SCAN`

### `IDX_CREATE` Required Fields
- `index_uuid`
- `table_uuid`
- `index_name`
- `index_type`
- `unique_flag`
- `nulls_distinct`
- `build_mode`
- `key_columns` (list of column_uuid or expression_sblr)
- `include_columns` (list of column_uuid)
- `predicate_sblr` (nullable)
- `options` (explicit key/value list from index type definition; no implicit or unspecified keys)

### `IDX_REBALANCE` Required Fields
- `index_uuid`
- `mode` (enum: offline, online)
- `target_fillfactor`: if omitted, use `index.rebalance.fillfactor_target` (factory default 90, configurable).
- `throttle_ms`: if omitted, use `index.rebalance.throttle_ms` (factory default 0, configurable). Sleeps between page batches.

### `IDX_RELOCATE` Required Fields
- `index_uuid`
- `mode` (enum: offline, online)
- `target_filespace_uuid`
- `max_bytes_per_txn`: if omitted, use `index.relocate.max_bytes_per_txn` (factory default 67108864, configurable).
- `throttle_ms`: if omitted, use `index.relocate.throttle_ms` (factory default 0, configurable).

### `IDX_LIGHT_SCAN` Required Fields
- `index_uuid`
- `sample_pages`: if omitted, use `index.health.light.sample_pages` (factory default 4096, configurable).
- `throttle_ms`: if omitted, use `index.health.light.throttle_ms` (factory default 0, configurable).

### `IDX_DIAGNOSTIC_SCAN` Required Fields
- `index_uuid`
- `throttle_ms`: if omitted, use `index.health.diagnostic.throttle_ms` (factory default 0, configurable).

## Canonical DDL Semantics
- Index names are unique per schema.
- Expression indexes are allowed when expression is immutable or stable per dialect rules.
- Predicate (partial) indexes are allowed when predicate is deterministic.
- `INCLUDE` columns are stored in index payload for covering queries but do not affect ordering.
- `nulls_distinct` defaults to true unless explicitly overridden by dialect.
  - ScratchBird rule: expressions used in indexes must be deterministic and free of side effects.
  - Disallowed in index expressions: volatile functions (random, clock, sequence nextval), subqueries, and non-deterministic UDRs.
- `desc_sort_mode` is permitted only for native ScratchBird DDL.
- `IDX_REBALANCE` and `IDX_RELOCATE` must be supported in `online` mode for all index types.

## Dialect Mapping

### Firebird 5
- `CREATE [UNIQUE] [ASCENDING|DESCENDING] INDEX name ON table (columns)`.
- Computed indexes map to `expression_sblr`.
- Partial indexes map to `predicate_sblr`.
- `ACTIVE` and `INACTIVE` index states map to `state=active|invalid`.
- Firebird does not support `INCLUDE` columns.

### PostgreSQL 18
- `CREATE [UNIQUE] INDEX [CONCURRENTLY] name ON table USING method (columns/opclass)`
- `INCLUDE (columns)` maps to `include_columns`.
- `NULLS NOT DISTINCT` sets `nulls_distinct=false`.
- `WHERE predicate` maps to `predicate_sblr`.
- `WITH (fillfactor=...)` maps to `fillfactor`.
- `CONCURRENTLY` maps to `build_mode=concurrent` and is rejected in Alpha.
- `CREATE UNIQUE INDEX USING hash` is rejected in Alpha.

### MySQL 8
- `CREATE [UNIQUE] INDEX name ON table (columns)` maps to `BTREE` unless `USING` specified.
- `USING HASH` maps to `HASH` only for engines that support hash.
- `FULLTEXT` maps to `FULLTEXT`.
- `SPATIAL` maps to `SPATIAL`.
- Prefix indexes map to `prefix_length`.
- Prefix length is required for `BLOB` and `TEXT` columns.
- MySQL does not support partial predicates; parser must reject `WHERE`.

## Error Handling
- Unsupported index types or options must return explicit error per dialect.
- Unsupported build modes must return explicit error in Alpha.
- REDIS_* index creation must validate table schema against `REDIS_DATA_STRUCTURES_SPEC.md`.

## ScratchBird Native Options
- `WITH (desc_sort_mode = 'invert_compare' | 'bytewise_complement')`
- `WITH (fulltext_ranking_mode = 'sb_tfidf' | 'mysql_nl' | 'mysql_bool' | 'pg_ts_rank' | 'pg_ts_rank_cd')`
- `WITH (cell_semantics = 'space_filling_curve' | 'grid' | 'spherical_geocell' | 'geohash')`
- `WITH (cell_curve = 'zorder' | 'hilbert')`
- `WITH (hilbert_variant = 'skilling_xy' | 'skilling_yx' | 'skilling_xyz' | 'skilling_xzy' | 'skilling_yxz' | 'skilling_yzx' | 'skilling_zxy' | 'skilling_zyx')`
- `WITH (axis_invert_mask = 0..7)` (bit0=x, bit1=y, bit2=z)
- `WITH (bits_per_dim = 1..31)` (2D) or `1..21` (3D)
- Native DDL is permitted to override these defaults at index creation time.
- Emulated dialects MUST NOT override these options; they use dialect defaults.
  - Firebird/PostgreSQL/MySQL emulation default `desc_sort_mode = invert_compare`.
  - For spatial/point indexes, emulated dialects MUST ignore `cell_semantics`, `cell_curve`, and `hilbert_*` options and enforce their dialect defaults.

## Alpha Scope and Deferred Variants
- Alpha implementations MUST expose only features available in the emulated engines.
- Native parser in Alpha exposes the union of emulated index types and their options only.
- Existing detailed variants that are already specified remain authoritative, but any new, non-emulated variants discovered during Alpha are recorded here and rejected until Beta.
- Deferred variants (record only, do not implement in Alpha):
  - `cell_semantics = s2_cell`
  - `cell_semantics = geohash_precision`
  - `hilbert_variant = butz_3d`
  - `hilbert_variant = table_driven_3d`
  - `axis_invert_mask != 0` for emulated point indexes
  - `bits_per_dim` overrides for emulated point indexes

## ScratchBird Native Index Types (Alpha)
- `USING BITMAP`:
  - Options: `container_threshold`, `use_run_container`.
- `USING COLUMNSTORE`:
  - Options: `row_group_size`, `encoding_override`.
- `USING LSM`:
  - Options: `memtable_mb`, `level0_max_sstables`, `level_multiplier`, `block_size`, `bloom_fp_rate`.
- `USING ART`:
  - Options: `prefix_inline_bytes`.
- `USING BLOOM`:
  - Options: `pages_per_range`, `expected_items_per_range`, `false_positive_rate`.
- `USING HNSW`:
  - Options: `m`, `ef_construction`, `ef_search`, `metric`, `vector_dim`.
- `USING IVF`:
  - Options: `nlist`, `nprobe`, `max_iters`, `sample_size`, `epsilon`, `metric`, `vector_dim`.
- `USING VECTOR_FLAT`:
  - Options: `metric`, `vector_dim`, `normalize_vectors`.
- `USING VECTOR_BIN_FLAT`:
  - Options: `metric` (hamming, jaccard, tanimoto), `vector_dim_bits`.
- `USING IVF_FLAT`:
  - Options: `nlist`, `nprobe`, `metric`, `vector_dim`, `training_sample_size`.
- `USING BIN_IVF_FLAT`:
  - Options: `nlist`, `nprobe`, `metric`, `vector_dim_bits`, `training_sample_size`.
- `USING IVF_PQ`:
  - Options: `nlist`, `nprobe`, `m` (subquantizers), `bits_per_code`, `metric`, `vector_dim`, `training_sample_size`.
- `USING IVF_SQ8`:
  - Options: `nlist`, `nprobe`, `metric`, `vector_dim`, `training_sample_size`.
- `USING IVF_SQ8_HYBRID`:
  - Options: `nlist`, `nprobe`, `metric`, `vector_dim`, `training_sample_size`, `gpu_search_threshold`, `gpu_coarse_quantizer`.
- `USING RHNSW_PQ`:
  - Options: `m`, `ef_construction`, `ef_search`, `pq_m`, `pq_bits`, `metric`, `vector_dim`.
- `USING RHNSW_SQ`:
  - Options: `m`, `ef_construction`, `ef_search`, `sq_bits`, `metric`, `vector_dim`.
- `USING ANNOY`:
  - Options: `n_trees`, `search_k`, `leaf_size`, `metric`, `vector_dim`.
- `USING NSG`:
  - Options: `R` (out_degree), `L` (candidate_pool), `C` (build_candidates), `search_L`, `metric`, `vector_dim`.
- `USING DISKANN`:
  - Options: `max_degree`, `build_L`, `search_L`, `beam_width`, `pq_bytes`, `alpha`, `training_sample_size`, `entrypoint_strategy`, `metric`, `vector_dim`, `use_disk_storage`.
- `USING SCANN`:
  - Options: `nlist`, `nprobe`, `reorder_k`, `quantizer` (none, sq8, pq), `pq_m`, `pq_bits`, `training_sample_size`, `centroid_iter`, `seed`, `metric`, `vector_dim`.
- `USING GPU_CAGRA`:
  - Options: `graph_degree`, `intermediate_graph_degree`, `build_iters`, `metric`, `vector_dim`, `gpu_id`, `seed`, `entrypoint_strategy`, `fallback_cpu`.
- `USING MINHASH_LSH`:
  - Options: `num_perm`, `band_count`, `rows_per_band`, `bloom_fp_rate`, `element_bit_width`.
- `USING SPARSE_INVERTED`:
  - Options: `max_df`, `min_df`, `norm` (l1, l2), `topk_default`.
- `USING SPARSE_WAND`:
  - Options: `block_size`, `pivot_strategy` (max, avg), `topk_default`.
- `USING TRIE`:
  - Options: `case_fold`, `max_node_bytes`, `compress_chains`.
- `USING INVERTED`:
  - Options: `analyzer`, `token_min_len`, `token_max_len`, `store_positions`.
- `USING STL_SORT`:
  - Options: `chunk_size`, `block_cache_mb`, `encoding` (plain, delta, bitpack).
- `USING NGRAM`:
  - Options: `min_n`, `max_n`, `case_fold`, `analyzer`.
- `USING MONGODB_2D`:
  - Options: `bits`, `min`, `max`, `bucket_size`.
- `USING MONGODB_2DSPHERE`:
  - Options: `max_cells`, `max_level`, `min_level`, `finest_level`, `coarsest_level`.
- `USING MONGODB_2DSPHERE_BUCKET`:
  - Options: same as `MONGODB_2DSPHERE`, plus `bucket_time_span`.
- `USING NEO4J_POINT`:
  - Options: `srid`, `dims`, `bits_per_dim`, `cell_curve`, `hilbert_variant`, `axis_invert_mask`.
- `USING MONGODB_GEO_HAYSTACK`:
  - Options: `bucket_size`, `secondary_key`.
- `USING MONGODB_WILDCARD`:
  - Options: `include_paths`, `exclude_paths`, `max_path_depth`.
- `USING MONGODB_ENCRYPTED_RANGE`:
  - Options: `range_bits`, `token_levels`, `token_hmac`, `range_bucket_policy`.
- `USING NEO4J_LOOKUP`:
  - Options: `token_kind` (label, reltype), `cache_mb`.
- `USING NEO4J_TEXT`:
  - Options: `analyzer`, `min_token_len`, `max_token_len`, `case_fold`.
- `USING NEO4J_RANGE`:
  - Options: `desc_sort_mode`, `fillfactor`.
- `USING NEO4J_POINT`:
  - Options: `srid`, `dims`, `space_filling_curve` (zorder, hilbert).
- `USING NEO4J_VECTOR`:
  - Options: `metric`, `vector_dim`, `index_impl` (HNSW, RHNSW_PQ, RHNSW_SQ).
- `USING CASSANDRA_SASI`:
  - Options: `mode` (contains, prefix, sparse), `analyzer`, `max_term_len`.
- `USING CASSANDRA_SAI`:
  - Options: `index_kind` (numeric, string, vector), `segment_bytes`, `posting_block_rows`.
- `USING REDIS_STRING`:
  - Options: `max_bytes`, `encoding` (raw, int, embstr), `ttl_policy`.
- `USING REDIS_HASH`:
  - Options: `hash_encoding` (ziplist, hashtable), `ziplist_max_entries`, `ziplist_max_value`, `ttl_policy`.
- `USING REDIS_LIST`:
  - Options: `list_encoding` (quicklist), `quicklist_node_bytes`, `quicklist_compress`, `ttl_policy`.
- `USING REDIS_SET`:
  - Options: `set_encoding` (intset, hashtable), `intset_max_entries`, `ttl_policy`.
- `USING REDIS_ZSET`:
  - Options: `zset_encoding` (ziplist, skiplist), `ziplist_max_entries`, `ziplist_max_value`, `ttl_policy`.
- `USING REDIS_STREAM`:
  - Options: `stream_node_max_entries`, `stream_node_max_bytes`, `ttl_policy`.
- `USING REDIS_BITMAP`:
  - Options: `bitmap_block_bits`, `ttl_policy`.
- `USING REDIS_HLL`:
  - Options: `register_bits`, `sparse_max_bytes`, `ttl_policy`.
- `USING REDIS_GEO`:
  - Options: `geo_encoding` (geohash), `precision_bits`, `ttl_policy`.

## Dialect Exposure
- Emulated dialects must not expose BITMAP, COLUMNSTORE, LSM, ART, BLOOM, vector family types (VECTOR_FLAT, VECTOR_BIN_FLAT, IVF_FLAT, BIN_IVF_FLAT, IVF_PQ, IVF_SQ8, IVF_SQ8_HYBRID, RHNSW_PQ, RHNSW_SQ, ANNOY, NSG, DISKANN, SCANN, GPU_CAGRA), MINHASH_LSH, SPARSE_INVERTED, SPARSE_WAND, TRIE, INVERTED, STL_SORT, NGRAM, MongoDB-derived types, Neo4j-derived types, Cassandra-derived types, or Redis-derived types unless explicitly mapped by the parser.

## Open Decisions
- None. All dialect mappings must be updated if canonical DDL changes.

## Update 2026-03-28: current code-backed DDL boundary

Directly proven DDL and catalog behavior in the current code pass:
- `CatalogManager::createIndex(...)` supports:
  - key columns
  - include columns
  - expression payload storage
  - partial predicate payload storage
  - tablespace-aware root allocation
  - dependency registration
  - immediate active-state publication
- executor create paths call into the catalog create overloads rather than a separate generic `IDX_*` operator framework

Current bounded or unproven boundary:
- the generic `IDX_CREATE`, `IDX_REBALANCE`, `IDX_RELOCATE`, `IDX_LIGHT_SCAN`, and `IDX_DIAGNOSTIC_SCAN` operator surface in this document is broader than the re-proven code in this pass
- broad online and concurrent build semantics are not closed by the current create path
- universal per-family support for `IDX_REBALANCE` and `IDX_RELOCATE` in online mode is not proven here

Current generic operator-routing facts from executor code:
- `GIN` requires specialized query operators
- `FULLTEXT` and related inverted families require specialized text search operators
- `HNSW` and related vector families require k-NN style operators
- `COLUMNSTORE` requires specialized scan operators
- `BRIN` generic point or range access remains block-range oriented and does not directly yield TIDs

The practical section `18` rule is therefore:
- family enum and parser exposure may remain broad
- independently claimed generic DDL and operator semantics must stay bounded to the factory and executor behavior actually proven in code
