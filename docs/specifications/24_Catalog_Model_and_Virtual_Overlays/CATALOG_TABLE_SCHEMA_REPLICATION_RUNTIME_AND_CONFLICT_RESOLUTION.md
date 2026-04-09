# Catalog: Replication Runtime and Conflict Resolution Tables

## Purpose
Define canonical persisted catalog tables for one-way and bi-directional replication execution, cursor tracking, apply ordering, retries, conflict handling, and split-brain control.

## Scope
- Runtime replication orchestration for publisher/subscriber and peer/peer channels.
- Replication state and evidence persistence.
- Conflict and split-brain handling metadata.

## Hard Invariants
1. Replication apply ordering is deterministic and transaction-atomic.
2. Every applied batch records origin and cursor position.
3. Bi-directional replication must prevent loops using origin progress vectors.
4. Conflict decisions are persisted before downstream apply continues.
5. Split-brain state fences replication apply until explicit recovery.

## Required Enum Kinds
Columns below use catalog enum domains (`[sb_dom]cat_enum_<kind>`) with labels from `CATALOG_ENUMS.md`:
- `replication_direction`
- `replication_channel_state`
- `replication_member_role`
- `replication_cursor_state`
- `replication_txn_state`
- `replication_retry_state`
- `replication_ddl_policy`
- `replication_conflict_policy`
- `replication_conflict_kind`
- `replication_resolution_state`
- `replication_event_kind`

## Table: `replication_channel`
One row per configured replication channel.

Columns:
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid` PK
- `channel_name` `[sb_dom]cat_identifier`
- `direction` `[sb_dom]cat_enum_replication_direction`
- `channel_state` `[sb_dom]cat_enum_replication_channel_state`
- `mode_version` `[sb_dom]cat_version_u64`
- `publication_uuid` `[sb_dom]cat_publication_uuid` nullable
- `subscription_uuid` `[sb_dom]cat_subscription_uuid` nullable
- `source_server_uuid` `[sb_dom]cat_fdw_server_uuid` nullable
- `target_server_uuid` `[sb_dom]cat_fdw_server_uuid` nullable
- `ddl_policy` `[sb_dom]cat_enum_replication_ddl_policy`
- `conflict_policy` `[sb_dom]cat_enum_replication_conflict_policy`
- `max_retry_count` `[sb_dom]cat_uint16`
- `retry_backoff_base_ms` `[sb_dom]cat_duration_ms`
- `retry_backoff_max_ms` `[sb_dom]cat_duration_ms`
- `lag_warn_ms` `[sb_dom]cat_duration_ms`
- `lag_critical_ms` `[sb_dom]cat_duration_ms`
- `batch_max_txn` `[sb_dom]cat_uint32`
- `batch_max_bytes` `[sb_dom]cat_bytes_u64`
- `split_brain_fence_enabled` `[sb_dom]cat_bool`
- `split_brain_detect_window_ms` `[sb_dom]cat_duration_ms`
- `created_by_uuid` `[sb_dom]cat_owner_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`channel_name`)
- `mode_version` increments exactly `+1` per state/policy mutation.
- `lag_warn_ms <= lag_critical_ms`.
- `retry_backoff_base_ms <= retry_backoff_max_ms`.
- `direction=ONE_WAY` requires non-null `publication_uuid` and `subscription_uuid`.
- `direction=BIDIRECTIONAL` requires at least one publication and one subscription endpoint mapping in `replication_channel_member`.

Indexes:
- `idx_sb_replication_channel_state` on (`channel_state`, `is_valid`)
- `idx_sb_replication_channel_direction` on (`direction`, `is_valid`)

## Table: `replication_channel_member`
Maps logical channel members and their roles.

