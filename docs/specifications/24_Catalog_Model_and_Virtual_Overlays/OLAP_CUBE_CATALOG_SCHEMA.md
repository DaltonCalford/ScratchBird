# Catalog: OLAP and Cube Schema

## Purpose
Define canonical catalog tables required for OLAP processing nodes and cube support.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `olap_watermark`
Columns:
- `watermark_uuid` `[sb_dom]cat_olap_watermark_uuid` PK
- `table_uuid` `[sb_dom]cat_table_uuid`
- `last_ingested_txid` `[sb_dom]cat_txid`
- `last_ingested_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`table_uuid`)

## Table: `olap_partition`
Columns:
- `partition_uuid` `[sb_dom]cat_olap_partition_uuid` PK
- `table_uuid` `[sb_dom]cat_table_uuid`
- `shard_uuid` `[sb_dom]cat_shard_uuid` nullable
- `range_kind` `[sb_dom]cat_enum_cube_range_kind`
- `range_min` `[sb_dom]cat_blob_binary` nullable
- `range_max` `[sb_dom]cat_blob_binary` nullable
- `row_count` `[sb_dom]cat_count_u64`
- `size_bytes` `[sb_dom]cat_bytes_u64`
- `compression` `[sb_dom]cat_enum_olap_compression`
- `tier` `[sb_dom]cat_enum_olap_tier`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `olap_segment`
Columns:
- `segment_uuid` `[sb_dom]cat_olap_segment_uuid` PK
- `partition_uuid` `[sb_dom]cat_olap_partition_uuid`
- `segment_index` `[sb_dom]cat_uint32`
- `row_count` `[sb_dom]cat_count_u64`
- `size_bytes` `[sb_dom]cat_bytes_u64`
- `min_key` `[sb_dom]cat_blob_binary` nullable
- `max_key` `[sb_dom]cat_blob_binary` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`partition_uuid`, `segment_index`)

## Table: `olap_ingest_log`
Columns:
- `batch_uuid` `[sb_dom]cat_olap_batch_uuid` PK
- `table_uuid` `[sb_dom]cat_table_uuid`
- `min_txid` `[sb_dom]cat_txid`
- `max_txid` `[sb_dom]cat_txid`
- `row_count` `[sb_dom]cat_count_u64`
- `size_bytes` `[sb_dom]cat_bytes_u64`
- `ingest_state` `[sb_dom]cat_enum_olap_ingest_state`
- `created_time` `[sb_dom]cat_timestamp`
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`table_uuid`, `min_txid`, `max_txid`)

## Table: `cube`
Columns:
- `cube_uuid` `[sb_dom]cat_cube_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `cube_name` `[sb_dom]cat_identifier`
- `base_table_uuid` `[sb_dom]cat_table_uuid`
- `status` `[sb_dom]cat_enum_cube_status`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_uuid`, `cube_name`)

## Table: `cube_dimension`
Columns:
- `dimension_uuid` `[sb_dom]cat_cube_dimension_uuid` PK
- `cube_uuid` `[sb_dom]cat_cube_uuid`
- `dimension_name` `[sb_dom]cat_identifier`
- `source_kind` `[sb_dom]cat_enum_cube_source_kind`
- `source_column_uuid` `[sb_dom]cat_column_uuid` nullable
- `source_expr_sblr_uuid` `[sb_dom]cat_cube_source_expr_uuid` nullable
- `data_type_uuid` `[sb_dom]cat_type_uuid`
- `is_time_dimension` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- Exactly one of `source_column_uuid` or `source_expr_sblr_uuid` must be non-null.

## Table: `cube_level`
Columns:
- `level_uuid` `[sb_dom]cat_cube_level_uuid` PK
- `dimension_uuid` `[sb_dom]cat_cube_dimension_uuid`
- `level_name` `[sb_dom]cat_identifier`
- `key_expr_sblr_uuid` `[sb_dom]cat_cube_level_key_expr_uuid`
- `label_expr_sblr_uuid` `[sb_dom]cat_cube_level_label_expr_uuid` nullable
- `sort_expr_sblr_uuid` `[sb_dom]cat_cube_level_sort_expr_uuid` nullable
- `level_order` `[sb_dom]cat_uint16`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`dimension_uuid`, `level_name`)

## Table: `cube_hierarchy`
Columns:
- `hierarchy_uuid` `[sb_dom]cat_cube_hierarchy_uuid` PK
- `dimension_uuid` `[sb_dom]cat_cube_dimension_uuid`
- `hierarchy_name` `[sb_dom]cat_identifier`
- `is_default` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`dimension_uuid`, `hierarchy_name`)

## Table: `cube_hierarchy_level`
Columns:
- `hierarchy_level_uuid` `[sb_dom]cat_cube_hierarchy_level_uuid` PK
- `hierarchy_uuid` `[sb_dom]cat_cube_hierarchy_uuid`
- `level_uuid` `[sb_dom]cat_cube_level_uuid`
- `position` `[sb_dom]cat_uint16`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`hierarchy_uuid`, `position`)

## Table: `cube_measure`
Columns:
- `measure_uuid` `[sb_dom]cat_cube_measure_uuid` PK
- `cube_uuid` `[sb_dom]cat_cube_uuid`
- `measure_name` `[sb_dom]cat_identifier`
- `agg_function` `[sb_dom]cat_enum_cube_agg_function`
- `source_expr_sblr_uuid` `[sb_dom]cat_cube_measure_expr_uuid`
- `data_type_uuid` `[sb_dom]cat_type_uuid`
- `null_handling` `[sb_dom]cat_enum_cube_null_handling`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`cube_uuid`, `measure_name`)

## Table: `cube_materialization`
Columns:
- `materialization_uuid` `[sb_dom]cat_cube_materialization_uuid` PK
- `cube_uuid` `[sb_dom]cat_cube_uuid`
- `storage_table_uuid` `[sb_dom]cat_table_uuid`
- `dimension_set_hash` `[sb_dom]cat_hash32`
- `measure_set_hash` `[sb_dom]cat_hash32`
- `policy_group_hash` `[sb_dom]cat_hash32` nullable
- `state` `[sb_dom]cat_enum_cube_materialization_state`
- `row_count` `[sb_dom]cat_count_u64`
- `size_bytes` `[sb_dom]cat_bytes_u64`
- `last_refresh_time` `[sb_dom]cat_timestamp` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `cube_refresh_policy`
Columns:
- `policy_uuid` `[sb_dom]cat_cube_refresh_policy_uuid` PK
- `cube_uuid` `[sb_dom]cat_cube_uuid`
- `refresh_mode` `[sb_dom]cat_enum_cube_refresh_mode`
- `interval_ms` `[sb_dom]cat_interval_ms` nullable
- `watermark_column_uuid` `[sb_dom]cat_column_uuid` nullable
- `max_staleness_ms` `[sb_dom]cat_interval_ms` nullable
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `cube_job`
Columns:
- `job_uuid` `[sb_dom]cat_cube_job_uuid` PK
- `cube_uuid` `[sb_dom]cat_cube_uuid`
- `job_type` `[sb_dom]cat_enum_cube_job_type`
- `state` `[sb_dom]cat_enum_cube_job_state`
- `created_time` `[sb_dom]cat_timestamp`
- `started_time` `[sb_dom]cat_timestamp` nullable
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `error_code` `[sb_dom]cat_identifier` nullable
- `error_message` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

## Table: `cube_job_step`
Columns:
- `step_uuid` `[sb_dom]cat_cube_job_step_uuid` PK
- `job_uuid` `[sb_dom]cat_cube_job_uuid`
- `step_index` `[sb_dom]cat_uint16`
- `step_name` `[sb_dom]cat_identifier`
- `state` `[sb_dom]cat_enum_cube_job_state`
- `started_time` `[sb_dom]cat_timestamp` nullable
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`job_uuid`, `step_index`)

## Table: `cube_stats`
Columns:
- `cube_uuid` `[sb_dom]cat_cube_uuid` PK
- `row_count` `[sb_dom]cat_count_u64`
- `size_bytes` `[sb_dom]cat_bytes_u64`
- `last_refresh_time` `[sb_dom]cat_timestamp`
- `avg_query_latency_ms` `[sb_dom]cat_duration_ms`
- `cache_hit_rate` `[sb_dom]cat_f32`
- `is_valid` `[sb_dom]cat_bool`

## Constraints
- Cube materialization storage tables must be marked `is_system=true` in `table`.
- If `policy_group_hash` is not null, it must match the session policy group hash.

## Test Contract
- Cube definitions can be created without changing on disk format.
- Cube materialization tables appear in catalog and are queryable.
- Refresh policy transitions materializations to active state.
