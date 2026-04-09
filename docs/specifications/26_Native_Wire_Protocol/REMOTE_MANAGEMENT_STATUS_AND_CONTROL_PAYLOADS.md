# Remote Management Status and Control Payloads

Status: reconstructed_required_with_current_substrate

## Scope

This file defines the public engine-facing payload contracts for remote
management inspection and control.

The manager heartbeat bus is not a public client protocol.
Public inspection and control must surface through fixed result schemas over the
existing engine protocol surfaces.

## Transport rule

Remote management inspection and control must use:
- ordinary statement result rows
- existing status or result payload families already owned by the engine

No new public listener-control transport family is introduced by this file.

## Required fixed row contracts

### Heartbeat status row

The canonical row must contain:
- `server_uuid`
- `manager_uuid`
- `owner_database_uuid`
- `heartbeat_state`
- `heartbeat_sequence`
- `emitted_at_ms`
- `config_generation`
- `controller_reachable`
- `listener_control_reachable`
- `parser_pool_ready`
- `parser_pool_warm`
- `derivative_backpressure_class`
- `shadow_group_state`
- `shadow_group_ready_members`
- `shadow_group_required_members`
- `last_instruction_id`
- `last_instruction_state`

### Instruction state row

The canonical row must contain:
- `instruction_id`
- `instruction_class`
- `target_database_uuid`
- `requested_by_principal`
- `queued_at_ms`
- `state`
- `assessment_result`
- `last_error_code`
- `last_error_text`
- `local_generation_before`
- `local_generation_after`

### Drift row

The canonical row must contain:
- `target_database_uuid`
- `cluster_generation`
- `local_generation`
- `drift_state`
- `last_applied_instruction_id`
- `last_failed_instruction_id`

## Required failure codes

Management inspection and control surfaces must expose stable failure codes for:
- `REMOTE_MGMT_DISABLED`
- `REMOTE_MGMT_UNAUTHORIZED`
- `REMOTE_MGMT_TARGET_MISMATCH`
- `REMOTE_MGMT_PRECONDITION_FAILED`
- `REMOTE_MGMT_HEARTBEAT_DEGRADED`
- `REMOTE_MGMT_DISPATCH_FAILED`
- `REMOTE_MGMT_APPLY_FAILED`
- `REMOTE_MGMT_QUARANTINED`

## Binding rules

- public remote-management payloads are engine-owned
- manager-private heartbeat traffic is not directly replayed to clients
- listener-local `STATUS` is not the canonical heartbeat payload
- controller aggregation may contribute fields, but engine-owned surfaces define
  the public row shape

## Current-proof versus required-implementation split

Current code now proves deterministic bounded manager-status rows and
engine-status payload fields for heartbeat, readiness, and bounded drift or
queue posture.

When the bounded manager substrate has no queued remote instruction state to
report, the canonical queue and drift fields remain present with deterministic
zero or default values instead of falling back to free-form text.
