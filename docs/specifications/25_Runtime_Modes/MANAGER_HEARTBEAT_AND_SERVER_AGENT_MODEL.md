# Manager Heartbeat and Server Agent Model

Status: reconstructed_required_with_current_substrate

## Current code-backed proof

Current code in this pass proves:
- the optional manager fronts the local native listener
- the manager performs local readiness and binding validation using `DBBT` and
  `LPREFACE`
- server-local readiness terms exist for listener, parser-pool, and control
  plane availability
- bounded clock-skew validation exists for local binding validation
- structured manager inspection rows publish heartbeat identity, readiness, and
  bounded drift or queue posture over `STATUS_RESPONSE`
- the listener management seam publishes parser-pool readiness and local
  control reachability to the manager

Current code in this pass does not yet promote a separate cluster membership
transport beyond that bounded manager-owned inspection surface.

## Required reconstructed ownership model

The manager is the server-local cluster agent for the server it fronts.

The manager owns:
- heartbeat emission to the cluster layer
- server-local readiness testing for cluster assessment
- receipt of remote management instructions
- dispatch of approved work into engine-owned and controller-owned seams
- reporting of deployment outcome, refusal, and quarantine state

The listener does not own cluster identity, cluster membership, or cluster
truth.

The engine does not use the heartbeat bus as the source of MGA durability
truth.

## Server heartbeat state

Heartbeat state values are:
- `STARTING`
- `READY`
- `DEGRADED`
- `QUIESCING`
- `QUARANTINED`
- `STOPPED`

State transitions are driven by:
- manager startup and shutdown
- controller reachability
- listener reachability
- parser-pool readiness
- local startup quarantine
- deployment failure or quarantine

## Required heartbeat payload

Each heartbeat frame must contain, at minimum:
- `manager_uuid`
- `server_uuid`
- `owner_database_uuid`
- `heartbeat_state`
- `heartbeat_sequence`
- `emitted_at_ms`
- `config_generation`
- `last_instruction_id`
- `last_instruction_state`
- `controller_reachable`
- `listener_control_reachable`
- `listener_family_inventory`
- `listener_binding_inventory`
- `parser_pool_min`
- `parser_pool_max`
- `parser_pool_warm`
- `parser_pool_ready`
- `startup_quarantine_active`
- `derivative_backpressure_class`
- `shadow_group_state`
- `shadow_group_ready_members`
- `shadow_group_required_members`
- `software_capability_bitmap`

## Heartbeat emission rules

The manager must emit heartbeat:
- on startup
- on transition between heartbeat states
- after successful or failed deployment assessment
- after successful or failed instruction application
- on the configured periodic interval while running

Required emission order is:
1. gather engine-owned state snapshot
2. gather controller and listener-local state snapshot
3. gather derivative and shadow-group summary state
4. stamp `config_generation` and instruction watermark
5. assign `heartbeat_sequence`
6. emit the frame to the cluster layer

## Assessment and health-test duties

The manager must support server-local test classes for:
- controller reachability
- listener management reachability
- listener family and bind inventory validation
- parser-pool readiness
- owner-database identity binding
- derivative backlog classification
- shadow-group readiness classification

These tests are server-local assessment tools. They do not redefine MGA
durability truth.

## Fail-closed rules

- stale or missing heartbeat must not be interpreted as local data loss
- stale or missing heartbeat must block unsafe remote deployment classes
- the cluster layer may quarantine a server on missing heartbeat, but the
  manager heartbeat is not recovery authority
- the listener may not emit cluster heartbeat independently of the manager

## Current-proof versus required-implementation split

Current code proves the manager front-door, local validation prerequisites, and
the bounded Beta 1 manager inspection row required for server-local heartbeat
and readiness publication.

Later cluster transport or membership-consumption work must extend this same
manager-owned contract. It must not move heartbeat authority to the listener or
introduce a separate durability truth source.

## Audit lookup anchors

Representative audit anchors for this file are:
- `sendListenerManagementCommand(`
- `LPREFACE_VALIDATE`
- `appendManagerInspectionEntries(`