Columns:
- `channel_member_uuid` `[sb_dom]cat_replication_channel_member_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
- `member_name` `[sb_dom]cat_identifier`
- `member_role` `[sb_dom]cat_enum_replication_member_role`
- `fdw_server_uuid` `[sb_dom]cat_fdw_server_uuid` nullable
- `local_endpoint` `[sb_dom]cat_bool`
- `priority_rank` `[sb_dom]cat_uint16`
- `origin_uuid` `[sb_dom]cat_replication_origin_uuid`
- `is_active` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`replication_channel_uuid`, `member_name`)
- UNIQUE(`replication_channel_uuid`, `origin_uuid`)
- In `ONE_WAY` channels exactly one `PUBLISHER` and one `SUBSCRIBER` member must exist.
- In `BIDIRECTIONAL` channels at least two active `PEER` members must exist.

Indexes:
- `idx_sb_replication_channel_member_role` on (`replication_channel_uuid`, `member_role`, `is_active`)

## Table: `replication_origin`
Stable origin identities used for loop prevention and conflict tie-breaks.

Columns:
- `replication_origin_uuid` `[sb_dom]cat_replication_origin_uuid` PK
- `origin_name` `[sb_dom]cat_identifier`
- `origin_scope` `[sb_dom]cat_identifier`
- `origin_priority` `[sb_dom]cat_uint16`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`origin_name`)
- Lower `origin_priority` means stronger precedence in priority-based conflict policy.

## Table: `replication_cursor`
Cursor positions and lag for each channel member stream.

Columns:
- `replication_cursor_uuid` `[sb_dom]cat_replication_cursor_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
- `channel_member_uuid` `[sb_dom]cat_replication_channel_member_uuid`
- `cursor_name` `[sb_dom]cat_identifier`
- `cursor_state` `[sb_dom]cat_enum_replication_cursor_state`
- `cursor_payload` `[sb_dom]cat_json`
- `source_commit_seq` `[sb_dom]cat_uint64`
- `source_commit_time` `[sb_dom]cat_timestamp` nullable
- `applied_commit_seq` `[sb_dom]cat_uint64`
- `applied_time` `[sb_dom]cat_timestamp` nullable
- `lag_ms` `[sb_dom]cat_duration_ms`
- `heartbeat_time` `[sb_dom]cat_timestamp` nullable
- `last_error_uuid` `[sb_dom]cat_replication_error_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`replication_channel_uuid`, `channel_member_uuid`, `cursor_name`)
- `applied_commit_seq <= source_commit_seq`

Indexes:
- `idx_sb_replication_cursor_state` on (`replication_channel_uuid`, `cursor_state`, `is_valid`)
- `idx_sb_replication_cursor_lag` on (`replication_channel_uuid`, `lag_ms`, `is_valid`)

## Table: `replication_origin_progress`
Origin progress vectors used for loop prevention.

Columns:
- `origin_progress_uuid` `[sb_dom]cat_replication_origin_progress_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
- `target_member_uuid` `[sb_dom]cat_replication_channel_member_uuid`
- `origin_uuid` `[sb_dom]cat_replication_origin_uuid`
- `max_applied_commit_seq` `[sb_dom]cat_uint64`
- `max_applied_time` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`replication_channel_uuid`, `target_member_uuid`, `origin_uuid`)

Indexes:
- `idx_sb_replication_origin_progress` on (`replication_channel_uuid`, `target_member_uuid`, `origin_uuid`)

## Table: `replication_txn_batch`
Batch-level ingest and apply tracking.

Columns:
- `replication_batch_uuid` `[sb_dom]cat_replication_batch_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
- `source_member_uuid` `[sb_dom]cat_replication_channel_member_uuid`
- `origin_uuid` `[sb_dom]cat_replication_origin_uuid`
- `source_txn_id` `[sb_dom]cat_identifier`
- `source_commit_seq` `[sb_dom]cat_uint64`
- `source_commit_time` `[sb_dom]cat_timestamp`
- `txn_state` `[sb_dom]cat_enum_replication_txn_state`
- `change_count` `[sb_dom]cat_uint32`
- `payload_bytes` `[sb_dom]cat_bytes_u64`
- `batch_checksum` `[sb_dom]cat_hash32`
- `received_time` `[sb_dom]cat_timestamp`
- `applied_time` `[sb_dom]cat_timestamp` nullable
- `retry_count` `[sb_dom]cat_uint16`
- `last_error_uuid` `[sb_dom]cat_replication_error_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`replication_channel_uuid`, `source_member_uuid`, `source_commit_seq`)
- `txn_state=FAILED` requires `last_error_uuid` non-null.

