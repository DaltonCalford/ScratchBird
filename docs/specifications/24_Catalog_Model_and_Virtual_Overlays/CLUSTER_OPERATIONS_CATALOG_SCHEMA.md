# Catalog: Cluster Operations Schema

## Purpose
Define canonical catalog tables required for routing, admission control, alerting, healing, and job scheduling. These tables must exist so future cluster features can be implemented without on disk changes.

Clock/SLO/autoscale/admission-policy schemas are canonicalized in:
- `CATALOG_TABLE_SCHEMA_CLOCK_SLO_AND_AUTOSCALE.md`
ScratchBird parserless cluster-fabric link/session/task schemas are canonicalized in:
- `CATALOG_TABLE_SCHEMA_SB_CLUSTER_FABRIC.md`

## Conventions
- All columns use catalog domains defined in `CATALOG_SYSTEM_DOMAINS.md`.
- Enum values are defined in `CATALOG_ENUMS.md`.
- `*_uuid` columns use `[sb_dom]cat_<name>_uuid`.
- `*_toast_uuid` columns reference TOAST values.

## Routing and Admission Tables

### Table: `workload_class`
Columns:
- `class_uuid` `[sb_dom]cat_workload_class_uuid` PK
- `class_name` `[sb_dom]cat_identifier`
- `description` `[sb_dom]cat_text` nullable
- `match_kind` `[sb_dom]cat_enum_workload_match_kind`
- `match_expr_sblr_uuid` `[sb_dom]cat_workload_match_expr_uuid` nullable
- `match_text` `[sb_dom]cat_text` nullable
- `default_role` `[sb_dom]cat_enum_node_role` nullable
- `priority` `[sb_dom]cat_priority_u8`
- `max_latency_ms` `[sb_dom]cat_duration_ms` nullable
- `allow_cross_shard` `[sb_dom]cat_bool`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`class_name`)
- Exactly one of `match_expr_sblr_uuid` or `match_text` must be non-null.

### Table: `workload_route`
Columns:
- `route_uuid` `[sb_dom]cat_workload_route_uuid` PK
- `class_uuid` `[sb_dom]cat_workload_class_uuid`
- `route_name` `[sb_dom]cat_identifier`
- `target_kind` `[sb_dom]cat_enum_route_target_kind`
- `target_uuid` `[sb_dom]cat_uuid` nullable
- `target_label` `[sb_dom]cat_identifier` nullable
- `role` `[sb_dom]cat_enum_node_role` nullable
- `service_type` `[sb_dom]cat_enum_service_type` nullable
- `transport` `[sb_dom]cat_enum_transport`
- `route_weight` `[sb_dom]cat_uint16`
- `selector_expr_sblr_uuid` `[sb_dom]cat_workload_selector_expr_uuid` nullable
- `fallback_route_uuid` `[sb_dom]cat_workload_route_uuid` nullable
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`class_uuid`, `route_name`)
- `target_uuid` or `target_label` must be set.

### Table: `admission_policy`
Columns:
- `policy_uuid` `[sb_dom]cat_admission_policy_uuid` PK
- `policy_name` `[sb_dom]cat_identifier`
- `max_concurrent_sessions` `[sb_dom]cat_uint32`
- `max_concurrent_queries` `[sb_dom]cat_uint32`
- `max_queue_depth` `[sb_dom]cat_uint32`
- `cpu_reject_pct` `[sb_dom]cat_percent_u8`
- `mem_reject_pct` `[sb_dom]cat_percent_u8`
- `io_reject_pct` `[sb_dom]cat_percent_u8`
- `reject_mode` `[sb_dom]cat_enum_admission_reject_mode`
- `queue_timeout_ms` `[sb_dom]cat_duration_ms`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`policy_name`)

### Table: `admission_binding`
Columns:
- `binding_uuid` `[sb_dom]cat_admission_binding_uuid` PK
- `policy_uuid` `[sb_dom]cat_admission_policy_uuid`
- `target_kind` `[sb_dom]cat_enum_admission_target_kind`
- `target_uuid` `[sb_dom]cat_uuid` nullable
- `class_uuid` `[sb_dom]cat_workload_class_uuid` nullable
- `priority` `[sb_dom]cat_priority_u8`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- Exactly one of `target_uuid` or `class_uuid` must be set.

