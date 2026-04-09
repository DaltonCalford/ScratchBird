# Remote Admin and Deployment Control Surface

Status: reconstructed_required_with_current_substrate

## Scope

This file defines the operator-facing surfaces for:
- remote management assessment
- remote deployment dispatch
- remote deployment inspection
- server heartbeat and readiness inspection

These surfaces are client and operator tools over engine-owned and
cluster-managed authority.

They do not grant direct control over the listener.

Beta 1 package `07` boundary:

- this file is consumed here only for local single-target remote-admin status,
  assess, and apply surfaces
- multi-target cluster queue inspection, cross-node drift management, and
  distributed deployment control remain outside this package and must fail
  closed

## Required operator capabilities

Operator tooling must support:
- inspect server heartbeat and readiness
- inspect queued, active, failed, and quarantined instructions
- submit assess-only management requests
- submit privileged apply requests
- inspect target-local vs cluster deployment drift
- release or acknowledge quarantined instructions where policy allows

Inspection privilege is separate from mutation privilege.

## Required result shapes

### Server status result

Remote admin tooling must expose a deterministic server-status row with:
- `server_uuid`
- `manager_uuid`
- `owner_database_uuid`
- `heartbeat_state`
- `heartbeat_sequence`
- `config_generation`
- `controller_reachable`
- `listener_control_reachable`
- `parser_pool_ready`
- `parser_pool_warm`
- `derivative_backpressure_class`
- `shadow_group_state`
- `last_instruction_id`
- `last_instruction_state`

### Instruction queue result

Tooling must expose a deterministic instruction-status row with:
- `instruction_id`
- `instruction_class`
- `target_database_uuid`
- `requested_by_principal`
- `queued_at_ms`
- `state`
- `assessment_result`
- `local_generation_before`
- `local_generation_after`
- `last_error_code`
- `last_error_text`

### Drift result

Tooling must expose a deterministic deployment-drift row with:
- `target_database_uuid`
- `cluster_generation`
- `local_generation`
- `drift_state`
- `last_applied_instruction_id`
- `last_failed_instruction_id`

For the bounded Beta 1 single-target lane, `cluster_generation` is permitted to
collapse to the admitted local management generation or a fixed single-target
envelope identity. It must not imply multi-target cluster consensus.

## Required command families

The tooling layer must support, at minimum:
- heartbeat inspection
- queued instruction inspection
- assess-only deployment submission
- privileged apply submission
- quarantined-instruction inspection
- drift inspection

Tooling may present these through SQL, administrative RPC, CLI, or GUI
surfaces, but every presentation must route into the same underlying
engine-authorized management model.

## Hard rules

- no tool may address the listener directly as the source of topology truth
- no tool may treat successful transport authentication as deployment
  authorization by itself
- no tool may hide target-local failure when cluster dispatch succeeded but
  local durable apply failed

## Current-proof versus required-implementation split

Current code proves bounded status and admin surfaces.

The full remote heartbeat, instruction queue, and drift inspection tool
contracts are canonically required reconstructed behavior and must be
implemented to this file, but package `07` closes only the local single-target
subset of those surfaces.
