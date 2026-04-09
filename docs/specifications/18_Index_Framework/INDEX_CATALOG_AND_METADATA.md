# Index Catalog and Metadata

## Purpose
Define catalog tables and metadata required for index creation, build, and maintenance.

## Dependencies
- 24_Catalog_Model_and_Virtual_Overlays
- 18_Index_Framework/INDEX_ARCHITECTURE.md

## Catalog Tables

### `index`
- `index_uuid` (UUID, PK)
- `table_uuid` (UUID, FK `table.table_uuid`)
- `schema_uuid` (UUID, FK `schema.schema_uuid`)
- `index_name` (string, unique per schema)
- `index_type` (enum: BTREE, HASH, GIN, GIST, SPGIST, BRIN, FULLTEXT, SPATIAL, BITMAP, COLUMNSTORE, COLUMNAR, LSM, HNSW, IVF, ART, BLOOM, VECTOR, VECTOR_FLAT, VECTOR_BIN_FLAT, IVF_FLAT, BIN_IVF_FLAT, IVF_PQ, IVF_SQ8, IVF_SQ8_HYBRID, RHNSW_PQ, RHNSW_SQ, ANNOY, NSG, DISKANN, SCANN, GPU_CAGRA, MINHASH_LSH, SPARSE_INVERTED, SPARSE_WAND, TRIE, INVERTED, STL_SORT, NGRAM, CLICKHOUSE_SET, CLICKHOUSE_TOKENBF_V1, CLICKHOUSE_SPARSE_GRAMS, CLICKHOUSE_TEXT, CLICKHOUSE_HYPOTHESIS, CLICKHOUSE_VECTOR_SIMILARITY, MONGODB_2D, MONGODB_2DSPHERE, MONGODB_2DSPHERE_BUCKET, MONGODB_GEO_HAYSTACK, MONGODB_WILDCARD, MONGODB_ENCRYPTED_RANGE, MONGODB_COLUMN, YBGIN, MILVUS_AUTOINDEX, MILVUS_IVF_RABITQ, MILVUS_IVF_HNSW, MILVUS_GPU_IVF_FLAT, MILVUS_GPU_IVF_PQ, MILVUS_GPU_BRUTE_FORCE, NEO4J_LOOKUP, NEO4J_TEXT, NEO4J_RANGE, NEO4J_POINT, NEO4J_VECTOR, CASSANDRA_SASI, CASSANDRA_SAI, REDIS_STRING, REDIS_HASH, REDIS_LIST, REDIS_SET, REDIS_ZSET, REDIS_STREAM, REDIS_BITMAP, REDIS_HLL, REDIS_GEO)
- `physical_family` (string or enum canonical runtime family name)
- `planner_family` (string or enum canonical planner family name)
- `family_mode` (string or enum family variation mode)
- `format_version` (u16)
- `alias_origin` (string, nullable when created through the direct canonical family surface)
- `family_options_version` (u16)
- `lifecycle_model` (string or enum canonical lifecycle identity)
- `metrics_type` (string or enum canonical family metrics contract)
- `metrics_version` (u16)
- `queryability_state` (enum: building, validating, queryable, stale, merging, retiring, failed)
- `unique_flag` (bool)
- `nulls_distinct` (bool)
- `desc_sort_mode` (enum: invert_compare, bytewise_complement)
- `build_mode` (enum: offline, online, concurrent)
- `state` (enum: building, validating, active, invalid, dropping)
- `root_page_id` (u32)
- `meta_page_id` (u32)
- `key_format_version` (u16)
- `fillfactor` (u16, default from configuration)
- `fulltext_ranking_mode_default` (enum: sb_tfidf, mysql_nl, mysql_bool, pg_ts_rank, pg_ts_rank_cd)
- `predicate_sblr` (blob, nullable)
- `created_txid` (u64)
- `created_at` (timestamp)
- `updated_at` (timestamp)

