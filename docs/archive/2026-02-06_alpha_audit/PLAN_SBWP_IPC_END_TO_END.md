# Plan: SBWP v1.1 End-to-End IPC Support

Status: Draft
Last Updated: 2026-02-03
Owner: Core/Network

## Goal
Upgrade the IPC layer and parser agents so that SBWP v1.1 features are fully
supported end-to-end between native ScratchBird clients and the engine, and
emulated parsers can forward advanced protocol features over IPC.

## Workstreams

### WS-1: IPC Contract Upgrade (SBIP v1.1)
- [ ] Extend IPC header to include protocol version, feature flags, and
      attachment/txn IDs.
- [ ] Add message types for:
  - STARTUP/FEATURE_NEGOTIATE
  - PREPARE/PARSE
  - BIND/DESCRIBE/EXECUTE/CLOSE
  - COPY_DATA/COPY_DONE/COPY_FAIL
  - STREAM_CONTROL
  - NOTIFY_SUBSCRIBE/UNSUBSCRIBE/NOTIFY_DELIVER
  - CANCEL
  - TXN_BEGIN/TXN_COMMIT/TXN_ROLLBACK/SAVEPOINT
- [ ] Update ENGINE_PARSER_IPC_CONTRACT.md and add versioning guidance.

### WS-2: Engine IPC Server
- [ ] Implement message handlers for new IPC message types.
- [ ] Add prepared statement cache mapping for IPC sessions.
- [ ] Implement streaming/COPY data channels with backpressure.
- [ ] Implement notification delivery pipeline.
- [ ] Implement cancel/interrupt propagation.

### WS-3: Parser Agents (Native + Emulated)
- [ ] Map SBWP STARTUP/FEATURE_NEGOTIATE to IPC.
- [ ] Forward attachment/txn IDs for all requests.
- [ ] Map native prepared/copy/notify/cancel flows to IPC.
- [ ] Map emulated protocol features to IPC equivalents where possible.
- [ ] Ensure protocol-specific error mapping remains correct.

### WS-4: Validation
- [ ] IPC framing + message type unit tests.
- [ ] Integration tests for SBWP v1.1 feature paths.
- [ ] Cross-protocol tests verifying emulated parsers can use IPC streaming
      and prepared execution.

## Exit Criteria
- Native SBWP v1.1 features function end-to-end.
- Emulated parsers forward advanced protocol features to engine.
- IPC contract is versioned and documented with tests.

