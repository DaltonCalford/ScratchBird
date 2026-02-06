# SBWP + IPC End-to-End Audit

Status: Draft
Date: 2026-02-03
Scope: ScratchBird server IPC layer, parser agents, and protocol adapters.
Goal: Ensure SBWP v1.1 features are supported end-to-end for native clients
and tunneled through IPC from emulated parsers.

## Summary
SBWP v1.1 defines feature negotiation, attachment/txn IDs, prepared statements,
COPY/streaming, notifications, compression, and cancel. The current IPC
contract (SBIP v1.0) and parser agent implementation only cover AUTH_REQUEST
and EXECUTE_SBLR, which is insufficient for full SBWP support. Emulated parsers
also cannot forward advanced features over IPC.

## Evidence
- SBWP spec: docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md
- IPC contract: docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- Parser agent spec: docs/specifications/network/PARSER_AGENT_SPEC.md
- Protocol adapters: src/protocol/adapters/*

## Gaps

### IPC Contract
- Missing IPC messages for SBWP STARTUP/feature negotiation.
- Missing prepared statement lifecycle messages.
- Missing COPY/streaming data frame messages.
- Missing notification subscribe/deliver messages.
- Missing cancel and interrupt semantics.
- No attachment_id/txn_id mapping contract.
- No compression/checksum flag support.

### Engine IPC Server
- No handlers for prepared statements, streaming, notifications, cancel.
- No attachment/txn lifecycle in IPC.
- No backpressure controls for streaming.

### Parser Agents
- Native parser does not forward SBWP-specific lifecycle to engine.
- Emulated parsers cannot map their protocol features to IPC.
- Error mapping relies on EXECUTE_SBLR only.

## Impact
- Native SBWP clients cannot use full v1.1 feature set.
- Emulated protocol features that require streaming or prepared execution are
  effectively blocked end-to-end.

