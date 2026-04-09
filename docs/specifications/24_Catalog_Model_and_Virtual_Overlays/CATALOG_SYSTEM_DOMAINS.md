# Catalog System Domains

## Purpose
Define ScratchBird-optimized system domains for canonical catalog tables. Every catalog column MUST use a catalog system domain or a catalog domain family. Raw base types are forbidden in catalog storage.

## Invariants
- Domain names use the `[sb_dom]cat_*` prefix.
- All catalog domains are native system domains (`system_origin=native`).
- Domain UUIDs are deterministic v5 values defined in `15_Complex_Types/SYSTEM_DOMAIN_UUID_REGISTRY.md`.
- Adding or changing a catalog column requires selecting an existing catalog domain or adding a new one to this spec and the registry.

## Length and Encoding Constants
- `CAT_IDENTIFIER_LEN = 252` characters.
- `CAT_LANGUAGE_LEN = 32` characters.
- `CAT_HOST_LEN = 255` characters.
- `CAT_SERVICE_LEN = 128` characters.
- `CAT_PATH_LEN = 1024` characters.
- All catalog text is UTF-8 and length limits are in characters.

## Fixed Catalog Domains
Each fixed catalog domain is created at database installation time.

| Domain Name | Base Type | Parameters | Constraints | Usage |
| --- | --- | --- | --- | --- |
| `[sb_dom]cat_identifier` | VARCHAR | length_chars=252 | length 1..252; identifier syntax enforced by parser | Default identifier domain when no more specific name domain applies. |
| `[sb_dom]cat_database_name` | VARCHAR | length_chars=252 | length 1..252 | Database names. |
| `[sb_dom]cat_schema_name` | VARCHAR | length_chars=252 | length 1..252 | Schema names. |
| `[sb_dom]cat_object_name` | VARCHAR | length_chars=252 | length 1..252 | Table/view/object names. |
| `[sb_dom]cat_column_name` | VARCHAR | length_chars=252 | length 1..252 | Column names. |
| `[sb_dom]cat_index_name` | VARCHAR | length_chars=252 | length 1..252 | Index names. |
| `[sb_dom]cat_constraint_name` | VARCHAR | length_chars=252 | length 1..252 | Constraint names. |
| `[sb_dom]cat_role_name` | VARCHAR | length_chars=252 | length 1..252 | Role names. |
| `[sb_dom]cat_user_name` | VARCHAR | length_chars=252 | length 1..252 | User names. |
| `[sb_dom]cat_language_code` | VARCHAR | length_chars=32 | ASCII letters/digits/`-`/`_`, length 2..32 | Language tags (BCP-47 style). |
| `[sb_dom]cat_host_name` | VARCHAR | length_chars=255 | ASCII host or IP literal, length 1..255 | Hostnames or IPs. |
| `[sb_dom]cat_service_name` | VARCHAR | length_chars=128 | ASCII letters/digits/`-`/`_`, length 1..128 | Service names. |
| `[sb_dom]cat_policy_name` | VARCHAR | length_chars=252 | length 1..252 | Policy names. |
| `[sb_dom]cat_job_name` | VARCHAR | length_chars=252 | length 1..252 | Job names. |
| `[sb_dom]cat_schema_path` | VARCHAR | length_chars=1024 | UTF-8, no NUL | Canonical schema path strings. |
| `[sb_dom]cat_file_path` | VARCHAR | length_chars=1024 | UTF-8, no NUL | OS file paths stored in catalog/config. |
| `[sb_dom]cat_text` | TEXT |  | none | General catalog text payloads. |
| `[sb_dom]cat_comment_text` | TEXT |  | none | Comments and annotations. |
| `[sb_dom]cat_sql_text` | TEXT |  | none | SQL/DDL source text. |
| `[sb_dom]cat_blob_text` | BLOB_SUB_TYPE_TEXT |  | uses `types.default_character_set` and `types.default_collation` | Large text payloads (BLR/DDL source if stored as text). |
| `[sb_dom]cat_blob_binary` | BLOB |  | none | Generic binary payloads. |
| `[sb_dom]cat_blob_blr` | BLOB |  | none | BLR/SBLR payloads. |
| `[sb_dom]cat_json` | JSON | json_validation=strict | strict JSON validation on write | JSON catalog payloads. |
| `[sb_dom]cat_hash32` | BINARY | length_bytes=32 | length exactly 32 bytes | Hash digests. |
| `[sb_dom]cat_bool` | BOOLEAN |  | none | Boolean flags. |
| `[sb_dom]cat_int16` | INT16 |  | none | Signed 16-bit values. |
| `[sb_dom]cat_int32` | INT32 |  | none | Signed 32-bit values. |
| `[sb_dom]cat_int64` | INT64 |  | none | Signed 64-bit values. |
| `[sb_dom]cat_uint8` | UINT8 |  | none | Unsigned 8-bit values. |
| `[sb_dom]cat_uint16` | UINT16 |  | none | Unsigned 16-bit values. |
| `[sb_dom]cat_uint32` | UINT32 |  | none | Unsigned 32-bit values. |
| `[sb_dom]cat_uint64` | UINT64 |  | none | Unsigned 64-bit values. |
| `[sb_dom]cat_f32` | FLOAT32 |  | none | 32-bit floats. |
| `[sb_dom]cat_f64` | FLOAT64 |  | none | 64-bit floats. |
| `[sb_dom]cat_txid` | UINT64 |  | MGA transaction id; 0 means not set | Transaction identifiers. |
| `[sb_dom]cat_count_u64` | UINT64 |  | >= 0 | Row/page/count metrics. |
| `[sb_dom]cat_bytes_u64` | UINT64 |  | >= 0 | Byte counts. |
| `[sb_dom]cat_percent_u8` | UINT8 |  | 0..100 | Percent values. |
| `[sb_dom]cat_weight_u8` | UINT8 |  | 0..255 | Weighting values. |
| `[sb_dom]cat_priority_u8` | UINT8 |  | 0..255 | Priority values. |
| `[sb_dom]cat_version_u64` | UINT64 |  | monotonically increasing | Version counters. |
| `[sb_dom]cat_port_u16` | UINT16 |  | 0..65535; 0 means unset | Network port values. |
| `[sb_dom]cat_duration_ms` | UINT64 |  | >= 0 | Durations in milliseconds. |
| `[sb_dom]cat_interval_ms` | UINT64 |  | >= 0 | Interval values in milliseconds. |
| `[sb_dom]cat_timestamp` | TIMESTAMP_WITH_ZONE | timezone_mode=offset;timezone_default=0 | offset_seconds must be 0 (UTC) | UTC timestamps. |
| `[sb_dom]cat_uuid` | UUID |  | must be SB UUID (v7) | Generic UUID column named `uuid`. |