### Table: `sys.node.capacity_profile`
Columns:
- `profile_uuid` `[sb_dom]cat_node_capacity_profile_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `cpu_cores` `[sb_dom]cat_uint16`
- `memory_bytes` `[sb_dom]cat_bytes_u64`
- `disk_bytes` `[sb_dom]cat_bytes_u64`
- `net_gbps` `[sb_dom]cat_uint16`
- `max_sessions` `[sb_dom]cat_uint32`
- `max_queries` `[sb_dom]cat_uint32`
- `sampled_time` `[sb_dom]cat_timestamp`
- `is_current` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`node_uuid`, `is_current`) where `is_current=true`.

## Cluster Policy and Failure Detection

### Table: `cluster_policy`
Columns:
- `policy_uuid` `[sb_dom]cat_cluster_policy_uuid` PK
- `cluster_uuid` `[sb_dom]cat_cluster_uuid`
- `policy_name` `[sb_dom]cat_identifier`
- `policy_kind` `[sb_dom]cat_enum_cluster_policy_kind`
- `policy_json_uuid` `[sb_dom]cat_policy_json_uuid` nullable
- `is_active` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`cluster_uuid`, `policy_name`)

### Table: `failure_detector`
Columns:
- `detector_uuid` `[sb_dom]cat_failure_detector_uuid` PK
- `cluster_uuid` `[sb_dom]cat_cluster_uuid`
- `detector_kind` `[sb_dom]cat_enum_failure_detector_kind`
- `heartbeat_interval_ms` `[sb_dom]cat_interval_ms`
- `phi_threshold` `[sb_dom]cat_f64` nullable
- `miss_threshold` `[sb_dom]cat_uint16` nullable
- `suspect_threshold` `[sb_dom]cat_uint16` nullable
- `fail_threshold` `[sb_dom]cat_uint16` nullable
- `grace_startup_ms` `[sb_dom]cat_interval_ms`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Alerting Tables

### Table: `alert_rule`
Columns:
- `rule_uuid` `[sb_dom]cat_alert_rule_uuid` PK
- `rule_name` `[sb_dom]cat_identifier`
- `rule_kind` `[sb_dom]cat_enum_alert_rule_kind`
- `severity` `[sb_dom]cat_enum_alert_severity`
- `condition_sblr_uuid` `[sb_dom]cat_alert_condition_uuid` nullable
- `condition_text` `[sb_dom]cat_text` nullable
- `throttle_interval_ms` `[sb_dom]cat_interval_ms`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`rule_name`)
- Exactly one of `condition_sblr_uuid` or `condition_text` must be non-null.

### Table: `alert_target`
Columns:
- `target_uuid` `[sb_dom]cat_alert_target_uuid` PK
- `target_name` `[sb_dom]cat_identifier`
- `target_kind` `[sb_dom]cat_enum_alert_target_kind`
- `endpoint` `[sb_dom]cat_text`
- `auth_secret_uuid` `[sb_dom]cat_alert_auth_uuid` nullable
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`target_name`)

### Table: `alert_route`
Columns:
- `route_uuid` `[sb_dom]cat_alert_route_uuid` PK
- `rule_uuid` `[sb_dom]cat_alert_rule_uuid`
- `target_uuid` `[sb_dom]cat_alert_target_uuid`
- `route_kind` `[sb_dom]cat_enum_alert_route_kind`
- `severity_min` `[sb_dom]cat_enum_alert_severity`
- `severity_max` `[sb_dom]cat_enum_alert_severity`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`rule_uuid`, `target_uuid`)

### Table: `alert_event`
Columns:
- `event_uuid` `[sb_dom]cat_alert_event_uuid` PK
- `rule_uuid` `[sb_dom]cat_alert_rule_uuid`
- `severity` `[sb_dom]cat_enum_alert_severity`
- `event_state` `[sb_dom]cat_enum_alert_event_state`
- `event_time` `[sb_dom]cat_timestamp`
- `resolved_time` `[sb_dom]cat_timestamp` nullable
- `event_payload_uuid` `[sb_dom]cat_alert_payload_uuid` nullable
- `is_valid` `[sb_dom]cat_bool`

### Table: `alert_ack`
Columns:
- `ack_uuid` `[sb_dom]cat_alert_ack_uuid` PK
- `event_uuid` `[sb_dom]cat_alert_event_uuid`
- `user_uuid` `[sb_dom]cat_user_uuid`
- `ack_time` `[sb_dom]cat_timestamp`
- `comment` `[sb_dom]cat_comment_text` nullable
- `is_valid` `[sb_dom]cat_bool`

### Table: `alert_silence`
Columns:
- `silence_uuid` `[sb_dom]cat_alert_silence_uuid` PK
- `scope_kind` `[sb_dom]cat_enum_alert_silence_scope`
- `scope_uuid` `[sb_dom]cat_uuid` nullable
- `starts_time` `[sb_dom]cat_timestamp`
- `ends_time` `[sb_dom]cat_timestamp`
- `created_by_uuid` `[sb_dom]cat_user_uuid`
- `reason` `[sb_dom]cat_text` nullable
- `is_enabled` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `ends_time` must be greater than `starts_time`.

## Partition and Segmentation Tables

### Table: `network_partition_event`
Columns:
- `partition_uuid` `[sb_dom]cat_partition_event_uuid` PK
- `cluster_uuid` `[sb_dom]cat_cluster_uuid`
- `partition_state` `[sb_dom]cat_enum_partition_state`
- `opened_time` `[sb_dom]cat_timestamp`
- `resolved_time` `[sb_dom]cat_timestamp` nullable
- `quorum_reachable` `[sb_dom]cat_bool`
- `local_node_uuid` `[sb_dom]cat_node_uuid`
- `description` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

### Table: `network_partition_member`
Columns:
- `member_uuid` `[sb_dom]cat_partition_member_uuid` PK
- `partition_uuid` `[sb_dom]cat_partition_event_uuid`
- `node_uuid` `[sb_dom]cat_node_uuid`
- `side_id` `[sb_dom]cat_uint16`
- `reachable` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

## Healing Tables

### Table: `healing_policy`
Columns:
- `policy_uuid` `[sb_dom]cat_healing_policy_uuid` PK
- `policy_name` `[sb_dom]cat_identifier`
- `trigger_kind` `[sb_dom]cat_enum_healing_trigger_kind`
- `min_severity` `[sb_dom]cat_enum_alert_severity`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`policy_name`)

### Table: `healing_action`
Columns:
- `action_uuid` `[sb_dom]cat_healing_action_uuid` PK
- `policy_uuid` `[sb_dom]cat_healing_policy_uuid`
- `action_kind` `[sb_dom]cat_enum_healing_action_kind`
- `action_order` `[sb_dom]cat_uint16`
- `is_blocking` `[sb_dom]cat_bool`
- `max_retries` `[sb_dom]cat_uint16`
- `cooldown_ms` `[sb_dom]cat_interval_ms`
- `is_enabled` `[sb_dom]cat_bool`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

### Table: `healing_action_param`
Columns:
- `param_uuid` `[sb_dom]cat_healing_action_param_uuid` PK
- `action_uuid` `[sb_dom]cat_healing_action_uuid`
- `param_key` `[sb_dom]cat_identifier`
- `param_type` `[sb_dom]cat_enum_healing_param_type`
- `val_u64` `[sb_dom]cat_uint64` nullable
- `val_i64` `[sb_dom]cat_int64` nullable
- `val_f64` `[sb_dom]cat_f64` nullable
- `val_bool` `[sb_dom]cat_bool` nullable
- `val_text` `[sb_dom]cat_text` nullable
- `val_uuid` `[sb_dom]cat_uuid` nullable
- `val_json` `[sb_dom]cat_json` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`action_uuid`, `param_key`)
- Exactly one of the `val_*` columns must be non-null and must match `param_type`.

### Table: `healing_run`
Columns:
- `run_uuid` `[sb_dom]cat_healing_run_uuid` PK
- `policy_uuid` `[sb_dom]cat_healing_policy_uuid`
- `trigger_event_uuid` `[sb_dom]cat_alert_event_uuid` nullable
- `state` `[sb_dom]cat_enum_healing_run_state`
- `started_time` `[sb_dom]cat_timestamp`
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `error_message` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

### Table: `healing_step`
Columns:
- `step_uuid` `[sb_dom]cat_healing_step_uuid` PK
- `run_uuid` `[sb_dom]cat_healing_run_uuid`
- `action_uuid` `[sb_dom]cat_healing_action_uuid`
- `step_index` `[sb_dom]cat_uint16`
- `state` `[sb_dom]cat_enum_healing_step_state`
- `started_time` `[sb_dom]cat_timestamp` nullable
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `error_message` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`run_uuid`, `step_index`)

## Scheduler Tables

### Table: `job_type`
Columns:
- `job_type_uuid` `[sb_dom]cat_job_type_uuid` PK
- `job_type_name` `[sb_dom]cat_identifier`
- `job_group` `[sb_dom]cat_enum_job_group`
- `is_system` `[sb_dom]cat_bool`
- `is_enabled` `[sb_dom]cat_bool`
- `default_timeout_ms` `[sb_dom]cat_duration_ms`
- `default_max_retries` `[sb_dom]cat_uint16`
- `default_priority` `[sb_dom]cat_priority_u8`
- `description` `[sb_dom]cat_text` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`job_type_name`)

### Table: `job_type_param`
Columns:
- `param_uuid` `[sb_dom]cat_job_type_param_uuid` PK
- `job_type_uuid` `[sb_dom]cat_job_type_uuid`
- `param_key` `[sb_dom]cat_identifier`
- `param_type` `[sb_dom]cat_enum_job_param_type`
- `is_required` `[sb_dom]cat_bool`
- `default_value` `[sb_dom]cat_text` nullable
- `description` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`job_type_uuid`, `param_key`)

### Table: `job`
Columns:
- `job_uuid` `[sb_dom]cat_job_uuid` PK
- `job_type_uuid` `[sb_dom]cat_job_type_uuid`
- `job_name` `[sb_dom]cat_job_name`
- `description` `[sb_dom]cat_text` nullable
- `job_class` `[sb_dom]cat_enum_job_class`
- `job_type` `[sb_dom]cat_enum_job_type`
- `owner_uuid` `[sb_dom]cat_owner_uuid`
- `state` `[sb_dom]cat_enum_job_state`
- `job_sql` `[sb_dom]cat_sql_text` nullable
- `procedure_uuid` `[sb_dom]cat_procedure_uuid` nullable
- `external_command` `[sb_dom]cat_text` nullable
- `schedule_kind` `[sb_dom]cat_enum_schedule_kind`
- `cron_expression` `[sb_dom]cat_text` nullable
- `interval_ms` `[sb_dom]cat_interval_ms` nullable
- `starts_time` `[sb_dom]cat_timestamp` nullable
- `ends_time` `[sb_dom]cat_timestamp` nullable
- `schedule_tz` `[sb_dom]cat_identifier` nullable
- `next_run_time` `[sb_dom]cat_timestamp` nullable
- `on_completion` `[sb_dom]cat_enum_job_on_completion`
- `partition_strategy` `[sb_dom]cat_identifier` nullable
- `partition_shard_uuid` `[sb_dom]cat_shard_uuid` nullable
- `partition_expression` `[sb_dom]cat_text` nullable
- `max_retries` `[sb_dom]cat_uint32`
- `retry_backoff_ms` `[sb_dom]cat_interval_ms`
- `timeout_ms` `[sb_dom]cat_duration_ms`
- `created_by_user_uuid` `[sb_dom]cat_user_uuid`
- `run_as_role_uuid` `[sb_dom]cat_role_uuid` nullable
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`job_name`)

### Table: `job_param`
Columns:
- `param_uuid` `[sb_dom]cat_job_param_uuid` PK
- `job_uuid` `[sb_dom]cat_job_uuid`
- `param_key` `[sb_dom]cat_identifier`
- `param_type` `[sb_dom]cat_enum_job_param_type`
- `param_value` `[sb_dom]cat_text`

Constraints:
- UNIQUE(`job_uuid`, `param_key`)

### Table: `job_schedule`
Columns:
- `schedule_uuid` `[sb_dom]cat_job_schedule_uuid` PK
- `schedule_kind` `[sb_dom]cat_enum_schedule_kind`
- `interval_ms` `[sb_dom]cat_interval_ms` nullable
- `cron_expr` `[sb_dom]cat_text` nullable
- `is_enabled` `[sb_dom]cat_bool`

Constraints:
- If `schedule_kind=EVERY` then `interval_ms` must be set.
- If `schedule_kind=CRON` then `cron_expr` must be set.

### Table: `job_run`
Columns:
- `run_uuid` `[sb_dom]cat_job_run_uuid` PK
- `job_uuid` `[sb_dom]cat_job_uuid`
- `assigned_node_uuid` `[sb_dom]cat_node_uuid`
- `shard_uuid` `[sb_dom]cat_shard_uuid` nullable
- `scheduled_time` `[sb_dom]cat_timestamp`
- `started_time` `[sb_dom]cat_timestamp`
- `completed_time` `[sb_dom]cat_timestamp` nullable
- `state` `[sb_dom]cat_enum_job_run_state`
- `retry_count` `[sb_dom]cat_uint32`
- `rows_affected` `[sb_dom]cat_int64`
- `error_code` `[sb_dom]cat_uint32`
- `result_data_uuid` `[sb_dom]cat_result_data_uuid` nullable
- `result_message` `[sb_dom]cat_text` nullable
- `is_valid` `[sb_dom]cat_bool`

### Table: `job_dependency`
Columns:
- `dependency_uuid` `[sb_dom]cat_job_dependency_uuid` PK
- `job_uuid` `[sb_dom]cat_job_uuid`
- `depends_on_job_uuid` `[sb_dom]cat_job_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

