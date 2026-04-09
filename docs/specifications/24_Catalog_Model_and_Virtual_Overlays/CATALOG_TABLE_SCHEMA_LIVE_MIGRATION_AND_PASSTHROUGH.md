# Catalog: Live Migration and Passthrough Tables

## Purpose
Define canonical persisted catalog tables for passthrough query routing and live cross-database migration (legacy engines to ScratchBird emulated schemas) with deterministic state and audit evidence.

## Scope
- Source engines: Firebird, PostgreSQL, MySQL, Cassandra, MongoDB, Neo4j, Redis, Milvus, ScratchBird.
- Migration lifecycle: bootstrap, snapshot, CDC catch-up, dual-write, dual-read audit, cutover, retire.
- Parser/listener orchestration state and evidence.

## Hard Invariants
1. Engine remains SQL-agnostic and executes SBLR only.
2. Parser/listener mode changes are persisted before they are enforced.
3. Writes in dual-write mode are ordered and auditable (`legacy` then `emulated`).
4. Every mode transition is recorded with actor identity and mode version.
5. All audit decisions are reproducible from persisted metadata.

## Required Enum Kinds
Columns below use catalog enum domains (`[sb_dom]cat_enum_<kind>`) with labels from `CATALOG_ENUMS.md`:
- `migration_source_engine`
- `migration_runtime_mode`
- `migration_compare_policy`
- `migration_write_policy`
- `migration_return_source`
- `migration_cursor_kind`
- `migration_object_state`
- `migration_apply_state`
- `migration_compare_state`
- `migration_event_kind`
- `migration_error_class`

## Table: `migration_job`
One row per migration workflow.

Columns:
- `migration_uuid` `[sb_dom]cat_migration_uuid` PK
- `migration_name` `[sb_dom]cat_identifier`
- `source_engine` `[sb_dom]cat_enum_migration_source_engine`
- `fdw_server_uuid` `[sb_dom]cat_fdw_server_uuid`
- `source_database_name` `[sb_dom]cat_database_name`
- `source_root_path` `[sb_dom]cat_schema_path`
- `target_emulated_db_uuid` `[sb_dom]cat_emulated_db_uuid` nullable
- `target_schema_uuid` `[sb_dom]cat_schema_uuid`
- `runtime_mode` `[sb_dom]cat_enum_migration_runtime_mode`
- `mode_version` `[sb_dom]cat_version_u64`
- `compare_policy` `[sb_dom]cat_enum_migration_compare_policy`
- `write_policy` `[sb_dom]cat_enum_migration_write_policy`
- `mirror_policy` `[sb_dom]cat_enum_migration_write_policy`
- `return_source` `[sb_dom]cat_enum_migration_return_source`
- `audit_sample_rate` `[sb_dom]cat_f64`
- `snapshot_token` `[sb_dom]cat_text` nullable
- `snapshot_time` `[sb_dom]cat_timestamp` nullable
- `cursor_watermark` `[sb_dom]cat_text` nullable
- `lag_ms` `[sb_dom]cat_duration_ms`
- `created_by_uuid` `[sb_dom]cat_owner_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `cutover_approved_by_uuid` `[sb_dom]cat_owner_uuid` nullable
- `cutover_time` `[sb_dom]cat_timestamp` nullable
- `retired_time` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`migration_name`)
- `audit_sample_rate` must be between `0.0` and `1.0`.
- `mode_version` must increment by exactly `+1` per mode transition.
- `runtime_mode=RETIRED` requires non-null `retired_time`.
- `runtime_mode=PRIMARY_EMULATED` requires non-null `cutover_time`.

Indexes:
- `idx_sb_migration_job_mode` on (`runtime_mode`, `is_valid`)
- `idx_sb_migration_job_target` on (`target_schema_uuid`, `is_valid`)

## Table: `migration_object_map`
Maps source objects to ScratchBird object UUIDs and per-object migration state.

Columns:
- `object_map_uuid` `[sb_dom]cat_migration_object_map_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `object_kind` `[sb_dom]cat_enum_object_kind`
- `source_object_path` `[sb_dom]cat_schema_path`
- `source_object_signature` `[sb_dom]cat_hash32`
- `target_object_uuid` `[sb_dom]cat_object_uuid`
- `target_schema_uuid` `[sb_dom]cat_schema_uuid`
- `target_table_uuid` `[sb_dom]cat_table_uuid` nullable
- `target_index_uuid` `[sb_dom]cat_index_uuid` nullable
- `object_state` `[sb_dom]cat_enum_migration_object_state`
- `last_snapshot_txid` `[sb_dom]cat_txid`
- `last_applied_seq` `[sb_dom]cat_uint64`
- `last_verified_time` `[sb_dom]cat_timestamp` nullable
- `last_error_uuid` `[sb_dom]cat_migration_error_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`migration_uuid`, `object_kind`, `source_object_path`)
- `target_table_uuid` is required when `object_kind=TABLE`.
- `target_index_uuid` is required when `object_kind=INDEX`.

Indexes:
- `idx_sb_migration_object_map_state` on (`migration_uuid`, `object_state`, `is_valid`)
- `idx_sb_migration_object_map_target` on (`target_object_uuid`, `is_valid`)

## Table: `migration_cursor`
Persisted snapshot and CDC cursor positions per migration stream.

Columns:
- `cursor_uuid` `[sb_dom]cat_migration_cursor_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `cursor_kind` `[sb_dom]cat_enum_migration_cursor_kind`
- `stream_name` `[sb_dom]cat_identifier`
- `cursor_payload` `[sb_dom]cat_json`
- `source_commit_seq` `[sb_dom]cat_uint64`
- `source_commit_time` `[sb_dom]cat_timestamp` nullable
- `lag_ms` `[sb_dom]cat_duration_ms`
- `last_apply_time` `[sb_dom]cat_timestamp` nullable
- `is_active` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`migration_uuid`, `cursor_kind`, `stream_name`)

