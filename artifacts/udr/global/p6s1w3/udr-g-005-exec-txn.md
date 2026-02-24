# UDR-G-005 Execution Classes + Remote Transaction Semantics
Last-Modified: 2026-02-24

## Scope
1. Close runtime execution semantics for `SBLR3_EXECUTE_REMOTE` and prepared/transaction lifecycle opcodes.
2. Ensure runtime operation class, status handling, and connector health transitions are deterministic under failure/retry.
3. Close test coverage for remote execution behavior beyond metadata-only paths.

## Implemented in this cycle
1. `src/sblr/executor.cpp` runtime execution closure:
   - Added request-id parsing and propagation from payload/options (`REQUEST_ID`/`REQUEST_UUID`) into remote execution flow.
   - Preserved operation-class classification for execute/prepared paths and ensured SQL-text validation is deterministic (`REMOTE_2310`).
   - Added connector health transition updates for runtime-dispatch outcomes:
     - success: clear failure count and move `PROBING/DEGRADED -> READY`.
     - failure: increment failure count and move `READY/PROBING -> DEGRADED`, then `DEGRADED -> FAILED` at threshold.
2. `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp` execution semantics coverage:
   - Added/updated execution-state tests for repeated runtime failure progression to `FAILED`.
   - Ensured control-envelope option payloads use schema-compatible `OPTION_KV` fields for execute paths.

## Validation Evidence
1. Build:
   - `cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j$(nproc)` (pass)
2. Unit tests:
   - `ctest --output-on-failure -R "SBLRVNextExecutorDispatchContractTest\\.(ExecuteRemoteFailuresDriveConnectorDegradedThenFailedState)$"` (pass)
   - `ctest --output-on-failure -R "SBLRVNextExecutorDispatchContractTest\\..*"` (pass; 24/24)

## Status
1. `UDR-G-005`: COMPLETED.
2. Gate status: `UDR-GATE-05` closed.
