# Tracker: Control-Plane Socket (Listener <-> Parser)

Status: In progress
Last Updated: 2026-01-18

## Scope
Implement the listener control-plane socket and handoff protocol used to
coordinate parser workers. This includes spawn commands, health checks,
metrics, and socket handoff metadata.

## Specifications
- docs/specifications/network/CONTROL_PLANE_PROTOCOL_SPEC.md
- docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/operations/LISTENER_POOL_METRICS.md

## Deliverables
- Control-plane server in listener binaries
- Control-plane client in parser agents
- Message framing + request/response support
- Health/metrics hooks for pool supervision

## Work Items
1) Control-plane message structs + serialization (versioned framing). (done)
2) Listener-side control-plane server socket (per protocol). (in progress)
3) Parser-side control-plane client (register, heartbeat, recycle).
4) Handoff metadata pipe and acknowledgement tracking.
5) Metrics emission and status dump.

## Notes
- Keep OS-specific socket passing logic isolated for later extension.
