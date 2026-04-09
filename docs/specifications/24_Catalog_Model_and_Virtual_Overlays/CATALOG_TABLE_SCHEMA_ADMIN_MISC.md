# Catalog: Admin, Auth Mapping, and Backup Tables

## Purpose
Define canonical catalog tables for authentication mappings, database creation metadata, and backup history.

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.

## Table: `auth_mapping`
Columns:
- `mapping_uuid` `[sb_dom]cat_auth_mapping_uuid` PK
- `auth_method` `[sb_dom]cat_enum_auth_method`
- `auth_source` `[sb_dom]cat_identifier`
- `external_subject` `[sb_dom]cat_identifier`
- `external_group` `[sb_dom]cat_identifier` nullable
- `database_uuid` `[sb_dom]cat_database_uuid` nullable
- `user_uuid` `[sb_dom]cat_user_uuid` nullable
- `role_uuid` `[sb_dom]cat_role_uuid` nullable
- `group_uuid` `[sb_dom]cat_group_uuid` nullable
- `priority` `[sb_dom]cat_priority_u8`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- At least one of `user_uuid`, `role_uuid`, or `group_uuid` must be non-null.
- UNIQUE(`auth_method`, `auth_source`, `external_subject`, `database_uuid`)

## Table: `db_creator`
Columns:
- `creator_uuid` `[sb_dom]cat_db_creator_uuid` PK
- `database_uuid` `[sb_dom]cat_database_uuid`
- `created_by_user_uuid` `[sb_dom]cat_user_uuid` nullable
- `created_by_role_uuid` `[sb_dom]cat_role_uuid` nullable
- `creator_host` `[sb_dom]cat_host_name` nullable
- `creator_app` `[sb_dom]cat_identifier` nullable
- `tool_name` `[sb_dom]cat_identifier` nullable
- `tool_version` `[sb_dom]cat_identifier` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`database_uuid`)

## Table: `backup_history`
Columns:
- `backup_uuid` `[sb_dom]cat_backup_uuid` PK
- `database_uuid` `[sb_dom]cat_database_uuid`
- `backup_kind` `[sb_dom]cat_enum_backup_kind`
- `backup_status` `[sb_dom]cat_enum_backup_status`
- `storage_profile` `[sb_dom]cat_identifier` nullable
- `storage_uri` `[sb_dom]cat_text` nullable
- `size_bytes` `[sb_dom]cat_bytes_u64` nullable
- `checksum` `[sb_dom]cat_hash32` nullable
- `started_time` `[sb_dom]cat_timestamp`
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `created_by_user_uuid` `[sb_dom]cat_user_uuid` nullable
- `error_message` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- If `backup_status=SUCCESS` then `completed_time` must be non-null.
- If `backup_status=FAILED` then `error_message` must be non-null.

Notes:
- `storage_uri` stores the normalized absolute path of the persisted backup artifact.
- `backup_history` is the authoritative engine catalog for lifecycle status and
  artifact location. The current non-cluster backup lane pairs it with a
  versioned local backup catalog file (`.scratchbird_backup_catalog.sbcat`) for
  parent-chain metadata and retention policy records until later backup
  execution/API tickets expose the broader operator surface.
- The runtime state progression is `STARTED` -> optional `RUNNING` -> terminal
  `SUCCESS`, `FAILED`, or `CANCELLED`.
