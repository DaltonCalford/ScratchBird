# Catalog: Code and Integration Tables

## Purpose
Define canonical on-disk tables for routines, UDRs, emulation metadata, foreign data access, migration history, and dormant/prepared transactions.

## Related Canonical Tables
SBLR runtime and native compilation artifact tables are defined in:
- `CATALOG_TABLE_SCHEMA_SBLR_EXECUTION_ARTIFACTS.md`
Live passthrough and cross-database migration control/audit tables are defined in:
- `CATALOG_TABLE_SCHEMA_LIVE_MIGRATION_AND_PASSTHROUGH.md`
Remote connector metadata, capability, and execution audit tables are defined in:
- `CATALOG_TABLE_SCHEMA_REMOTE_ENGINE_CONNECTOR.md`
ScratchBird parserless cluster-fabric link/session/task tables are defined in:
- `CATALOG_TABLE_SCHEMA_SB_CLUSTER_FABRIC.md`
Security PKI and encryption tables are defined in:
- `CATALOG_TABLE_SCHEMA_SECURITY.md`
Parser capability and transform tables are defined in:
- `CATALOG_TABLE_SCHEMA_PARSER_CAPABILITIES.md`

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Table: `procedure`
Columns:
- `procedure_uuid` `[sb_dom]cat_procedure_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `procedure_name` `[sb_dom]cat_object_name`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `procedure_type` `[sb_dom]cat_enum_procedure_type`
- `is_selectable` `[sb_dom]cat_bool`
- `language` `[sb_dom]cat_enum_procedure_language`
- `sql_security` `[sb_dom]cat_enum_sql_security`
- `name_is_delimited` `[sb_dom]cat_bool`
- `body_redacted` `[sb_dom]cat_bool`
- `deterministic` `[sb_dom]cat_bool`
- `parameter_count` `[sb_dom]cat_uint32`
- `return_type_uuid` `[sb_dom]cat_return_type_uuid` nullable
- `body_sblr_uuid` `[sb_dom]cat_body_sblr_uuid` nullable
- `bytecode_uuid` `[sb_dom]cat_bytecode_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `procedure_param`
Columns:
- `param_uuid` `[sb_dom]cat_param_uuid` PK
- `procedure_uuid` `[sb_dom]cat_procedure_uuid`
- `param_name` `[sb_dom]cat_object_name`
- `param_position` `[sb_dom]cat_uint16`
- `param_mode` `[sb_dom]cat_enum_parameter_mode`
- `data_type_uuid` `[sb_dom]cat_type_uuid`
- `default_expr_sblr_uuid` `[sb_dom]cat_default_expr_sblr_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

## Table: `udr`
Columns:
- `udr_uuid` `[sb_dom]cat_udr_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `udr_name` `[sb_dom]cat_object_name`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `library_path` `[sb_dom]cat_file_path`
- `entry_point` `[sb_dom]cat_identifier`
- `udr_type` `[sb_dom]cat_enum_udr_type`
- `name_is_delimited` `[sb_dom]cat_bool`
- `signature_uuid` `[sb_dom]cat_udr_signature_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `package`
Columns:
- `package_uuid` `[sb_dom]cat_package_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `package_name` `[sb_dom]cat_object_name`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `package_header_uuid` `[sb_dom]cat_package_header_uuid` nullable
- `package_body_uuid` `[sb_dom]cat_package_body_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `name_is_delimited` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

## Table: `package_member`
Columns:
- `member_uuid` `[sb_dom]cat_package_member_uuid` PK
- `package_uuid` `[sb_dom]cat_package_uuid`
- `member_name` `[sb_dom]cat_object_name`
- `member_kind` `[sb_dom]cat_enum_package_member_kind`
- `procedure_uuid` `[sb_dom]cat_procedure_uuid`
- `position` `[sb_dom]cat_uint16`
- `is_public` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`package_uuid`, `member_name`)

## Table: `exception`
Columns:
- `exception_uuid` `[sb_dom]cat_exception_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `exception_name` `[sb_dom]cat_object_name`
- `message_uuid` `[sb_dom]cat_exception_message_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `language`
Columns:
- `language_uuid` `[sb_dom]cat_language_uuid` PK
- `language_name` `[sb_dom]cat_identifier`
- `language_kind` `[sb_dom]cat_enum_language_kind`
- `handler_udr_uuid` `[sb_dom]cat_udr_uuid` nullable
- `inline_handler_udr_uuid` `[sb_dom]cat_udr_uuid` nullable
- `validator_udr_uuid` `[sb_dom]cat_udr_uuid` nullable
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `is_trusted` `[sb_dom]cat_bool`
- `is_system` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`language_name`)

## Table: `event`
Columns:
- `event_uuid` `[sb_dom]cat_event_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `event_name` `[sb_dom]cat_object_name`
- `definer_uuid` `[sb_dom]cat_owner_uuid`
- `status` `[sb_dom]cat_enum_event_status`
- `on_completion` `[sb_dom]cat_enum_event_on_completion`
- `schedule_kind` `[sb_dom]cat_enum_schedule_kind`
- `cron_expr` `[sb_dom]cat_text` nullable
- `interval_ms` `[sb_dom]cat_interval_ms` nullable
- `starts_time` `[sb_dom]cat_timestamp` nullable
- `ends_time` `[sb_dom]cat_timestamp` nullable
- `last_executed_time` `[sb_dom]cat_timestamp` nullable
- `body_sblr_uuid` `[sb_dom]cat_body_sblr_uuid`
- `body_sql_uuid` `[sb_dom]cat_definition_sql_uuid` nullable
- `comment` `[sb_dom]cat_comment_text` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`schema_uuid`, `event_name`)
- If `schedule_kind=CRON` then `cron_expr` must be set.
- If `schedule_kind=EVERY` then `interval_ms` must be set.

