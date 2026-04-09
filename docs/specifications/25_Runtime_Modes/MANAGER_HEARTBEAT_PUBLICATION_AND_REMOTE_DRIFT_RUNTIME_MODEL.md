# Manager Heartbeat Publication and Remote Drift Runtime Model

## Scope

This file defines the runtime model for:

- manager-owned heartbeat publication
- server-agent heartbeat freshness
- remote drift assessment
- queued remote instruction posture
- aggregated cluster-fabric status publication

## Governing ownership rule

The manager is the canonical server-local cluster agent.

Canonical rule:

1. the manager owns server-local heartbeat publication toward the wider
   cluster-control lane
2. the listener does not become the cluster heartbeat bus
3. the database remains authoritative for listener topology and policy
4. aggregated drift and remote-instruction posture may be published by the
   manager, but it must remain grounded in catalog and runtime truth

## Current code-backed authority

Current code-backed recovery proves:

1. bounded manager MCP control exists
2. manager token auth exists
3. DBBT and LPREFACE binding exists
4. cluster-fabric persisted link, session, task, event, and error rows already
   exist
5. failure-detector and clock-policy persisted substrate already exists
6. manager inspection rows publish heartbeat identity, local readiness, and
   bounded queue or drift posture through deterministic status entries
7. listener management `STATUS` publishes parser-pool and listener-control
   readiness that the manager consumes without fabricating state

## Reconstructed-required runtime behavior

The manager runtime lane publishes, at minimum:

- heartbeat freshness for the local server agent
- link readiness class
- queued remote-instruction counts by state
- unresolved drift counts by target
- last successful assessment time
- last successful apply time, when applicable

When no queued remote instruction is active, the queue and drift counts remain
deterministically `0` or `CONSISTENT` rather than disappearing from the result
surface.

## Heartbeat publication model

### Published identity

Heartbeat publication must remain bound to:

- cluster identity
- node identity
- manager or server-agent identity
- control-plane epoch or generation

### Published posture

Heartbeat publication must remain able to express:

- ready
- degraded
- blocked
- quarantined

### Freshness rule

Heartbeat freshness must remain derivable from:

- persisted or runtime heartbeat timing
- failure-detector policy
- startup-grace policy

## Remote drift model

Remote drift is the difference between:

- desired management instruction posture
- assessed target posture
- applied target posture

Canonical rule:

- drift is a first-class runtime state
- it is not hidden behind a generic success/failure label

## Instruction state coupling

The manager runtime must be able to publish counts or status by instruction
class such as:

- queued
- assessed
- approved
- blocked
- applying
- applied
- quarantined
- cancelled

## Aggregation rule

If the manager publishes one aggregate status view, it must preserve visible
separation between:

- local manager heartbeat health
- local listener readiness
- local engine or database health
- remote drift posture
- queued instruction posture

## Fail-closed rules

The manager runtime shall not:

1. publish heartbeat healthy while startup-grace or failure-detector policy
   would classify the node as stale or suspect
2. collapse drift, queue state, and local health into one generic status bit
3. fabricate listener or engine state not obtained from the bounded control or
   catalog substrate

## MGA boundary

Persisted heartbeat, drift, and instruction records remain MGA-governed
database state. Publication does not bypass ordinary transaction rules.
