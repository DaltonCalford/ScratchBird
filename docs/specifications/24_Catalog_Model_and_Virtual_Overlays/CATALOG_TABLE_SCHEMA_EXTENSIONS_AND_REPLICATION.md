# Catalog: Extensions and Replication Tables

## Purpose
Define canonical catalog tables required for extensions, publications, and subscriptions.

Runtime replication execution, cursor tracking, conflict handling, and split-brain controls are defined in:
- `CATALOG_TABLE_SCHEMA_REPLICATION_RUNTIME_AND_CONFLICT_RESOLUTION.md`

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.

## Table: `extension`
Columns:
- `extension_uuid` `[sb_dom]cat_extension_uuid` PK
- `extension_name` `[sb_dom]cat_identifier`
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `version` `[sb_dom]cat_identifier`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `is_relocatable` `[sb_dom]cat_bool`
- `config_uuid` `[sb_dom]cat_extension_config_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`extension_name`)

## Table: `publication`
Columns:
- `publication_uuid` `[sb_dom]cat_publication_uuid` PK
- `publication_name` `[sb_dom]cat_identifier`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `publish_insert` `[sb_dom]cat_bool`
- `publish_update` `[sb_dom]cat_bool`
- `publish_delete` `[sb_dom]cat_bool`
- `publish_truncate` `[sb_dom]cat_bool`
- `publish_via_partition_root` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`publication_name`)

## Table: `publication_table`
Columns:
- `publication_table_uuid` `[sb_dom]cat_publication_table_uuid` PK
- `publication_uuid` `[sb_dom]cat_publication_uuid`
- `table_uuid` `[sb_dom]cat_table_uuid`
- `column_list_uuid` `[sb_dom]cat_publication_columns_uuid` nullable
- `where_expr_sblr_uuid` `[sb_dom]cat_publication_where_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`publication_uuid`, `table_uuid`)

## Table: `publication_schema`
Columns:
- `publication_schema_uuid` `[sb_dom]cat_publication_schema_uuid` PK
- `publication_uuid` `[sb_dom]cat_publication_uuid`
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`publication_uuid`, `schema_uuid`)

## Table: `subscription`
Columns:
- `subscription_uuid` `[sb_dom]cat_subscription_uuid` PK
- `subscription_name` `[sb_dom]cat_identifier`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `connection_info_uuid` `[sb_dom]cat_connection_info_uuid` nullable
- `enabled` `[sb_dom]cat_bool`
- `slot_name` `[sb_dom]cat_identifier` nullable
- `sync_commit` `[sb_dom]cat_bool`
- `copy_data` `[sb_dom]cat_bool`
- `create_slot` `[sb_dom]cat_bool`
- `refresh_on_start` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`subscription_name`)

Notes:
- `connection_info_uuid` stores a JSON payload (hostname, port, user, ssl options, publication list).

## Table: `subscription_table`
Columns:
- `subscription_table_uuid` `[sb_dom]cat_subscription_table_uuid` PK
- `subscription_uuid` `[sb_dom]cat_subscription_uuid`
- `table_uuid` `[sb_dom]cat_table_uuid`
- `state` `[sb_dom]cat_enum_subscription_table_state`
- `last_error` `[sb_dom]cat_text` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`subscription_uuid`, `table_uuid`)
