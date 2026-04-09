# Catalog: Clock, SLO, and Autoscale Tables

## Purpose
Define canonical on-disk tables for cluster clock policy, SLO policy, autoscale actions, and admission tuning evidence.

## Scope
- Clock discipline and skew policy storage.
- SLO target, burn-rate, and action storage.
- Autoscale and admission tuning policy/evidence storage.
- Canonical storage contract used by section 25 runtime algorithms.

## Conventions
- All columns use catalog domains from `CATALOG_SYSTEM_DOMAINS.md`.
- Enum columns use `[sb_dom]cat_enum_<enum_kind>` where available.
- If a dedicated enum kind is not used, identifier columns include fixed allowed-label constraints.
- All timestamps are UTC (`[sb_dom]cat_timestamp`).

## Fixed Label Sets Used In This Document
These labels are fixed and case-sensitive.

### `clock_source_kind`
- `NTP`
- `PTP`
- `PEER_MEDIAN`

### `clock_state_label`
- `HEALTHY`
- `WARN`
- `SOFT_SKEW`
- `HARD_SKEW`
- `STALE`

### `clock_action_taken`
- `NONE`
- `DEGRADE_WEIGHT`
- `READ_ONLY`
- `QUARANTINE`
- `FENCE_WRITES`

### `slo_burn_severity`
- `NONE`
- `MODERATE`
- `HIGH`
- `CRITICAL`

### `slo_action_plan`
- `NONE`
- `ADMISSION_TIGHTEN`
- `SCALE_OUT`
- `SCALE_OUT_AND_TIGHTEN`
- `INCIDENT_PAGE`

### `autoscale_action_kind`
- `SCALE_OUT`
- `SCALE_IN`
- `NO_OP`

### `autoscale_action_state`
- `PENDING`
- `APPLIED`
- `FAILED`
- `CANCELLED`

## Table: `clock_policy`
Columns:
- `clock_policy_uuid` `[sb_dom]cat_clock_policy_uuid` PK
- `policy_name` `[sb_dom]cat_identifier`
- `warn_skew_ms` `[sb_dom]cat_uint32`
- `soft_skew_ms` `[sb_dom]cat_uint32`
- `hard_skew_ms` `[sb_dom]cat_uint32`
- `max_jitter_ms` `[sb_dom]cat_uint32`
- `sample_interval_ms` `[sb_dom]cat_uint32`
- `stale_after_ms` `[sb_dom]cat_uint32`
- `skew_guard_ms` `[sb_dom]cat_uint32`
- `node_quarantine_on_hard_skew` `[sb_dom]cat_bool`
- `version_u64` `[sb_dom]cat_version_u64`
- `created_time` `[sb_dom]cat_timestamp`
- `last_modified_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`policy_name`)
- `warn_skew_ms < soft_skew_ms < hard_skew_ms`.
- `sample_interval_ms > 0`.
- `stale_after_ms >= sample_interval_ms`.

## Table: `clock_source`
Columns:
- `clock_source_uuid` `[sb_dom]cat_clock_source_uuid` PK
- `clock_policy_uuid` `[sb_dom]cat_clock_policy_uuid`
- `source_kind` `[sb_dom]cat_identifier`
- `endpoint` `[sb_dom]cat_text`
- `priority_rank` `[sb_dom]cat_uint16`
- `is_enabled` `[sb_dom]cat_bool`
- `last_probe_utc` `[sb_dom]cat_timestamp` nullable
- `last_probe_offset_ms` `[sb_dom]cat_int32` nullable
- `last_probe_jitter_ms` `[sb_dom]cat_uint32` nullable
- `version_u64` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`clock_policy_uuid`, `priority_rank`)
- `source_kind` must be one of: `NTP`, `PTP`, `PEER_MEDIAN`.

## Table: `sys.node.clock_state`
Columns:
- `node_clock_state_uuid` `[sb_dom]cat_node_clock_state_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid` UNIQUE
- `clock_policy_uuid` `[sb_dom]cat_clock_policy_uuid`
- `clock_state` `[sb_dom]cat_identifier`
- `offset_ms` `[sb_dom]cat_int32`
- `jitter_ms` `[sb_dom]cat_uint32`
- `sample_count` `[sb_dom]cat_uint32`
- `last_sync_utc` `[sb_dom]cat_timestamp`
- `last_transition_utc` `[sb_dom]cat_timestamp`
- `logical_counter` `[sb_dom]cat_uint32`
- `version_u64` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `clock_state` must be one of: `HEALTHY`, `WARN`, `SOFT_SKEW`, `HARD_SKEW`, `STALE`.

## Table: `clock_violation_event`
Columns:
- `clock_violation_event_uuid` `[sb_dom]cat_clock_violation_event_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `clock_policy_uuid` `[sb_dom]cat_clock_policy_uuid`
- `clock_state` `[sb_dom]cat_identifier`
- `offset_ms` `[sb_dom]cat_int32`
- `jitter_ms` `[sb_dom]cat_uint32`
- `action_taken` `[sb_dom]cat_identifier`
- `event_time_utc` `[sb_dom]cat_timestamp`
- `resolved_time_utc` `[sb_dom]cat_timestamp` nullable
- `version_u64` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `clock_state` must be one of: `WARN`, `SOFT_SKEW`, `HARD_SKEW`, `STALE`.
- `action_taken` must be one of: `NONE`, `DEGRADE_WEIGHT`, `READ_ONLY`, `QUARANTINE`, `FENCE_WRITES`.

