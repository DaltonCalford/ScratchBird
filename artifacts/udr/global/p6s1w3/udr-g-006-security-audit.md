# UDR-G-006 Deterministic Security Audit + Degraded-Mode Behavior
Last-Modified: 2026-02-24

## Scope
1. Close deterministic audit behavior for remote execution outcomes (success/failure/cancel).
2. Ensure cancel handling maps to deterministic status and does not incorrectly poison connector readiness.
3. Ensure degraded/failed connector states and runtime errors are auditable with stable request correlation.

## Implemented in this cycle
1. `src/sblr/executor.cpp` audit + cancel closure:
   - Preserved audit emission on cancellation path (`CANCELLED/QUERY_CANCELED`) with deterministic rejection code `REMOTE_2311`.
   - Bound audit request correlation to propagated `remote_request_id` instead of ad hoc IDs.
   - Hardened audit tx binding so `txid` is only attached when the runtime transaction catalog row exists; prevents audit upsert rejection on stale/non-catalog txids.
2. `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp` security/audit coverage:
   - Updated cancel test to use schema-compatible control-envelope option keys (`CANCEL`, `REQUEST_ID`, `SQL_TEXT`).
   - Verified cancelled execution preserves connector readiness and emits auditable cancelled status with expected request correlation.
   - Verified repeated runtime failures produce degraded/failed connector state transitions and auditable failure records.

## Validation Evidence
1. Build:
   - `cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j$(nproc)` (pass)
2. Unit tests:
   - `ctest --output-on-failure -R "SBLRVNextExecutorDispatchContractTest\\.(ExecuteRemoteCancelProducesCancelledAuditAndPreservesConnectorReadiness|ExecuteRemoteFailuresDriveConnectorDegradedThenFailedState)$"` (pass)
   - `ctest --output-on-failure -R "SBLRVNextExecutorDispatchContractTest\\..*"` (pass; 24/24)

## Status
1. `UDR-G-006`: COMPLETED.
2. Gate status: `UDR-GATE-06` closed.
