# Cluster Scheduler and Maintenance

Status: reconstructed_required_with_current_substrate

## Current code-backed authority

Current proof is limited to:
- local scheduler-backed maintenance orchestration
- governance-aware admission around local maintenance execution
- server-local health components that expose whether the listener or parser pool
  is available
- server-local health components that expose whether the control plane is
  reachable
- native manager proxying
- readiness checks against the internal native listener
- binding validation using `DBBT` and `LPREFACE`

These are the local execution prerequisites for the larger cluster-management
lane. They are not, by themselves, a complete distributed scheduler.

## Required reconstructed cluster-management layer

The cluster layer is the remote administrative dispatcher for deployed
ScratchBird servers.

For current rebuild purposes, this file is authoritative for the management
rollout scheduler even where cross-node query placement or broader cluster
runtime parity is still incomplete.

The required scheduler responsibilities are:
- queue remote management instructions
- assess target readiness before dispatch
- sequence dependent changes in a safe order
- track deployment status per target
- hold, retry, quarantine, or roll back instructions according to policy
- expose remote query and assessment surfaces to operators

Instruction classes controlled through this layer include:
- plugin installation or removal
- authentication and security policy changes
- memory allocation and budget changes
- listener topology changes
- parser pool resizing
- maintenance entry and exit
- derivative-lane controls
- any other promoted engine-management setting

## Server-local manager role

The optional manager is the required owner for server-local heartbeat, health
test, remote instruction intake, and dispatch for the server it fronts.

The manager must:
- receive management work from the cluster layer
- confirm local readiness
- hand approved changes to the engine-owned admin surface
- hand bounded listener work to the controller when required
- return precise deployment and refusal outcomes

The manager must not:
- become the durable source of configuration truth
- apply privileged changes without engine authorization
- invent local success for actions the engine or controller did not accept

## Required deployment model

For every remotely managed change, the system must support:
- queued instruction identity
- target identity and target scope
- assessment result
- dispatch timestamp and execution watermark
- local durable apply confirmation
- rollback or quarantine state where relevant
- operator-queryable outcome history

Local apply must be reflected both:
- in the target database as local durable state
- in the cluster layer as deployment and audit state

## Current-proof versus required-implementation split

Current code in this pass does not yet prove:
- a full manager heartbeat bus
- cross-node scheduler arbitration
- full remote deployment queue execution

These remain implementation gaps, not specification gaps.

## Non-authority

This file is still not authority for:
- generic cluster gossip design beyond the manager-owned server heartbeat lane
- speculative cross-node query-placement algorithms
- unsupported distributed runtime claims that are not yet promoted into canon
