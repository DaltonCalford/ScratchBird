# Catalog: Storage Metrics Tables

## Purpose
Define catalog tables for persisted storage statistics and scan reports.

## Enums
- `scan_type`: light, diagnostic, full

## Table: `table_stats`
Columns:
- `table_uuid` UUID
- `row_count_est` u64
- `row_count_live` u64
- `row_count_dead` u64
- `page_count` u64
- `avg_row_len` u32
- `bloat_ratio` f32
- `last_analyze_txid` u64
- `last_vacuum_txid` u64

Primary key:
- `table_uuid`

## Table: `column_stats`
Columns:
- See `CATALOG_TABLE_SCHEMA_CORE_OBJECTS.md` for the canonical column list.

## Table: `index_stats`
Columns:
- Canonical schema is defined in `18_Index_Framework/INDEX_CATALOG_AND_METADATA.md` under `index_stats`.
- This document does not redefine columns; it references the canonical index schema to prevent drift.

Primary key:
- `index_uuid`

## Table: `filespace_stats`
Columns:
- `filespace_id` u32
- `total_pages` u64
- `free_pages` u64
- `dirty_pages` u64
- `read_iops` u64
- `write_iops` u64
- `last_scan_txid` u64

Primary key:
- `filespace_id`

## Table: `scan_reports`
Columns:
- `scan_id` UUID
- `scan_type` enum (scan_type)
- `started_txid` u64
- `completed_txid` u64
- `error_count` u64
- `report_blob` BLOB

Primary key:
- `scan_id`

## Table: `emulated_stat_def`
Columns:
- `stat_uuid` UUID
- `engine` STRING
- `stat_name` STRING
- `stat_unit` STRING
- `stat_type` STRING
- `scope` STRING
- `description` STRING
- `is_required` BOOL

Primary key:
- `stat_uuid`

## Table: `emulated_stat_value`
Columns:
- `stat_uuid` UUID
- `object_uuid` UUID
- `value_i64` INT64
- `value_u64` UINT64
- `value_f64` FLOAT64
- `value_bool` BOOL
- `value_string` STRING
- `value_blob` BLOB
- `updated_txid` UINT64
- `updated_at` TIMESTAMP

Primary key:
- `(stat_uuid, object_uuid)`

## Table: `emulated_stat_map`
Columns:
- `engine` STRING
- `source_name` STRING
- `scope` STRING
- `metric_ref` STRING
- `map_kind` STRING
- `scale_factor` FLOAT64
- `derived_expr_sblr` BLOB

Primary key:
- `(engine, source_name, scope)`