### `index_access_method`
Canonical catalog schema:
- `access_method_uuid` `[sb_dom]cat_index_access_method_uuid` PK
- `method_name` `[sb_dom]cat_identifier`
- `index_type` `[sb_dom]cat_enum_index_type`
- `handler_udr_uuid` `[sb_dom]cat_udr_uuid` nullable
- `supports_unique` `[sb_dom]cat_bool`
- `supports_multicolumn` `[sb_dom]cat_bool`
- `supports_include` `[sb_dom]cat_bool`
- `supports_partial` `[sb_dom]cat_bool`
- `supports_order` `[sb_dom]cat_bool`
- `supports_nulls_order` `[sb_dom]cat_bool`
- `supports_concurrent` `[sb_dom]cat_bool`
- `default_fillfactor` `[sb_dom]cat_uint16`
- `is_system` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`method_name`)

### `index_column`
- `index_uuid` (UUID, FK)
- `position` (u16, 1-based order in index key)
- `column_uuid` (UUID, nullable when `expression_sblr` present)
- `expression_sblr` (blob, nullable)
- `opclass_uuid` (UUID, FK `index_opclass.opclass_uuid`)
- `collation_uuid` (UUID, nullable)
- `sort_order` (enum: ASC, DESC)
- `null_order` (enum: FIRST, LAST)
- `prefix_length` (u16, nullable, only for MySQL-style prefix indexes)
- `is_include` (bool, default false)

### `index_opclass`
- `opclass_uuid` (UUID, PK)
- `name` (string)
- `index_type` (enum)
- `input_type_uuid` (UUID)
- `collation_uuid` (UUID, nullable)
- `owner_schema_uuid` (UUID)

### `index_opclass_fn`
- `opclass_uuid` (UUID, FK)
- `fn_kind` (enum: compare, consistent, union, penalty, picksplit, compress, decompress, same,
  extract_value, extract_query, term_normalize, term_tokenize, distance)
- `fn_uuid` (UUID, FK `function.fn_uuid`)
- `support_number` (u16, default 0 when not applicable).

### `index_stats`
- `index_uuid` (UUID, FK)
- `stats_version` (u32)
- `last_analyze_txid` (u64)
- `row_count_est` (u64)
- `distinct_count_est` (u64)
- `null_frac` (float)
- `histogram_bounds` (blob)
- `most_common_vals` (blob)
- `avg_key_len` (u16)
- `avg_entry_len` (u16)
- `leaf_pages` (u32)
- `height` (u16)
- `clustering_factor` (u64)
- `correlation` (float)
- `bloat_ratio` (float)
- `last_vacuum_txid` (u64)
- `last_reindex_txid` (u64)
- `metrics_last_refresh_xid` (u64)
- `family_metrics_version` (u32)
- `family_metrics_type` (enum or string canonical family metrics packet type)
- `metrics_confidence_class` (enum: unknown, fresh_native, fresh_derived, stale_but_usable, missing_conservative, non_conforming)
- `queryability_state` (enum: queryable, limited, invalid, unknown)
- `family_metrics_payload` (json or text payload for the last published planner packet)

### `index_option`
- `index_uuid` (UUID, FK)
- `option_key` (string)
- `option_value` (string)
- `option_type` (enum: int, float, bool, string, json)
- `updated_at` (timestamp)

### `index_usage`
- `index_uuid` (UUID, FK)
- `scan_count` (u64)
- `tuple_read` (u64)
- `tuple_returned` (u64)
- `index_only_hits` (u64)
- `blocks_read` (u64)
- `blocks_hit` (u64)
- `total_time_ns` (u64)
- `last_used_at` (timestamp)

### `index_contention`
- `index_uuid` (UUID, FK)
- `lock_wait_count` (u64)
- `lock_wait_time_ns` (u64)
- `deadlock_count` (u64)
- `latch_wait_count` (u64)
- `latch_wait_time_ns` (u64)
- `unique_key_conflict_count` (u64)
- `hot_key_count` (u64)

### `index_storage`
- `index_uuid` (UUID, FK)
- `page_count` (u64)
- `bytes_used` (u64)
- `bytes_allocated` (u64)
- `fragmentation_ratio` (float)
- `filespace_uuid` (UUID)

### `index_health`
- `index_uuid` (UUID, FK)
- `last_light_scan_txid` (u64)
- `last_light_scan_at` (timestamp)
- `light_status` (enum: healthy, warning, error)
- `light_error_count` (u32)
- `last_diag_scan_txid` (u64)
- `last_diag_scan_at` (timestamp)
- `diagnostic_status` (enum: healthy, warning, error, corrupt)
- `diagnostic_error_count` (u32)
- `checksum_errors` (u32)
- `order_errors` (u32)
- `pointer_errors` (u32)
- `orphan_pages` (u32)
- `duplicate_keys` (u32)
- `in_memory_errors` (u32)
- `pages_scanned` (u64)
- `bytes_scanned` (u64)

