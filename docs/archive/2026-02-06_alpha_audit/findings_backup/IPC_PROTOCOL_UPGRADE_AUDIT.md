# IPC + Protocol Upgrade Audit

Status: Draft
Date: 2026-02-03
Scope: Engine IPC layer + parser agents + SBWP v1.1 support across native and
emulated protocols.

## Summary
The current Parser↔Engine IPC contract is v1.0 and only covers AUTH_REQUEST and
EXECUTE_SBLR flows. SBWP v1.1 specifies additional protocol features (TLS-only
startup, session attachment/txn IDs, prepared statements, streaming, COPY,
notifications, federation, compression). These features are not represented in
IPC messages or in parser-agent responsibilities, so native clients cannot use
full SBWP features end-to-end, and emulated parsers cannot forward advanced
engine features over IPC.

## Evidence (Specs)
- SBWP v1.1: `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
- IPC contract v1.0: `docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md`
- Parser agent spec: `docs/specifications/network/PARSER_AGENT_SPEC.md`

## Gaps

### IPC Contract Coverage
- Missing messages for:
  - STARTUP/feature negotiation
  - Prepared statement lifecycle (PARSE/BIND/DESCRIBE/EXECUTE/CLOSE)
  - Streaming/COPY data frames
  - Notification subscription and delivery
  - Cancel request and interrupt semantics
  - Attachment/transaction lifecycle (attach/detach, begin/commit/rollback)
  - Compression and checksum flags
- IPC contract does not define how attachment_id/txn_id from SBWP map to
  engine sessions, nor how the parser maintains them.

### Parser Agent Responsibilities
- Parser agents currently only forward auth + SBLR execution per IPC contract.
- No explicit support for SBWP feature flags or protocol versioning per IPC.
- Emulated parsers (PostgreSQL/MySQL/Firebird) rely on IPC for execution, but
  IPC lacks mechanisms to carry streaming or prepared statement workflows.

### Engine IPC Server
- IPC server handles EXECUTE_SBLR and metadata only; no endpoints for
  prepared statement lifecycle or streaming.
- No explicit backpressure or streaming control in IPC contract.

## Impact
- Native ScratchBird clients cannot use full SBWP v1.1 feature set end-to-end.
- Emulated parsers cannot map native protocol features that rely on streaming
  or prepared execution to engine IPC.
- Prevents implementing protocol features like COPY and notifications in a
  uniform way across protocols.