## Table: `emulation_type`
Columns:
- `emulation_type_uuid` `[sb_dom]cat_emulation_type_uuid` PK
- `emulation_name` `[sb_dom]cat_identifier`
- `version_major` `[sb_dom]cat_uint8`
- `version_minor` `[sb_dom]cat_uint8`
- `mapping_rules_uuid` `[sb_dom]cat_mapping_rules_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `emulation_server`
Columns:
- `emulation_server_uuid` `[sb_dom]cat_emulation_server_uuid` PK
- `server_name` `[sb_dom]cat_identifier`
- `emulation_type_uuid` `[sb_dom]cat_emulation_type_uuid`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `server_config_uuid` `[sb_dom]cat_server_config_uuid` nullable
- `is_active` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `emulated_database`
Columns:
- `emulated_db_uuid` `[sb_dom]cat_emulated_db_uuid` PK
- `database_name` `[sb_dom]cat_database_name`
- `emulation_server_uuid` `[sb_dom]cat_emulation_server_uuid`
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `db_metadata_uuid` `[sb_dom]cat_db_metadata_uuid` nullable
- `is_active` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `fdw`
Columns:
- `fdw_uuid` `[sb_dom]cat_fdw_uuid` PK
- `fdw_name` `[sb_dom]cat_identifier`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `handler_udr_uuid` `[sb_dom]cat_udr_uuid` nullable
- `validator_udr_uuid` `[sb_dom]cat_udr_uuid` nullable
- `options_uuid` `[sb_dom]cat_fdw_options_uuid` nullable
- `is_system` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`fdw_name`)

## Table: `fdw_server`
Columns:
- `fdw_server_uuid` `[sb_dom]cat_fdw_server_uuid` PK
- `server_name` `[sb_dom]cat_identifier`
- `fdw_uuid` `[sb_dom]cat_fdw_uuid`
- `server_type` `[sb_dom]cat_identifier`
- `host` `[sb_dom]cat_host_name`
- `port` `[sb_dom]cat_port_u16`
- `connection_options_uuid` `[sb_dom]cat_connection_options_uuid` nullable
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `is_active` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `foreign_table`
Columns:
- `foreign_table_uuid` `[sb_dom]cat_foreign_table_uuid` PK
- `schema_uuid` `[sb_dom]cat_schema_uuid`
- `table_name` `[sb_dom]cat_object_name`
- `fdw_server_uuid` `[sb_dom]cat_fdw_server_uuid`
- `remote_schema` `[sb_dom]cat_identifier`
- `remote_table` `[sb_dom]cat_object_name`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `column_mapping_uuid` `[sb_dom]cat_column_mapping_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `name_is_delimited` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