## Table: `slo_profile`
Columns:
- `slo_profile_uuid` `[sb_dom]cat_slo_profile_uuid` PK
- `profile_name` `[sb_dom]cat_identifier`
- `role_name` `[sb_dom]cat_enum_node_role`
- `availability_target_pct` `[sb_dom]cat_f64`
- `latency_p95_target_ms` `[sb_dom]cat_uint32`
- `latency_p99_target_ms` `[sb_dom]cat_uint32`
- `error_rate_target_pct` `[sb_dom]cat_f64`
- `window_minutes` `[sb_dom]cat_uint32`
- `short_burn_window_minutes` `[sb_dom]cat_uint32`
- `long_burn_window_minutes` `[sb_dom]cat_uint32`
- `critical_burn_threshold` `[sb_dom]cat_f64`
- `high_burn_threshold` `[sb_dom]cat_f64`
- `moderate_burn_threshold` `[sb_dom]cat_f64`
- `version_u64` `[sb_dom]cat_version_u64`
- `is_active` `[sb_dom]cat_bool`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`profile_name`)
- `latency_p95_target_ms <= latency_p99_target_ms`.
- `moderate_burn_threshold <= high_burn_threshold <= critical_burn_threshold`.

## Table: `slo_binding`
Columns:
- `slo_binding_uuid` `[sb_dom]cat_slo_binding_uuid` PK
- `slo_profile_uuid` `[sb_dom]cat_slo_profile_uuid`
- `node_uuid` `[sb_dom]cat_node_uuid` nullable
- `role_name` `[sb_dom]cat_enum_node_role`
- `priority_rank` `[sb_dom]cat_uint16`
- `effective_from_utc` `[sb_dom]cat_timestamp`
- `effective_to_utc` `[sb_dom]cat_timestamp` nullable
- `version_u64` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`slo_profile_uuid`, `node_uuid`, `role_name`, `effective_from_utc`)
- When `node_uuid` is null, row is role default.