### `index_maintenance`
- `maintenance_id` (u64, PK)
- `index_uuid` (UUID, FK)
- `kind` (enum: rebuild, rebalance, compact, relocate)
- `mode` (enum: offline, online)
- `state` (enum: building_shadow, applying_deltas, swapping, complete, failed)
- `shadow_root_page_id` (u32)
- `shadow_meta_page_id` (u32)
- `target_filespace_uuid` (UUID, nullable)
- `target_fillfactor` (u16, nullable)
- `started_txid` (u64)
- `started_at` (timestamp)
- `updated_at` (timestamp)

### `index_maintenance_delta`
- `maintenance_id` (u64, FK)
- `delta_id` (u64)
- `delta_op` (enum: insert, delete)
- `tid` (TID)
- `commit_txid` (u64)

### `index_build_delta` (used only by online/concurrent build)
- `index_uuid` (UUID, FK)
- `delta_id` (u64)
- `delta_op` (enum: insert, delete, update)
- `tid` (TID)
- `key_bytes` (blob)
- `created_txid` (u64)

## Constraints
- `index.index_name` is unique per schema.
- `index_column.position` is unique per index.
- `index_column.is_include=true` entries must appear after all key segments.
- `index_column.expression_sblr` and `column_uuid` are mutually exclusive.
- `index.build_mode=online|concurrent` is rejected in Alpha builds.
- `index_option` requires unique `(index_uuid, option_key)`.
- Index creation must validate that all required options for the chosen `index_type` are present and within allowed ranges.
- Canonical family fields on the `index` row must be internally consistent with the admitted lowering for `index_type` before the row may publish as active.
- `index_usage`, `index_contention`, and `index_storage` must have exactly one row per `index_uuid`.
- `index_health` must have exactly one row per `index_uuid`.
- `index_maintenance` must have at most one active row per `index_uuid`.

## Storage Notes
- `index_type` remains the admitted named-family identity. `physical_family`
  and `planner_family` explain lowering and costing semantics; they do not
  replace the named family.
- donor-visible emulation families such as `SPATIAL`, `VECTOR`, `COLUMNAR`,
  `MONGODB_COLUMN`, `YBGIN`, `MILVUS_*`, and `CLICKHOUSE_*` must preserve that
  named identity even when `physical_family` routes to a shared substrate.
- `predicate_sblr` and `expression_sblr` store canonical SBLR AST bytes.
- `histogram_bounds` and `most_common_vals` store canonical encoded values from section 14.
- `index_option` stores all index-type-specific options; parsers must validate required options before emitting `IDX_CREATE`.

## Open Decisions
- None. Field changes require updating parsers and catalog overlays.

## Update 2026-03-30: Beta 1 catalog authority closure

The earlier narrow `IndexInfo`-only reading is no longer sufficient for Beta 1.

Beta 1 required catalog authority is:
- `CatalogManager::IndexInfo` plus full persisted canonical family fields carried
  by the authoritative `index` row and its `index_params_oid` TOAST-backed
  parameter payload
- authoritative `index_stats` persistence for queryability, confidence, refresh
  epoch, and family metrics payload
- authoritative operator and maintenance row families for access methods,
  opclasses, usage, contention, health, maintenance, and build-delta state

Shared-backend families follow this rule:
- `index_type` remains the independent named-family identity
- `physical_family` and `planner_family` may collapse to a shared substrate
- `alias_origin` and `family_mode` explain that lowering rather than hiding it

Current code-backed materialization for Beta 1 is:
- `CatalogManager::IndexInfo` as the in-memory canonical projection
- `index_params_oid` as the durable carrier for `physical_family`,
  `planner_family`, `family_mode`, `format_version`, `alias_origin`,
  `family_options_version`, `lifecycle_model`, `metrics_type`,
  `metrics_version`, `queryability_state`, and validated family option payloads
- create/open validation against the admitted `IndexFactory` lowering for the
  row's `index_type`
