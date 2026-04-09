# IPC and Internal Message Surfaces

This file classifies architecture-visible message and transport seams so they cannot be misread as one class of contract.

## Contract classification matrix

| Surface class | Current meaning | Ownership rule | Stability assumption |
| --- | --- | --- | --- |
| public transport contract | a message/protocol surface explicitly owned by a protocol or handshake section | owned outside section 32 by the contract section | only as stable as the owning section says |
| bounded control-plane contract | a listener/server/process control surface visible across a bounded runtime seam | owned primarily by section 29 with architecture classification here | bounded and purpose-specific, not universal API surface |
| internal implementation message | an internal handoff, spawn, bridge, or orchestration path | not a public contract by default | unstable unless another section explicitly promotes it |

## Message-class matrix

| Message class | Current state | Allowed payload role | Explicit exclusion |
| --- | --- | --- | --- |
| session control | current_bounded | create, bind, rebind, teardown, or capability-check session state | not a user-visible data contract by default |
| execution dispatch | current_bounded | carry a bounded request envelope from entry surface to execution surface | not a stable extension ABI |
| result or status return | current_bounded | carry success, refusal, or bounded diagnostic status back to the caller surface | not proof of universal streaming behavior |
| control-plane management | current_bounded | coordinate bounded listener or server process behavior | not a general remote management plane |
| diagnostic or audit handoff | partial | emit bounded diagnostics or internal evidence paths | not a stable external observability protocol |

## IPC or internal handoff state machine

1. `unclassified`: a seam exists but has not yet been classified as public, bounded control-plane, or internal.
2. `classified`: the seam is assigned one of the contract classes above.
3. `bound_to_owner`: the seam is tied to its owning detailed section.
4. `message_emitted`: a bounded internal or external payload is sent under the owner's rules.
5. `message_consumed`: the receiving subsystem accepts, rejects, or tears down the handoff.
6. `retired_or_promoted`: the seam remains internal, is retired, or is explicitly promoted by a new canonical owner section.

## Publication rules

1. Public transport contracts must be explicitly owned by sections 26, 27, 29, or 30 as appropriate.
2. Control-plane message existence does not by itself create a public API guarantee.
3. Internal handoff or bridge messages remain internal unless another canonical section explicitly upgrades them.
4. Section 32 only classifies message surfaces; it does not widen their contractual scope.
5. A change in message naming, shape, or ordering is non-breaking only for internal implementation messages; all public or bounded control-plane changes require owner review.

## Explicit non-guarantees

- internal message naming is not a stable API promise
- presence of an IPC seam does not imply general remote execution or cluster-fabric semantics
- a transport-visible handshake or listener control path does not imply a broad client-extensibility promise by itself
- diagnostic handoff paths are not operator-facing protocol guarantees unless explicitly promoted by another section
