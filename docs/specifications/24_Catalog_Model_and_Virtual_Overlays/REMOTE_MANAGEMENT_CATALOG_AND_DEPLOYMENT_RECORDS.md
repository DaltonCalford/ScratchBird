# Remote Management Catalog and Deployment Records

Status: reconstructed_required_with_current_substrate

## Current code-backed authority

Current code in this pass already proves catalog-backed cluster control state
for:
- `cluster_policy`
- `failure_detector`
- cluster-fabric link and session rows with heartbeat and readiness fields
- target-local scalar configuration rows in `sys.config.value` plus durable
  history in `sys.config.change_log`
- target-local dedicated listener-topology rows in `listener_profile`,
  `listener_binding`, `listener_emulation_binding`, `parser_pool_policy`,
  `listener_runtime_target`, and `listener_generation_record`

Current code proves:
- physical catalog allocation for `cluster_policy` and `failure_detector`
- upsert, lookup, list, and invalidate semantics for failure-detector rows
- catalog fields for heartbeat interval, thresholds, and last-heartbeat style
  timestamps in existing cluster-fabric and related catalog records

Current code in this pass does not yet prove the promoted remote-management
instruction queue or deployment-history catalog defined below.

## Target-local durable settings rule

Accepted remote-management changes must land in one of two target-local durable
families:
- scalar configuration rows in `sys.config.value` plus history in
  `sys.config.change_log`
- dedicated listener-topology rows in `listener_profile`, `listener_binding`,
  `listener_emulation_binding`, `parser_pool_policy`,
  `listener_runtime_target`, and `listener_generation_record`

No accepted instruction may leave target-local durable state ambiguous between
generic scalar key space and dedicated listener-topology families.

## Beta 1 package `07` boundary

For the bounded Beta 1 single-target lane:

- target-local durable settings remain mandatory current authority
- a local single-target management history row or status record may be used for
  admitted assess and apply visibility
- the full promoted cluster instruction, target fanout, and cluster-history
  catalog family below remains future-only for this package and must not be
  implied as shipped runtime support

## Required reconstructed catalog model

The remote management plane must use catalog-backed dual-record persistence.

For every accepted remote administrative change, the system must retain:
- cluster deployment history
- target-local durable management state inside the affected database

The required canonical tables are:
- `remote_mgmt_instruction`
- `remote_mgmt_instruction_target`
- `remote_mgmt_instruction_event`
- `remote_mgmt_target_generation`
- `remote_mgmt_capability_snapshot`

## Table: `remote_mgmt_instruction`

Columns:
- `instruction_uuid` `[sb_dom]cat_uuid` PK
- `instruction_class` `[sb_dom]cat_identifier`
- `requested_by_uuid` `[sb_dom]cat_user_uuid`
- `requested_at` `[sb_dom]cat_timestamp`
- `assessment_state` `[sb_dom]cat_identifier`
- `dispatch_state` `[sb_dom]cat_identifier`
- `payload_sblr_uuid` `[sb_dom]cat_uuid` nullable
- `payload_json_uuid` `[sb_dom]cat_uuid` nullable
- `payload_hash` `[sb_dom]cat_text`
- `required_capability_mask` `[sb_dom]cat_uint64`
- `requires_local_persistence` `[sb_dom]cat_bool`
- `requires_listener_runtime_action` `[sb_dom]cat_bool`
- `rollback_class` `[sb_dom]cat_identifier` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- exactly one of `payload_sblr_uuid` or `payload_json_uuid` must be non-null

## Table: `remote_mgmt_instruction_target`