Indexes:
- `idx_sb_migration_cursor_active` on (`migration_uuid`, `is_active`, `is_valid`)
- `idx_sb_migration_cursor_lag` on (`migration_uuid`, `lag_ms`, `is_valid`)

## Table: `migration_apply_audit`
Write-path audit records for dual-write and mirror modes.

Columns:
- `apply_audit_uuid` `[sb_dom]cat_migration_apply_audit_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `cursor_uuid` `[sb_dom]cat_migration_cursor_uuid` nullable
- `object_map_uuid` `[sb_dom]cat_migration_object_map_uuid` nullable
- `request_uuid` `[sb_dom]cat_uuid`
- `source_commit_seq` `[sb_dom]cat_uint64`
- `operation_kind` `[sb_dom]cat_identifier`
- `apply_state` `[sb_dom]cat_enum_migration_apply_state`
- `retry_count` `[sb_dom]cat_uint16`
- `legacy_result_code` `[sb_dom]cat_identifier` nullable
- `emulated_result_code` `[sb_dom]cat_identifier` nullable
- `legacy_latency_ms` `[sb_dom]cat_duration_ms` nullable
- `emulated_latency_ms` `[sb_dom]cat_duration_ms` nullable
- `error_uuid` `[sb_dom]cat_migration_error_uuid` nullable
- `event_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `retry_count` must be `<= 65535`.
- `apply_state=FAILED` requires non-null `error_uuid`.

Indexes:
- `idx_sb_migration_apply_audit_seq` on (`migration_uuid`, `source_commit_seq`, `is_valid`)
- `idx_sb_migration_apply_audit_state` on (`migration_uuid`, `apply_state`, `is_valid`)

## Table: `migration_read_audit`
Read-path compare results for dual-read audit mode.

Columns:
- `read_audit_uuid` `[sb_dom]cat_migration_read_audit_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `session_uuid` `[sb_dom]cat_session_uuid`
- `statement_fingerprint` `[sb_dom]cat_hash32`
- `parameter_signature` `[sb_dom]cat_hash32` nullable
- `compare_policy` `[sb_dom]cat_enum_migration_compare_policy`
- `compare_state` `[sb_dom]cat_enum_migration_compare_state`
- `legacy_row_count` `[sb_dom]cat_uint64`
- `emulated_row_count` `[sb_dom]cat_uint64`
- `legacy_checksum` `[sb_dom]cat_hash32` nullable
- `emulated_checksum` `[sb_dom]cat_hash32` nullable
- `mismatch_count` `[sb_dom]cat_uint32`
- `diff_payload` `[sb_dom]cat_json` nullable
- `event_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `compare_state=MISMATCH` requires `mismatch_count > 0`.
- `compare_policy=ROW_COUNT_AND_CHECKSUM` requires non-null checksums.
- `compare_policy=FULL_ROW_COMPARE` requires non-null `diff_payload` on mismatch.

