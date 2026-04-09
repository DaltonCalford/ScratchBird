# Admin SQL Remote Management Binding and Result Contract

Status: reconstructed_required_with_current_substrate

## Current code-backed parser authority

Current code in this pass proves native parser and SBLR support for:
- `ALTER SYSTEM SET section.key = value`
- `CONFIG RELOAD`
- `CONFIG HISTORY`

These are the current administrative parsing footholds.

## Required reconstructed admin-SQL family

The native parser must own deterministic normalization for the remote
management-admin family.

Required canonical statement families are:
- `SHOW MANAGEMENT SERVERS`
- `SHOW MANAGEMENT INSTRUCTIONS`
- `SHOW MANAGEMENT DRIFT`
- `ALTER SYSTEM ASSESS REMOTE SET <section.key> = <value> ON SERVER <uuid>`
- `ALTER SYSTEM APPLY INSTRUCTION <uuid>`
- `ALTER SYSTEM CANCEL INSTRUCTION <uuid>`
- `ALTER SYSTEM QUARANTINE INSTRUCTION <uuid>`
- `ALTER SYSTEM ACKNOWLEDGE INSTRUCTION <uuid>`

These statements are native-only until an emulated profile explicitly maps
them.

## Required parser normalization rules

The parser must:
- classify these statements in one remote-management control family
- produce one canonical feature key per statement family
- bind server and database targets by UUID form only
- reject heuristic target-name resolution
- reject missing or ambiguous `section.key`
- reject missing target or instruction UUIDs
- reject unsupported instruction classes before SBLR emission

## Required feature keys

Canonical feature keys are:
- `F_REMOTE_MGMT_SERVERS_SHOW`
- `F_REMOTE_MGMT_INSTRUCTIONS_SHOW`
- `F_REMOTE_MGMT_DRIFT_SHOW`
- `F_REMOTE_MGMT_ASSESS_SET`
- `F_REMOTE_MGMT_APPLY_INSTRUCTION`
- `F_REMOTE_MGMT_CANCEL_INSTRUCTION`
- `F_REMOTE_MGMT_QUARANTINE_INSTRUCTION`
- `F_REMOTE_MGMT_ACKNOWLEDGE_INSTRUCTION`

## Required bound payload fields

### Assessment payload

The assessment payload must contain:
- `instruction_class`
- `target_server_uuid`
- `target_database_uuid` when database scope is required
- `section`
- `key`
- `typed_value`
- `requested_by_uuid`
- `requires_local_persistence`
- `requires_listener_runtime_action`

### Instruction action payload

The action payload must contain:
- `instruction_uuid`
- `requested_by_uuid`
- `action_kind`

## Required result contracts

### `SHOW MANAGEMENT SERVERS`

Must return the fixed heartbeat-status row defined by:
- [REMOTE_MANAGEMENT_STATUS_AND_CONTROL_PAYLOADS.md](docs/specifications/26_Native_Wire_Protocol/REMOTE_MANAGEMENT_STATUS_AND_CONTROL_PAYLOADS.md)
- [REMOTE_ADMIN_AND_DEPLOYMENT_CONTROL_SURFACE.md](docs/specifications/30_Client_Tooling/REMOTE_ADMIN_AND_DEPLOYMENT_CONTROL_SURFACE.md)

### `SHOW MANAGEMENT INSTRUCTIONS`

Must return the fixed instruction-state row defined by those same sections.

### `SHOW MANAGEMENT DRIFT`

Must return the fixed drift row defined by those same sections.

### Mutating statements

Assessment and instruction-action statements must return one deterministic
command-status row containing:
- `instruction_uuid`
- `state`
- `target_database_uuid`
- `target_server_uuid`
- `status_code`
- `status_text`

## Capability and privilege rule

The parser must not widen privilege.

A parsed remote-management statement is only a bound request.
Engine-owned authorization still decides whether it may execute.

## Current-proof versus required-implementation split

Current code proves only the existing `ALTER SYSTEM` and config-control family.

The remote-management admin-SQL family defined here is canonically required
reconstructed behavior and must be implemented to this file.
