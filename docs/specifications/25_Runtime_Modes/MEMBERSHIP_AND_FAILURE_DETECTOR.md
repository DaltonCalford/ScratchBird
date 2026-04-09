# Membership and Failure Detector

Status: reconstructed_required_with_current_substrate

## Current code-backed authority

Current code in this pass proves only the server-local prerequisites for a
manager-owned cluster lane:
- the optional manager fronts the local native listener
- the manager performs local readiness and binding checks before proxying
- the manager and listener use `DBBT` and `LPREFACE` validation to prove the
  target database and listener pairing
- server-local readiness components expose whether the listener or parser pool
  is available
- server-local readiness components expose whether the control plane is
  reachable
- bounded skew checks exist for the local binding-validation path

Current code in this pass does not prove a full cross-node heartbeat bus,
distributed membership store, or remote deployment queue.

## Required reconstructed specification

The cluster layer is the remote management and server-membership control plane
for deployed ScratchBird servers.

The manager process is the server-local cluster agent for the server it fronts.
The listener is not a cluster member and does not own cluster truth.

Required manager-owned responsibilities are:
- emit server heartbeat to the cluster layer
- run server-local health and readiness tests on behalf of the cluster layer
- receive remote management instructions from the cluster layer
- dispatch approved instructions into bounded engine-owned or controller-owned
  execution seams
- return deployment, readiness, and failure status to the cluster layer

Required heartbeat payload classes are:
- manager identity
- owner database identity
- controller reachability
- listener family and port inventory
- parser pool readiness and warm capacity
- derivative backlog and shadow-group readiness summaries
- software capability inventory
- configuration deployment watermark
- last instruction outcome and quarantine state

## Unified remote management model

The cluster layer must allow remote operators to:
- queue management instructions
- query queued and historical instructions
- assess whether a target server can apply a change
- deploy approved instructions
- inspect outcome, drift, and refusal reasons remotely

Remote management scope includes, at minimum:
- listener topology and emulation families
- listener bind policy and port allocation
- parser pool sizing and policy
- plugin installation, enable, disable, add, and removal
- authentication method selection and manager configuration
- security policy and hardening configuration
- memory allocation and runtime budget settings
- maintenance scheduling and derivative-lane policy
- other engine-owned administrative settings promoted into canon

Every remote management change must create both:
- a cluster-resident instruction and deployment record
- a target-local durable settings record owned by the target database

File reload alone is not sufficient as the only durable record for cluster
managed settings.

## Required instruction lifecycle

Each remote management instruction must move through the following states:
- `QUEUED`
- `ASSESSED`
- `READY`
- `DISPATCHED`
- `APPLYING`
- `APPLIED`
- `FAILED`
- `QUARANTINED`
- `ROLLED_BACK` when explicit rollback is supported for that instruction class

Assessment must validate, at minimum:
- target identity
- capability support
- privilege scope
- dependency and ordering requirements
- maintenance window constraints
- local refusal conditions

## Authority and safety rules

The authoritative execution route is:
- cluster layer issues the approved instruction
- manager receives the instruction for the local server
- engine-owned admin surface validates and binds the request
- controller issues bounded listener-management work when required
- target database persists the local setting or deployment state
- manager reports outcome back to the cluster layer

Required consequences:
- the manager is the server-local agent, not the durable policy store
- the listener remains a bounded runtime target only
- the engine remains the local authority for durable apply, validation, and
  refusal
- no remote management transport may bypass engine authorization
- no listener or manager transport becomes the source of configuration truth

## Current-proof versus required-implementation split

Current code proves the local front-door manager and the local
manager-to-listener validation path.

The broader heartbeat, membership, queued deployment, and remote assessment
model is canonically required reconstructed behavior and must be implemented to
this file, even where current code still lags.