## Domain Families
### Catalog UUID Domains
- Rule: any catalog column named `<name>_uuid` MUST use the domain `[sb_dom]cat_<name>_uuid`.
- Base type: UUID.
- Parameters: none.
- Example: column `table_uuid` uses `[sb_dom]cat_table_uuid`.

### Catalog Enum Domains
- Rule: any catalog enum column MUST use domain `[sb_dom]cat_enum_<enum_kind>`.
- Base type: INT16.
- Parameters: `enum_kind=<enum_kind>` (string).
- The allowed labels for each `enum_kind` are defined in the source specs listed below.
- Changing enum labels requires a new `enum_kind` name (for example `cluster_state_v2`) and a catalog migration.

## Enum Kind Registry
The `enum_kind` values below MUST match the label lists in the referenced specifications.

| enum_kind | Defined In |
| --- | --- |
| system_origin | `24_Catalog_Model_and_Virtual_Overlays/SYSTEM_OBJECT_VISIBILITY_AND_INSTALLATION.md` |
| visibility_scope | `24_Catalog_Model_and_Virtual_Overlays/SYSTEM_OBJECT_VISIBILITY_AND_INSTALLATION.md` |
| parser_scope | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_DOMAIN_AND_RESERVED_WORDS.md` |
| domain_kind | `22_SBLR_Canonical_Model_and_Opcodes/SBLR_DOMAIN_PAYLOADS_V3.md` |
| domain_param_type | `22_SBLR_Canonical_Model_and_Opcodes/SBLR_DOMAIN_PAYLOADS_V3.md` |
| domain_constraint_kind | `22_SBLR_Canonical_Model_and_Opcodes/SBLR_DOMAIN_PAYLOADS_V3.md` |
| domain_security_kind | `22_SBLR_Canonical_Model_and_Opcodes/SBLR_DOMAIN_PAYLOADS_V3.md` |
| domain_validation_kind | `22_SBLR_Canonical_Model_and_Opcodes/SBLR_DOMAIN_PAYLOADS_V3.md` |
| domain_integrity_kind | `22_SBLR_Canonical_Model_and_Opcodes/SBLR_DOMAIN_PAYLOADS_V3.md` |
| type_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TYPE_SCHEMA.md` |
| cast_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_TYPE_SCHEMA.md` |
| type_modifier_key | `15_Complex_Types/DOMAIN_EMULATION_PARAMETERS.md` (uses the parameter key names) |
| emulation_engine | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_EMULATION_PROFILE.md` |
| storage_profile | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| table_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| row_uuid_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| system_column_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| object_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| permission_object_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| grantee_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| constraint_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| fk_action | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| fk_match_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| trigger_timing | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| trigger_scope | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| trigger_event | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| trigger_granularity | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| database_trigger_event | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| dependency_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| histogram_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| collation_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| collation_strength | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| toast_storage_strategy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| temp_metadata_scope | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| temp_data_scope | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| temp_on_commit | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| mv_refresh_strategy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| group_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| auth_method | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| home_schema_source | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| home_principal_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| home_scope_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| search_path_scope_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| authkey_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| authkey_usage | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| policy_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| transport | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| transaction_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| artifact_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| queue_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| emulation_engine | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| storage_profile | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| rule_event | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| rule_action_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| package_member_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| event_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| event_on_completion | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| language_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| backup_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| backup_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| partition_strategy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| partition_bound_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| inheritance_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| index_type | `18_Index_Framework/INDEX_CATALOG_AND_METADATA.md` |
| job_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| job_class | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| job_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| job_run_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| job_on_completion | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| schedule_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| procedure_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| procedure_language | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| parameter_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| sql_security | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| udr_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| udr_engine_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| encryption_algorithm | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| kdf_algorithm | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| key_rotation_policy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| key_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| key_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cert_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cert_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| trust_anchor_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| tls_version | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| revocation_source | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| revocation_reason | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| pki_artifact_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| distribution_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| rollover_phase | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| unlock_result | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| server_role | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| server_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_phase | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| isolation_level | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| dormant_statement_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| dormant_statement_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| dormant_transaction_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| dormant_access_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| dormant_wait_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| config_value_type | `24_Catalog_Model_and_Virtual_Overlays/CONFIGURATION_CATALOG_SCHEMA.md` |
| config_scope | `24_Catalog_Model_and_Virtual_Overlays/CONFIGURATION_CATALOG_SCHEMA.md` |
| config_source | `24_Catalog_Model_and_Virtual_Overlays/CONFIGURATION_CATALOG_SCHEMA.md` |
| stat_engine | `24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md` |
| stat_type | `24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md` |
| stat_scope | `24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md` |
| stat_map_kind | `24_Catalog_Model_and_Virtual_Overlays/EMULATED_STATS_MAPPING.md` |
| scan_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_STORAGE_METRICS.md` |
| emulation_engine | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cluster_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cluster_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| consensus_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| node_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| node_role | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| service_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| service_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| shard_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| shard_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replica_role | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replica_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| shard_key_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| shard_range_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| range_order | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| consistency_level | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| failover_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| rebalance_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| hash_function | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_source_engine | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_runtime_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_compare_policy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_write_policy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_return_source | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_cursor_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_object_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_apply_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_compare_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_event_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| migration_error_class | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| object_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| throttle_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| shard_policy_param_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| workload_match_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| route_target_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| admission_reject_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| admission_target_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cluster_policy_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| failure_detector_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| alert_rule_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| alert_severity | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| alert_target_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| alert_route_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| alert_event_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| alert_silence_scope | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| partition_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| healing_trigger_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| healing_action_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| healing_param_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| healing_run_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| healing_step_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| job_param_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| job_group | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| token_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| lifecycle_event_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_range_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| olap_compression | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| olap_tier | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| olap_ingest_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_status | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_source_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_agg_function | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_null_handling | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_materialization_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_refresh_mode | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_job_type | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| cube_job_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| subscription_table_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_direction | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_channel_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_member_role | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_cursor_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_txn_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_retry_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_ddl_policy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_conflict_policy | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_conflict_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_resolution_state | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |
| replication_event_kind | `24_Catalog_Model_and_Virtual_Overlays/CATALOG_ENUMS.md` |

## Mapping Rules for Catalog Specs
- Columns typed as `string` or `STRING` MUST map to a catalog domain, not a raw type.
- Column names determine the domain when a more specific match exists.
- Pattern matching is case-insensitive and uses full column names.

| Column Name or Suffix | Domain |
| --- | --- |
| database_name | `[sb_dom]cat_database_name` |
| schema_name | `[sb_dom]cat_schema_name` |
| table_name, view_name, object_name | `[sb_dom]cat_object_name` |
| column_name | `[sb_dom]cat_column_name` |
| index_name | `[sb_dom]cat_index_name` |
| constraint_name | `[sb_dom]cat_constraint_name` |
| role_name | `[sb_dom]cat_role_name` |
| user_name | `[sb_dom]cat_user_name` |
| language_code | `[sb_dom]cat_language_code` |
| host, host_name | `[sb_dom]cat_host_name` |
| service_name | `[sb_dom]cat_service_name` |
| policy_name | `[sb_dom]cat_policy_name` |
| job_name | `[sb_dom]cat_job_name` |
| job_type_name | `[sb_dom]cat_job_name` |
| schema_path | `[sb_dom]cat_schema_path` |
| file_path, path | `[sb_dom]cat_file_path` |
| description, comment, reason, message | `[sb_dom]cat_text` or `[sb_dom]cat_comment_text` (use comment_text for user-facing comments) |
| sql_text, source_text, definition | `[sb_dom]cat_sql_text` |
| *_sblr, *_blr | `[sb_dom]cat_blob_blr` |
| *_blob, blob | `[sb_dom]cat_blob_binary` |
| *_json, json | `[sb_dom]cat_json` |
| normalization_evidence_hash, clause_order_checksum, hash_sha256 | `[sb_dom]cat_hash32` |
| sblr_checksum, plan_checksum | `[sb_dom]cat_uint64` |

## Creation Rules
- All fixed catalog domains are created at database installation.
- All `_uuid` columns create their own `[sb_dom]cat_<name>_uuid` domains during catalog bootstrap.
- All enum columns create `[sb_dom]cat_enum_<enum_kind>` domains during catalog bootstrap.
- Every catalog domain is stored in `domain` with `is_system=true` and `system_origin=native`.

## Test Contract
- Every catalog column maps to a catalog system domain.
- Every catalog `_uuid` column has a matching `[sb_dom]cat_<name>_uuid` domain.
- `cat_percent_u8` rejects values > 100.
- `cat_port_u16` rejects values > 65535.
- `cat_timestamp` rejects non-UTC offsets.
- `cat_json` rejects invalid JSON when `json_validation=strict`.

## Open Questions
- None.
