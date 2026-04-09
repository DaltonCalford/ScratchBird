# Section 26 Specification Outline

## Objective

Define the implementation-ready current transport contracts for ScratchBird
native wire framing and parser-agent to engine IPC framing, without inventing
transport surfaces that current runtime code does not expose.

## Scope

Section `26` defines the current transport contracts for:
- ScratchBird native wire framing (`SBDB`)
- parser-agent to engine IPC framing (`SBIP`)
- current message catalogs, payload ownership boundaries, result frames, error
  frames, and copy or stream message families

## Active authority files

- `IPC_SBWP_FRAME_SPEC.md`
  - current native and IPC frame header authority
- `MESSAGE_CATALOG_AND_SCHEMAS.md`
  - current message families and payload-schema ownership
- `PROTOCOL_STATE_MACHINES.md`
  - current native and IPC request or response ordering authority
- `RESULT_SHAPES_AND_ERROR_FRAMES.md`
  - current result, completion, and error transport mapping
- `SERVICE_CHANNELS_AND_STREAMING.md`
  - current copy or stream control authority and explicit unsupported edges
- `DECISION_RECORD.md`
- `TEST_CONTRACT.md`
- `DEPENDENCIES.md`

## Unsupported-boundary files

- `FORENSIC_REPLAY_SESSION_PROFILE.md`
- `CLUSTER_UDR_FABRIC_CHANNEL_SPEC.md`
- `NORMATIVE_P1_WIRE_DISTRIBUTED_READ_AND_TELEMETRY_CHECKLIST.md`

These are explicit unsupported-boundary documents in the current tree. They are
not active implementation authority.

## Cross-section boundaries

- section `27`
  - handshake and auth sequencing
- section `28`
  - parser ownership of SQL and adapter behavior
- section `29`
  - listener process model and local control-plane orchestration
- section `22`
  - SBLR container and verifier payload ownership
- section `21`
  - result-shape source semantics

## Non-goals

Section `26` does not define:
- full replay-session transport
- cluster fabric runtime
- distributed read telemetry expansion
- listener management control-plane framing

Those surfaces are outside current section `26` implementation authority.