Indexes:
- `idx_sb_migration_read_audit_stmt` on (`migration_uuid`, `statement_fingerprint`, `event_time`)
- `idx_sb_migration_read_audit_state` on (`migration_uuid`, `compare_state`, `is_valid`)

## Table: `migration_event`
Authoritative mode transition and operator action log.

Columns:
- `event_uuid` `[sb_dom]cat_migration_event_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `mode_version` `[sb_dom]cat_version_u64`
- `event_kind` `[sb_dom]cat_enum_migration_event_kind`
- `previous_mode` `[sb_dom]cat_enum_migration_runtime_mode` nullable
- `new_mode` `[sb_dom]cat_enum_migration_runtime_mode` nullable
- `actor_uuid` `[sb_dom]cat_owner_uuid`
- `actor_source` `[sb_dom]cat_identifier`
- `approval_token` `[sb_dom]cat_identifier` nullable
- `event_note` `[sb_dom]cat_comment_text` nullable
- `event_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `previous_mode` and `new_mode` must both be non-null for `event_kind=MODE_CHANGE`.
- `mode_version` must match `migration_job.mode_version` at event commit time.

Indexes:
- `idx_sb_migration_event_mode` on (`migration_uuid`, `mode_version`)
- `idx_sb_migration_event_time` on (`migration_uuid`, `event_time`)

## Table: `migration_error`
Deduplicated migration error log for retry and escalation behavior.

Columns:
- `error_uuid` `[sb_dom]cat_migration_error_uuid` PK
- `migration_uuid` `[sb_dom]cat_migration_uuid`
- `error_class` `[sb_dom]cat_enum_migration_error_class`
- `severity` `[sb_dom]cat_enum_alert_severity`
- `source_component` `[sb_dom]cat_identifier`
- `source_code` `[sb_dom]cat_identifier`
- `message_text` `[sb_dom]cat_text`
- `recoverable` `[sb_dom]cat_bool`
- `retry_after_ms` `[sb_dom]cat_duration_ms` nullable
- `first_seen_time` `[sb_dom]cat_timestamp`
- `last_seen_time` `[sb_dom]cat_timestamp`
- `occurrence_count` `[sb_dom]cat_uint32`
- `is_open` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `last_seen_time >= first_seen_time`.
- `recoverable=false` requires `retry_after_ms` null.
- `occurrence_count >= 1`.

Indexes:
- `idx_sb_migration_error_open` on (`migration_uuid`, `is_open`, `severity`)
- `idx_sb_migration_error_class` on (`migration_uuid`, `error_class`, `is_valid`)

## Required System Views
- `sys.migration_status`
  - One row per `migration_job`.
  - Includes `mode_version`, `runtime_mode`, `lag_ms`, `open_error_count`, `last_event_time`.
- `sys.migration_audit_summary`
  - Aggregates `migration_apply_audit` and `migration_read_audit` by `migration_uuid` and rolling time window.

## Retention and Purge Rules
- `migration_apply_audit` and `migration_read_audit` rows are retained for at least `config:migration.audit_retention_days`.
- Purge operation must preserve rows referenced by open `migration_error` records.
- Purge operation must be blocked while `runtime_mode` is not `RETIRED`.

## Normative Implementation Checklist
1. Create all tables in one atomic catalog bootstrap step with FK validation enabled.
2. Create all required indexes before enabling migration control SQL surfaces.
3. Enforce mode transition writes using optimistic `mode_version` compare-and-swap.
4. Persist event row in `migration_event` in the same transaction as `migration_job.runtime_mode`.
5. Record every write-path operation in `migration_apply_audit` while in `DUAL_WRITE`, `DUAL_READ_AUDIT`, or `MIRROR_LEGACY`.
6. Record every compare operation in `migration_read_audit` while in `DUAL_READ_AUDIT`.
7. Surface open errors in `sys.migration_status` from `migration_error` where `is_open=true`.
8. Reject cutover if unresolved `compare_state=MISMATCH` rows exist within the configured guard window.
9. Reject retire if backlog cursor rows in `migration_cursor` are still `is_active=true`.
10. Mark migration records `is_valid=false` only through explicit retire/drop control flow.

## Cross-Section Links
- `28_Parser_Implementations/NORMATIVE_PARSER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md`
- `29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_PASSTHROUGH_AND_LIVE_MIGRATION_CHECKLIST.md`
- `30_Client_Tooling/NATIVE_MIGRATION_AND_PASSTHROUGH_SQL_CONTROL_CONTRACT.md`
