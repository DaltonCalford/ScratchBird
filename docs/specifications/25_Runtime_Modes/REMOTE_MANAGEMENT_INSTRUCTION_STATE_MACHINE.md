# Remote Management Instruction State Machine

Status: reconstructed_required_with_current_substrate

## Instruction object

Each remote management instruction must contain:
- `instruction_id`
- `instruction_class`
- `target_scope`
- `target_database_uuid`
- `requested_by_principal`
- `queued_at_ms`
- `payload_hash`
- `required_capabilities`
- `precondition_set`
- `local_persistence_required`
- `listener_runtime_action_required`
- `rollback_class`

Supported instruction classes include, at minimum:
- plugin add, remove, enable, disable
- authentication configuration mutation
- security-policy mutation
- memory-budget mutation
- listener topology mutation
- parser-pool policy mutation
- derivative-lane control mutation
- maintenance entry and exit

## Required state machine

Instruction states are:
- `QUEUED`
- `ASSESSED`
- `READY`
- `DISPATCHED`
- `APPLYING`
- `APPLIED`
- `FAILED`
- `QUARANTINED`
- `ROLLED_BACK`
- `SUPERSEDED`
- `CANCELLED`

## State transition rules

`QUEUED -> ASSESSED`
- verify target identity
- verify capability support
- verify privilege scope
- verify dependency ordering
- verify maintenance-window and local refusal conditions

`ASSESSED -> READY`
- all required checks succeeded
- no predecessor instruction remains blocking

`READY -> DISPATCHED`
- cluster layer assigns the instruction to the server-local manager

`DISPATCHED -> APPLYING`
- manager authenticates the instruction source
- engine-owned admin path authorizes and binds the local operation

`APPLYING -> APPLIED`
- local durable apply succeeds where required
- controller and listener runtime work succeeds where required
- outcome is durably recorded for the target

`APPLYING -> FAILED`
- any required local apply or bounded runtime action fails

`FAILED -> QUARANTINED`
- retry safety cannot be proven
- ordering continuity cannot be proven
- or operator policy requires hold before retry

`APPLIED -> ROLLED_BACK`
- only for instruction classes with explicit rollback support

## Required apply route

The authoritative route is:
1. cluster layer queues the instruction
2. cluster layer assesses and marks it `READY`
3. manager receives the instruction
4. engine-owned admin surface validates and authorizes the local action
5. target database persists the accepted local state
6. controller issues bounded listener-management work when needed
7. manager reports the final outcome back to the cluster layer

No instruction may bypass step `4`.

## Dual-record rule

For any accepted instruction that changes durable settings or administrative
state, the system must retain:
- a cluster deployment record
- a target-local durable record in the affected database

File reload alone is not sufficient as the only durable record for a
cluster-managed change.

## Assessment refusal classes

Assessment must fail closed on:
- target mismatch
- unsupported capability
- unauthorized instruction class
- conflicting predecessor instruction
- unsafe heartbeat or degraded local readiness
- unresolved startup quarantine
- unresolved derivative or shadow-group quarantine when that instruction class
  depends on them

## Current-proof versus required-implementation split

Current code in this pass proves only bounded local management, config mutation,
reload, and listener-management dispatch primitives.

The queued deployment object, full state machine, and dual-record durability
model are canonically required reconstructed behavior and must be implemented to
this file.