### Table: `job_secret`
Columns:
- `secret_uuid` `[sb_dom]cat_job_secret_uuid` PK
- `job_uuid` `[sb_dom]cat_job_uuid`
- `secret_key` `[sb_dom]cat_identifier`
- `secret_value_uuid` `[sb_dom]cat_secret_value_uuid`
- `created_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

### Table: `job_type_policy`
Columns:
- `policy_uuid` `[sb_dom]cat_job_type_policy_uuid` PK
- `job_type_uuid` `[sb_dom]cat_job_type_uuid`
- `max_concurrent` `[sb_dom]cat_uint16`
- `is_enabled` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`job_type_uuid`)

## Node Lifecycle Tables (Schema: `sys.node`)

### Table: `bootstrap_token`
Columns:
- `token_uuid` `[sb_dom]cat_node_bootstrap_token_uuid` PK
- `token_hash` `[sb_dom]cat_hash32`
- `issued_time` `[sb_dom]cat_timestamp`
- `expires_time` `[sb_dom]cat_timestamp`
- `max_uses` `[sb_dom]cat_uint16`
- `uses` `[sb_dom]cat_uint16`
- `state` `[sb_dom]cat_enum_token_state`

Constraints:
- UNIQUE(`token_hash`)

### Table: `lifecycle_event`
Columns:
- `event_uuid` `[sb_dom]cat_node_lifecycle_event_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `event_type` `[sb_dom]cat_enum_lifecycle_event_type`
- `event_time` `[sb_dom]cat_timestamp`
- `details` `[sb_dom]cat_json` nullable

Indexes:
- INDEX(`node_uuid`)
- INDEX(`event_time`)

### Table: `clock_state`
Canonical definition:
- `CATALOG_TABLE_SCHEMA_CLOCK_SLO_AND_AUTOSCALE.md` (`sys.node.clock_state`).

## Test Contract
- Admission policy selection is deterministic.
- Workload routing uses `workload_route` and respects weight.
- Scheduler creates run records for every job execution.
- Job dependencies block execution until satisfied.
- Alert silences suppress matching alerts.