Columns:
- `instruction_target_uuid` `[sb_dom]cat_uuid` PK
- `instruction_uuid` `[sb_dom]cat_uuid`
- `target_database_uuid` `[sb_dom]cat_uuid`
- `target_server_uuid` `[sb_dom]cat_uuid`
- `target_scope_kind` `[sb_dom]cat_identifier`
- `precondition_summary_uuid` `[sb_dom]cat_uuid` nullable
- `local_generation_before` `[sb_dom]cat_uint64`
- `local_generation_after` `[sb_dom]cat_uint64` nullable
- `last_state` `[sb_dom]cat_identifier`
- `last_error_code` `[sb_dom]cat_identifier` nullable
- `last_error_text_uuid` `[sb_dom]cat_uuid` nullable
- `assessed_at` `[sb_dom]cat_timestamp` nullable
- `dispatched_at` `[sb_dom]cat_timestamp` nullable
- `applied_at` `[sb_dom]cat_timestamp` nullable
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`instruction_uuid`, `target_database_uuid`)

## Table: `remote_mgmt_instruction_event`

Columns:
- `instruction_event_uuid` `[sb_dom]cat_uuid` PK
- `instruction_uuid` `[sb_dom]cat_uuid`
- `target_database_uuid` `[sb_dom]cat_uuid`
- `event_seq` `[sb_dom]cat_uint64`
- `event_state` `[sb_dom]cat_identifier`
- `event_code` `[sb_dom]cat_identifier` nullable
- `event_detail_uuid` `[sb_dom]cat_uuid` nullable
- `event_time` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`instruction_uuid`, `target_database_uuid`, `event_seq`)

## Table: `remote_mgmt_target_generation`

Columns:
- `target_generation_uuid` `[sb_dom]cat_uuid` PK
- `target_database_uuid` `[sb_dom]cat_uuid`
- `cluster_generation` `[sb_dom]cat_uint64`
- `local_generation` `[sb_dom]cat_uint64`
- `last_applied_instruction_uuid` `[sb_dom]cat_uuid` nullable
- `last_failed_instruction_uuid` `[sb_dom]cat_uuid` nullable
- `drift_state` `[sb_dom]cat_identifier`
- `observed_at` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

Constraints:
- UNIQUE(`target_database_uuid`)

## Table: `remote_mgmt_capability_snapshot`

Columns:
- `capability_snapshot_uuid` `[sb_dom]cat_uuid` PK
- `target_database_uuid` `[sb_dom]cat_uuid`
- `server_uuid` `[sb_dom]cat_uuid`
- `config_generation` `[sb_dom]cat_uint64`
- `capability_mask` `[sb_dom]cat_uint64`
- `heartbeat_state` `[sb_dom]cat_identifier`
- `controller_reachable` `[sb_dom]cat_bool`
- `listener_control_reachable` `[sb_dom]cat_bool`
- `parser_pool_ready` `[sb_dom]cat_bool`
- `derivative_backpressure_class` `[sb_dom]cat_identifier`
- `shadow_group_state` `[sb_dom]cat_identifier`
- `captured_at` `[sb_dom]cat_timestamp`
- `is_valid` `[sb_dom]cat_bool`

## Dual-record persistence rule

Remote-management durability requires two records:
- cluster deployment record in the remote-management catalog model
- target-local durable settings or deployment state in the affected database,
  using the scalar or dedicated listener-topology families above

Neither the manager nor the listener may be treated as the only durable record
for an accepted change.

`remote_mgmt_instruction_target.local_generation_before` and
`remote_mgmt_instruction_target.local_generation_after` must refer to the
target-local generation published by the affected scalar or listener-topology
family, not to a transient in-memory apply counter.

## MGA rule

All remote-management catalog writes are transaction-scoped.
Assessment, apply, failure, quarantine, rollback, and drift rows are committed
or rolled back under the same always-in-transaction MGA model as any other
catalog mutation.

## Current-proof versus required-implementation split

Current code proves the cluster-policy and failure-detector substrate together
with the target-local scalar and dedicated listener-topology durability split.

The remaining remote-management instruction, deployment-event, and
target-generation catalogs defined here are canonically required reconstructed
behavior beyond the now-implemented target-local persistence split above.

Package `07` consumes this file to preserve the target-local durability split
and to keep the larger cluster catalog model explicitly out of the bounded Beta
1 implementation promise.
