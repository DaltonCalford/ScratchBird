Last updated: 2026-02-24

# PG-EMU-012 Extended Protocol (Parse/Bind/Execute/Sync)

## Scope of this cycle
- Add direct protocol-level coverage for PostgreSQL extended-query message flow.
- Validate emitter sequencing for Parse/Bind/Execute/Sync in adapter path.
- Capture current executor-path behavior when backend IPC endpoint is unavailable.

## Code touchpoints
- `tests/unit/test_protocol_adapter_dialects.cpp`

## Implemented in this cycle
- Added PostgreSQL frontend packet builders and backend frame decoders in unit harness:
  - frontend message encoder for `PARSE`, `BIND`, `EXECUTE`, `SYNC`;
  - backend message-type extraction from framed wire stream.
- Added `ProtocolAdapterDialectsC1.PostgreSQLExtendedParseBindSyncFlow`:
  - verifies ordered output `ParseComplete -> BindComplete -> ReadyForQuery`.
- Added `ProtocolAdapterDialectsC1.PostgreSQLExtendedExecuteMissingPortalReportsErrorOnSync`:
  - verifies missing portal execute emits `ErrorResponse`;
  - verifies `ReadyForQuery` follows on `SYNC`.
- Added `ProtocolAdapterDialectsC1.PostgreSQLExtendedExecutePathEmitsErrorAndReady`:
  - verifies full Parse/Bind/Execute/Sync path emits extended-protocol frames and terminates with `ReadyForQuery`;
  - current environment intentionally records executor failure frame when no IPC engine endpoint is present.

## Verification evidence
- Build:
  - `cmake --build build -j8 --target scratchbird_tests`
- Focused extended-protocol tests:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC1.PostgreSQLExtended*'`
  - Result: `3/3` passed.
- Regression checks:
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC1.*'`
  - `build/tests/scratchbird_tests --gtest_filter='ProtocolAdapterDialectsC3.*'`
  - Result: all passed.

## Remaining closure items
- Native-vs-emulated live byte-capture replay for extended query traffic is still blocked by external native PostgreSQL credential/runtime setup in this lane.
- Once live native extended captures are available, promote this row from adapter-semantics evidence to full wire-parity evidence.

