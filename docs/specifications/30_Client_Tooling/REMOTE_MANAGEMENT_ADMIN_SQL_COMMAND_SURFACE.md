# Remote Management Admin SQL Command Surface

Status: reconstructed_required_with_current_substrate

## Scope

This file defines the operator-facing command contract for remote management
through SQL-admin and equivalent tooling surfaces.

The canonical command family is native-dialect and engine-owned.

## Required commands

### Inspection commands

- `SHOW MANAGEMENT SERVERS`
- `SHOW MANAGEMENT INSTRUCTIONS`
- `SHOW MANAGEMENT DRIFT`

### Assessment and dispatch commands

- `ALTER SYSTEM ASSESS REMOTE SET <section.key> = <value> ON SERVER <uuid>`
- `ALTER SYSTEM APPLY INSTRUCTION <uuid>`
- `ALTER SYSTEM CANCEL INSTRUCTION <uuid>`
- `ALTER SYSTEM QUARANTINE INSTRUCTION <uuid>`
- `ALTER SYSTEM ACKNOWLEDGE INSTRUCTION <uuid>`

## Required semantics

### `SHOW MANAGEMENT SERVERS`

Returns one row per managed server.

Required columns are the heartbeat-status contract from section `26`.

### `SHOW MANAGEMENT INSTRUCTIONS`

Returns queued, active, failed, quarantined, cancelled, superseded, and applied
instructions.

Required columns are the instruction-state contract from section `26`.

### `SHOW MANAGEMENT DRIFT`

Returns one row per managed target that has cluster/local generation state.

Required columns are the drift contract from section `26`.

### `ALTER SYSTEM ASSESS REMOTE SET`

Creates or refreshes an instruction in assessed or ready state without
claiming that the change was already applied.

The command must fail closed when:
- the target UUID is invalid
- the caller lacks management privilege
- the key is not a promoted remote-managed key
- readiness or heartbeat state forbids safe dispatch

### `ALTER SYSTEM APPLY INSTRUCTION`

Dispatches a prepared instruction to the target server-local manager.

The command must not return success if local durable apply later fails.

### `ALTER SYSTEM CANCEL INSTRUCTION`

Cancels only instructions that have not reached irreversible local apply.

### `ALTER SYSTEM QUARANTINE INSTRUCTION`

Places the instruction or target into a hold state requiring explicit operator
attention.

### `ALTER SYSTEM ACKNOWLEDGE INSTRUCTION`

Acknowledges a quarantined or failed instruction without silently changing its
apply history.

## Output rule

All mutating commands must return the deterministic command-status row defined
by section `28`.

Tooling may render the row in CLI, UI, or API form, but must not change the
column meaning.

## Current-proof versus required-implementation split

Current code proves only bounded admin statements such as `ALTER SYSTEM SET`
and config reload.

The remote-management SQL-admin command family defined here is canonically
required reconstructed behavior and must be implemented to this file.
