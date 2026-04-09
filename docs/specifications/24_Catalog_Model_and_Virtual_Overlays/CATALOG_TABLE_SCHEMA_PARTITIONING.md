# Catalog: Partitioning and Inheritance Tables

## Purpose
Define canonical catalog tables for partitioned tables and inheritance.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `partitioned_table`
Columns:
- `partitioned_table_uuid` `[sb_dom]cat_partitioned_table_uuid` PK
- `table_uuid` `[sb_dom]cat_table_uuid`
- `strategy` `[sb_dom]cat_enum_partition_strategy`
- `key_columns_uuid` `[sb_dom]cat_partition_key_columns_uuid` nullable
- `key_expr_sblr_uuid` `[sb_dom]cat_partition_key_expr_uuid` nullable
- `partition_count` `[sb_dom]cat_uint32` nullable
- `default_partition_uuid` `[sb_dom]cat_partition_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`table_uuid`)
- Exactly one of `key_columns_uuid` or `key_expr_sblr_uuid` must be non-null.

## Table: `partition`
Columns:
- `partition_uuid` `[sb_dom]cat_partition_uuid` PK
- `parent_table_uuid` `[sb_dom]cat_table_uuid`
- `partition_table_uuid` `[sb_dom]cat_table_uuid`
- `partition_name` `[sb_dom]cat_identifier`
- `bound_kind` `[sb_dom]cat_enum_partition_bound_kind`
- `range_min_bytes` `[sb_dom]cat_blob_binary` nullable
- `range_max_bytes` `[sb_dom]cat_blob_binary` nullable
- `list_values_uuid` `[sb_dom]cat_partition_list_uuid` nullable
- `hash_modulus` `[sb_dom]cat_uint32` nullable
- `hash_remainder` `[sb_dom]cat_uint32` nullable
- `bound_expr_sblr_uuid` `[sb_dom]cat_partition_bound_expr_uuid` nullable
- `is_default` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parent_table_uuid`, `partition_name`)
- If `bound_kind=RANGE` then `range_min_bytes` and `range_max_bytes` must be non-null.
- If `bound_kind=LIST` then `list_values_uuid` must be non-null.
- If `bound_kind=HASH` then `hash_modulus` and `hash_remainder` must be non-null.
- If `is_default=true` then `bound_kind=DEFAULT`.

## Table: `table_inheritance`
Columns:
- `inheritance_uuid` `[sb_dom]cat_inheritance_uuid` PK
- `parent_table_uuid` `[sb_dom]cat_table_uuid`
- `child_table_uuid` `[sb_dom]cat_table_uuid`
- `inheritance_kind` `[sb_dom]cat_enum_inheritance_kind`
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`parent_table_uuid`, `child_table_uuid`)