## Table: `slo_window`
Columns:
- `slo_window_uuid` `[sb_dom]cat_slo_window_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `role_name` `[sb_dom]cat_enum_node_role`
- `window_start_utc` `[sb_dom]cat_timestamp`
- `window_end_utc` `[sb_dom]cat_timestamp`
- `request_count` `[sb_dom]cat_uint64`
- `success_count` `[sb_dom]cat_uint64`
- `error_count` `[sb_dom]cat_uint64`
- `latency_p95_ms` `[sb_dom]cat_uint32`
- `latency_p99_ms` `[sb_dom]cat_uint32`
- `availability_sli_pct` `[sb_dom]cat_f64`
- `error_rate_sli_pct` `[sb_dom]cat_f64`
- `version_u64` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `window_end_utc > window_start_utc`.
- `success_count + error_count <= request_count`.

## Table: `slo_burn_event`
Columns:
- `slo_burn_event_uuid` `[sb_dom]cat_slo_burn_event_uuid` PK
- `node_uuid` `[sb_dom]cat_node_uuid`
- `role_name` `[sb_dom]cat_enum_node_role`
- `slo_profile_uuid` `[sb_dom]cat_slo_profile_uuid`
- `short_burn_rate` `[sb_dom]cat_f64`
- `long_burn_rate` `[sb_dom]cat_f64`
- `burn_severity` `[sb_dom]cat_identifier`
- `action_plan` `[sb_dom]cat_identifier`
- `event_time_utc` `[sb_dom]cat_timestamp`
- `resolved_time_utc` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `burn_severity` must be one of: `NONE`, `MODERATE`, `HIGH`, `CRITICAL`.
- `action_plan` must be one of: `NONE`, `ADMISSION_TIGHTEN`, `SCALE_OUT`, `SCALE_OUT_AND_TIGHTEN`, `INCIDENT_PAGE`.

## Table: `autoscale_policy`
Columns:
- `autoscale_policy_uuid` `[sb_dom]cat_autoscale_policy_uuid` PK
- `role_name` `[sb_dom]cat_enum_node_role`
- `min_nodes` `[sb_dom]cat_uint16`
- `max_nodes` `[sb_dom]cat_uint16`
- `scale_out_step` `[sb_dom]cat_uint16`
- `scale_in_step` `[sb_dom]cat_uint16`
- `scale_out_cooldown_ms` `[sb_dom]cat_duration_ms`
- `scale_in_cooldown_ms` `[sb_dom]cat_duration_ms`
- `cpu_scale_out_pct` `[sb_dom]cat_percent_u8`
- `queue_scale_out_pct` `[sb_dom]cat_percent_u8`
- `slo_burn_scale_out_threshold` `[sb_dom]cat_f64`
- `slo_recovery_scale_in_threshold` `[sb_dom]cat_f64`
- `version_u64` `[sb_dom]cat_version_u64`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`role_name`)
- `min_nodes <= max_nodes`.
- `scale_out_step > 0` and `scale_in_step > 0`.

## Table: `autoscale_action`
Columns:
- `autoscale_action_uuid` `[sb_dom]cat_autoscale_action_uuid` PK
- `role_name` `[sb_dom]cat_enum_node_role`
- `action_kind` `[sb_dom]cat_identifier`
- `requested_count_delta` `[sb_dom]cat_int16`
- `applied_count_delta` `[sb_dom]cat_int16`
- `trigger_reason` `[sb_dom]cat_text`
- `trigger_burn_rate` `[sb_dom]cat_f64`
- `policy_version_u64` `[sb_dom]cat_version_u64`
- `action_time_utc` `[sb_dom]cat_timestamp`
- `completed_time_utc` `[sb_dom]cat_timestamp` nullable
- `action_state` `[sb_dom]cat_identifier`
- `failure_code` `[sb_dom]cat_identifier` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `action_kind` must be one of: `SCALE_OUT`, `SCALE_IN`, `NO_OP`.
- `action_state` must be one of: `PENDING`, `APPLIED`, `FAILED`, `CANCELLED`.

## Table: `admission_tuning_event`
Columns:
- `admission_tuning_event_uuid` `[sb_dom]cat_admission_tuning_event_uuid` PK
- `role_name` `[sb_dom]cat_enum_node_role`
- `old_max_concurrent_queries` `[sb_dom]cat_uint32`
- `new_max_concurrent_queries` `[sb_dom]cat_uint32`
- `old_max_queue_depth` `[sb_dom]cat_uint32`
- `new_max_queue_depth` `[sb_dom]cat_uint32`
- `old_queue_timeout_ms` `[sb_dom]cat_uint32`
- `new_queue_timeout_ms` `[sb_dom]cat_uint32`
- `reason` `[sb_dom]cat_text`
- `policy_version_u64` `[sb_dom]cat_version_u64`
- `event_time_utc` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- `new_max_concurrent_queries > 0`.
- `new_max_queue_depth > 0`.
- `new_queue_timeout_ms > 0`.

## Integration Requirements
- Section 25 algorithms MUST read/write only these tables for clock/SLO/autoscale/admission policy state.
- Section 20 diagnostics and section 31 reliability gates MUST use these tables for evidence extraction.

## Test Contract Additions
- `T24-RUNTIME-POLICY-01`: all fixed label constraints reject invalid values.
- `T24-RUNTIME-POLICY-02`: clock threshold ordering constraints are enforced.
- `T24-RUNTIME-POLICY-03`: SLO threshold ordering constraints are enforced.
- `T24-RUNTIME-POLICY-04`: autoscale min/max and step constraints are enforced.
- `T24-RUNTIME-POLICY-05`: admission tuning rows cannot persist invalid non-positive limits.
