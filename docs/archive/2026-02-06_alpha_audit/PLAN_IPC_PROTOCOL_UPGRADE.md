# Plan: IPC + SBWP v1.1 End-to-End Support

Status: Draft
Last Updated: 2026-02-03
Owner: Core/Network

## Goal
Upgrade the Parser↔Engine IPC contract so that ScratchBird native (SBWP v1.1)
features are supported end-to-end, and emulated parsers can forward advanced
engine features through IPC. The updated IPC contract must support:
- Session attachment/txn IDs
- Prepared statements
- Streaming/COPY
- Notifications
- Cancel
- Feature negotiation and compression flags

## Inputs
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
- `docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md`
- `docs/specifications/network/PARSER_AGENT_SPEC.md`
- `docs/specifications/wire_protocols/*_EMULATION_BEHAVIOR.md`

## Workstreams

### WS-1: IPC Contract v1.1
- [ ] Extend IPC header to include protocol version + feature flags.
- [ ] Add message types for:
  - STARTUP/FEATURE_NEGOTIATE
  - PREPARE/PARSE
  - BIND/DESCRIBE/EXECUTE/CLOSE
  - COPY_DATA/COPY_DONE/COPY_FAIL
  - STREAM_CONTROL (backpressure)
  - NOTIFY_SUBSCRIBE/UNSUBSCRIBE/NOTIFY_DELIVER
  - CANCEL
  - TXN_BEGIN/TXN_COMMIT/TXN_ROLLBACK/SAVEPOINT
- [ ] Define attachment_id/txn_id mapping and lifetime rules.
- [ ] Define compression/checksum flags and payload framing.
- [ ] Update ENGINE_PARSER_IPC_CONTRACT.md accordingly.

### WS-2: Engine IPC Server
- [ ] Implement new IPC message handlers in server IPC layer.
- [ ] Map IPC prepared statements to engine statement cache.
- [ ] Implement streaming/COPY data channels with backpressure.
- [ ] Implement notification delivery to parser sessions.
- [ ] Implement cancel semantics and interrupt propagation.

### WS-3: Parser Agents (Native + Emulated)
- [ ] Native parser: forward SBWP startup/feature negotiation to engine.
- [ ] PostgreSQL/MySQL/Firebird parsers: map protocol features to IPC messages
      (prepared statements, streaming, COPY).
- [ ] Add IPC-level error mapping to protocol-specific formats.
- [ ] Ensure attachment/txn IDs are tracked and passed on each IPC call.

### WS-4: Validation and Compatibility
- [ ] Add unit tests for IPC framing and message types.
- [ ] Add integration tests for SBWP v1.1 feature paths (prepare, COPY,
      notifications, cancel).
- [ ] Add cross-protocol tests to ensure emulated parsers can use the same IPC
      features where applicable.

## Exit Criteria
- SBWP v1.1 features work end-to-end with native clients.
- Emulated parsers forward advanced features via IPC.
- IPC contract is versioned and documented, with tests.

