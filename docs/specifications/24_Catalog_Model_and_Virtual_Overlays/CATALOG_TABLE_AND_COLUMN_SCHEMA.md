# Catalog: Table and Column Schema

## Purpose
Define canonical catalog tables for table and column metadata, including row UUID surfacing rules.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values (see `11_TOAST_and_LOB_Storage`).
- `*_name` columns are cached copies; authoritative names live in `object_name`.

## Table: `table`
Columns:
- `table_uuid` `[sb_dom]cat_table_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid` FK -> `schema.schema_uuid`
- `table_name` `[sb_dom]cat_object_name`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `root_gpid` `[sb_dom]cat_uint64`
- `column_count` `[sb_dom]cat_uint16`
- `row_count_est` `[sb_dom]cat_count_u64`
- `table_kind` `[sb_dom]cat_enum_table_kind`
- `storage_profile` `[sb_dom]cat_enum_storage_profile`
- `filespace_uuid` `[sb_dom]cat_filespace_uuid`
- `has_toast` `[sb_dom]cat_bool`
- `toast_table_uuid` `[sb_dom]cat_table_uuid` nullable
- `rls_enabled` `[sb_dom]cat_bool`
- `rls_forced` `[sb_dom]cat_bool`
- `temp_metadata_scope` `[sb_dom]cat_enum_temp_metadata_scope`
- `temp_data_scope` `[sb_dom]cat_enum_temp_data_scope`
- `temp_on_commit` `[sb_dom]cat_enum_temp_on_commit`
- `temp_flags` `[sb_dom]cat_uint8`
- `name_is_delimited` `[sb_dom]cat_bool`
- `default_charset_uuid` `[sb_dom]cat_charset_uuid`
- `default_collation_uuid` `[sb_dom]cat_collation_uuid`
- `storage_params_uuid` `[sb_dom]cat_storage_params_uuid` nullable
- `creating_session_uuid` `[sb_dom]cat_session_uuid` nullable
- `creating_txid` `[sb_dom]cat_txid`
- `temp_parent_table_uuid` `[sb_dom]cat_table_uuid` nullable
- `temp_schema_uuid` `[sb_dom]cat_schema_uuid` nullable
- `row_uuid_mode` `[sb_dom]cat_enum_row_uuid_mode`
- `row_uuid_column_uuid` `[sb_dom]cat_column_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `policy_epoch` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Indexes:
- PK(`table_uuid`)
- INDEX(`schema_uuid`)
- INDEX(`schema_uuid`, `table_name`)
- INDEX(`row_uuid_column_uuid`)

Constraints:
- `table_name` must equal the default-language name in `object_name` for `table_uuid`.
- When `has_toast=true`, `toast_table_uuid` MUST reference a table with `table_kind=TOAST`.
- When `row_uuid_mode` is not `INTERNAL_ONLY`, `row_uuid_column_uuid` MUST be set.

## Table: `column`
Columns:
- `column_uuid` `[sb_dom]cat_column_uuid` PK
- `table_uuid` `[sb_dom]cat_table_uuid` FK -> `table.table_uuid`
- `column_name` `[sb_dom]cat_column_name`
- `ordinal` `[sb_dom]cat_uint16`
- `type_uuid` `[sb_dom]cat_type_uuid`
- `domain_uuid` `[sb_dom]cat_domain_uuid` nullable
- `type_precision` `[sb_dom]cat_uint32`
- `type_scale` `[sb_dom]cat_uint32`
- `type_length` `[sb_dom]cat_uint32`
- `vector_dim` `[sb_dom]cat_uint32`
- `is_array` `[sb_dom]cat_bool`
- `array_size` `[sb_dom]cat_uint32`
- `array_dim_list_uuid` `[sb_dom]cat_array_dim_list_uuid` nullable
- `is_nullable` `[sb_dom]cat_bool`
- `has_default` `[sb_dom]cat_bool`
- `default_expr_sblr_uuid` `[sb_dom]cat_default_expr_sblr_uuid` nullable
- `default_value_text` `[sb_dom]cat_sql_text` nullable
- `check_expr_sblr_uuid` `[sb_dom]cat_check_expr_sblr_uuid` nullable
- `is_primary_key` `[sb_dom]cat_bool`
- `is_unique` `[sb_dom]cat_bool`
- `is_foreign_key` `[sb_dom]cat_bool`
- `is_generated` `[sb_dom]cat_bool`
- `storage_strategy` `[sb_dom]cat_enum_toast_storage_strategy`
- `with_timezone` `[sb_dom]cat_bool`
- `name_is_delimited` `[sb_dom]cat_bool`
- `charset_uuid` `[sb_dom]cat_charset_uuid`
- `timezone_uuid` `[sb_dom]cat_timezone_uuid`
- `collation_uuid` `[sb_dom]cat_collation_uuid`
- `is_row_uuid` `[sb_dom]cat_bool`
- `is_hidden` `[sb_dom]cat_bool`
- `system_column_kind` `[sb_dom]cat_enum_system_column_kind`
- `is_system` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Indexes:
- PK(`column_uuid`)
- INDEX(`table_uuid`)
- UNIQUE(`table_uuid`, `ordinal`)
- UNIQUE(`table_uuid`, `column_name`)

Constraints:
- When `domain_uuid` is set, `type_uuid` MUST equal the domain base type.
- When `is_row_uuid=true`, `type_uuid` MUST be UUID and column values are read-only.
- When `system_column_kind` is not `NONE`, `is_hidden` MUST be true.

## Row UUID Surface Rules
- Every row has an internal `row_uuid` stored in the record header.
- If a table defines a UUID column that is PRIMARY KEY or UNIQUE and `is_row_uuid=true`, the column value is derived from `row_uuid` and MUST NOT be stored in payload.
- `row_uuid_mode` MUST be set to `SURFACED_PK` or `SURFACED_UNIQUE` when `row_uuid_column_uuid` is populated.
- Columns with `is_row_uuid=true` are read-only; attempts to insert or update a value MUST raise `ROW_UUID_READ_ONLY`.

### Auto-Surface Rule (Native Parser)
- If a table declares a single-column PRIMARY KEY or UNIQUE constraint on a UUID-typed column, the native parser MUST set `is_row_uuid=true` and `row_uuid_mode` accordingly.
- This optimization is mandatory in Alpha and has no override unless explicitly defined in a future spec revision.

## System Column Rules
- Every table implicitly exposes the following hidden system columns:
- `[sb_col]row_uuid` (UUID) from record header `row_uuid`.
- `[sb_col]last_edit_txid` (UINT64) derived from record header `create_txid` for visible rows and `delete_txid` for tombstones when accessed with system visibility.
- System columns are not stored in payload and do not appear in `SELECT *` unless `SHOW SYSTEM` is enabled or explicitly referenced by name.
- System columns MUST be marked in catalog with `is_hidden=true` and `system_column_kind` set appropriately when exposed via catalog views.

## Notes
- `array_dim_list_uuid` stores a packed list of dimension sizes when `array_size=0` and the array is multi-dimensional.
- Emulated parsers may gate row UUID surfacing if their dialect requires explicit UUID storage.