Indexes:
- `idx_sb_replication_txn_batch_state` on (`replication_channel_uuid`, `txn_state`, `is_valid`)
- `idx_sb_replication_txn_batch_order` on (`replication_channel_uuid`, `source_commit_seq`)

## Table: `replication_apply_log`
Per-target apply outcomes.

Columns:
- `replication_apply_log_uuid` `[sb_dom]cat_replication_apply_log_uuid` PK
- `replication_batch_uuid` `[sb_dom]cat_replication_batch_uuid`
- `target_member_uuid` `[sb_dom]cat_replication_channel_member_uuid`
- `apply_order` `[sb_dom]cat_uint16`
- `txn_state` `[sb_dom]cat_enum_replication_txn_state`
- `apply_start_time` `[sb_dom]cat_timestamp`
- `apply_end_time` `[sb_dom]cat_timestamp` nullable
- `applied_commit_seq` `[sb_dom]cat_uint64` nullable
- `rows_inserted` `[sb_dom]cat_uint64`
- `rows_updated` `[sb_dom]cat_uint64`
- `rows_deleted` `[sb_dom]cat_uint64`
- `ddl_count` `[sb_dom]cat_uint32`
- `error_uuid` `[sb_dom]cat_replication_error_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`replication_batch_uuid`, `target_member_uuid`)
- `apply_order >= 1`.

Indexes:
- `idx_sb_replication_apply_log_state` on (`target_member_uuid`, `txn_state`, `is_valid`)

## Table: `replication_retry_queue`
Deterministic retry queue for failed apply batches.

Columns:
- `replication_retry_uuid` `[sb_dom]cat_replication_retry_uuid` PK
- `replication_batch_uuid` `[sb_dom]cat_replication_batch_uuid`
- `retry_state` `[sb_dom]cat_enum_replication_retry_state`
- `retry_count` `[sb_dom]cat_uint16`
- `next_retry_time` `[sb_dom]cat_timestamp`
- `last_retry_time` `[sb_dom]cat_timestamp` nullable
- `last_error_uuid` `[sb_dom]cat_replication_error_uuid` nullable
- `dead_letter_reason` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`replication_batch_uuid`)
- `retry_state=DEAD_LETTER` requires non-null `dead_letter_reason`.

Indexes:
- `idx_sb_replication_retry_queue_due` on (`retry_state`, `next_retry_time`)

## Table: `replication_conflict`
Open and historical replication conflicts.

Columns:
- `replication_conflict_uuid` `[sb_dom]cat_replication_conflict_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
- `replication_batch_uuid` `[sb_dom]cat_replication_batch_uuid`
- `conflict_kind` `[sb_dom]cat_enum_replication_conflict_kind`
- `source_origin_uuid` `[sb_dom]cat_replication_origin_uuid`
- `target_origin_uuid` `[sb_dom]cat_replication_origin_uuid` nullable
- `target_object_uuid` `[sb_dom]cat_object_uuid`
- `target_row_uuid` `[sb_dom]cat_uuid` nullable
- `source_commit_seq` `[sb_dom]cat_uint64`
- `target_commit_seq` `[sb_dom]cat_uint64` nullable
- `source_payload` `[sb_dom]cat_json`
- `target_payload` `[sb_dom]cat_json` nullable
- `resolution_state` `[sb_dom]cat_enum_replication_resolution_state`
- `resolved_by_uuid` `[sb_dom]cat_owner_uuid` nullable
- `resolved_time` `[sb_dom]cat_timestamp` nullable
- `resolution_note` `[sb_dom]cat_comment_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `resolution_state` in resolved set requires non-null `resolved_time`.

Indexes:
- `idx_sb_replication_conflict_open` on (`replication_channel_uuid`, `resolution_state`)
- `idx_sb_replication_conflict_object` on (`target_object_uuid`, `target_row_uuid`)

## Table: `replication_split_brain_event`
Records partition/split-brain detections and recovery.

Columns:
- `replication_split_brain_uuid` `[sb_dom]cat_replication_split_brain_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
- `event_kind` `[sb_dom]cat_enum_replication_event_kind`
- `detected_time` `[sb_dom]cat_timestamp`
- `resolved_time` `[sb_dom]cat_timestamp` nullable
- `detection_payload` `[sb_dom]cat_json`
- `resolution_payload` `[sb_dom]cat_json` nullable
- `fence_applied` `[sb_dom]cat_bool`
- `fence_cleared` `[sb_dom]cat_bool`
- `approved_by_uuid` `[sb_dom]cat_owner_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `event_kind=SPLIT_BRAIN_CLEARED` requires non-null `resolved_time`.

Indexes:
- `idx_sb_replication_split_brain_open` on (`replication_channel_uuid`, `fence_applied`, `fence_cleared`)

## Table: `replication_error`
Deduplicated replication errors.

Columns:
- `replication_error_uuid` `[sb_dom]cat_replication_error_uuid` PK
- `replication_channel_uuid` `[sb_dom]cat_replication_channel_uuid`
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
- `occurrence_count >= 1`.
- `recoverable=false` requires `retry_after_ms` null.

Indexes:
- `idx_sb_replication_error_open` on (`replication_channel_uuid`, `is_open`)

## Required System Views
- `sys.replication_channel_status`
  - One row per channel with `channel_state`, `mode_version`, `lag_ms`, `open_conflict_count`, `open_error_count`.
- `sys.replication_conflict_queue`
  - One row per unresolved conflict.
- `sys.replication_cursor_status`
  - One row per active cursor with current commit sequence and lag.

## Normative One-Way and Bi-Directional Data Rules
1. Apply order is ascending `source_commit_seq`; ties are resolved by ascending `source_member_uuid`.
2. Batches with the same `source_txn_id` must be applied atomically per target member.
3. For bi-directional channels, apply must reject a batch when `source_commit_seq <= max_applied_commit_seq` in `replication_origin_progress` for the same origin.
4. Conflict rows must be created before `txn_state` changes to `FAILED` or `CONFLICT`.
5. Split-brain detection must fence channel (`channel_state=FENCED`) before new apply batches are accepted.

## Normative Implementation Checklist
1. Create all tables in one catalog bootstrap transaction with FK checks enabled.
2. Create required indexes before enabling replication control SQL.
3. Enforce `mode_version` compare-and-swap for channel state/policy changes.
4. Persist `replication_txn_batch` before any target apply work begins.
5. Persist `replication_apply_log` for every target apply attempt.
6. Queue retry in `replication_retry_queue` for retryable failures only.
7. Persist origin progress updates only after successful target apply commit.
8. Persist conflicts in `replication_conflict` before retry or manual resolution decisions.
9. Persist split-brain events and apply fence/clear flags in `replication_split_brain_event`.
10. Reject channel transition to `STREAMING` while unresolved split-brain fence exists.

## Cross-Section Links
- `28_Parser_Implementations/NORMATIVE_PARSER_REPLICATION_CONTROL_AND_ROUTING_CHECKLIST.md`
- `29_Listener_and_Server_Orchestration/NORMATIVE_LISTENER_ONE_WAY_AND_BIDIRECTIONAL_REPLICATION_CHECKLIST.md`
- `30_Client_Tooling/NATIVE_REPLICATION_SQL_CONTROL_CONTRACT.md`