## Table: `fdw_user_mapping`
Columns:
- `mapping_uuid` `[sb_dom]cat_fdw_mapping_uuid` PK
- `user_uuid` `[sb_dom]cat_user_uuid`
- `fdw_server_uuid` `[sb_dom]cat_fdw_server_uuid`
- `remote_user` `[sb_dom]cat_user_name`
- `remote_credentials_uuid` `[sb_dom]cat_remote_credentials_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `server_registry`
Columns:
- `server_uuid` `[sb_dom]cat_server_uuid` PK
- `server_name` `[sb_dom]cat_identifier`
- `host` `[sb_dom]cat_host_name`
- `port` `[sb_dom]cat_port_u16`
- `role` `[sb_dom]cat_enum_server_role`
- `state` `[sb_dom]cat_enum_server_state`
- `last_heartbeat` `[sb_dom]cat_timestamp`
- `last_txid` `[sb_dom]cat_txid`
- `replication_lag_ms` `[sb_dom]cat_duration_ms`
- `cluster_id` `[sb_dom]cat_identifier`
- `server_version` `[sb_dom]cat_identifier`
- `metadata_uuid` `[sb_dom]cat_server_metadata_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `udr_engine`
Columns:
- `engine_uuid` `[sb_dom]cat_udr_engine_uuid` PK
- `engine_name` `[sb_dom]cat_identifier`
- `engine_type` `[sb_dom]cat_enum_udr_engine_type`
- `is_active` `[sb_dom]cat_bool`
- `is_default` `[sb_dom]cat_bool`
- `plugin_path` `[sb_dom]cat_file_path`
- `config_uuid` `[sb_dom]cat_udr_engine_config_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `udr_module`
Columns:
- `module_uuid` `[sb_dom]cat_udr_module_uuid` PK
- `module_name` `[sb_dom]cat_identifier`
- `engine_uuid` `[sb_dom]cat_udr_engine_uuid`
- `library_path` `[sb_dom]cat_file_path`
- `checksum` `[sb_dom]cat_identifier`
- `entry_point` `[sb_dom]cat_identifier`
- `dependencies_uuid` `[sb_dom]cat_udr_module_dependencies_uuid` nullable
- `is_loaded` `[sb_dom]cat_bool`
- `is_validated` `[sb_dom]cat_bool`
- `loaded_count` `[sb_dom]cat_count_u64`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Table: `migration_history`
Columns:
- `history_uuid` `[sb_dom]cat_migration_history_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `table_uuid` `[sb_dom]cat_table_uuid`
- `source_filespace_id` `[sb_dom]cat_uint16`
- `target_filespace_id` `[sb_dom]cat_uint16`
- `source_filespace_uuid` `[sb_dom]cat_filespace_uuid`
- `target_filespace_uuid` `[sb_dom]cat_filespace_uuid`
- `final_phase` `[sb_dom]cat_enum_migration_phase`
- `migration_txid` `[sb_dom]cat_txid`
- `total_pages` `[sb_dom]cat_uint32`
- `pages_copied` `[sb_dom]cat_uint32`
- `start_time` `[sb_dom]cat_timestamp`
- `end_time` `[sb_dom]cat_timestamp`
- `catch_up_iterations` `[sb_dom]cat_uint32`
- `total_bytes_copied` `[sb_dom]cat_bytes_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `source_filespace_id` and `source_filespace_uuid` MUST reference the same filespace.
- `target_filespace_id` and `target_filespace_uuid` MUST reference the same filespace.

## Table: `dormant_transaction`
Columns:
- `dormant_uuid` `[sb_dom]cat_dormant_uuid` PK
- `attachment_uuid` `[sb_dom]cat_attachment_uuid`
- `proc_id` `[sb_dom]cat_uint32`
- `txn_id` `[sb_dom]cat_txid`
- `session_uuid` `[sb_dom]cat_session_uuid`
- `user_uuid` `[sb_dom]cat_user_uuid`
- `session_user_uuid` `[sb_dom]cat_user_uuid`
- `role_uuid` `[sb_dom]cat_role_uuid`
- `isolation_level` `[sb_dom]cat_enum_isolation_level`
- `access_mode` `[sb_dom]cat_enum_dormant_access_mode`
- `wait_mode` `[sb_dom]cat_enum_dormant_wait_mode`
- `autocommit_mode` `[sb_dom]cat_bool`
- `lock_timeout_seconds` `[sb_dom]cat_uint32`
- `current_schema_uuid` `[sb_dom]cat_schema_uuid`
- `session_settings_uuid` `[sb_dom]cat_session_settings_uuid` nullable
- `last_statement_uuid` `[sb_dom]cat_last_statement_uuid` nullable
- `last_statement_hash` `[sb_dom]cat_uint64`
- `last_statement_type` `[sb_dom]cat_enum_dormant_statement_type`
- `last_statement_status` `[sb_dom]cat_enum_dormant_statement_status`
- `state` `[sb_dom]cat_enum_dormant_transaction_state`
- `start_time` `[sb_dom]cat_timestamp`
- `last_activity_time` `[sb_dom]cat_timestamp`
- `dormant_since` `[sb_dom]cat_timestamp`
- `lease_expires_at` `[sb_dom]cat_timestamp`
- `last_statement_time` `[sb_dom]cat_timestamp`
- `last_rows_affected` `[sb_dom]cat_int64`
- `last_error_code` `[sb_dom]cat_uint32`
- `last_sqlstate` `[sb_dom]cat_identifier`
- `server_instance_uuid` `[sb_dom]cat_server_uuid`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `last_sqlstate` must be exactly 5 characters when present.

## Table: `prepared_transaction`
Columns:
- `prepared_uuid` `[sb_dom]cat_prepared_uuid` PK
- `txn_id` `[sb_dom]cat_txid`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `database_uuid` `[sb_dom]cat_database_uuid`
- `gid` `[sb_dom]cat_identifier`
- `prepared_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`
