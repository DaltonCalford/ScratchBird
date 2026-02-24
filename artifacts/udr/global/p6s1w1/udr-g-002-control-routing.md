# UDR-G-002 Control Envelope + Executor Routing
Last-Modified: 2026-02-23

## Scope
Close `UDR-G-002`:
1. Define `CTL_REMOTE_*` envelope build/validation path.
2. Route remote opcodes to executor/runtime entry points.
3. Replace mandatory-surface reject bridges with deterministic control dispatch.

## Implemented
1. Added v3 opcode registry coverage for FDW alter/import + remote execution/metadata/txn/session opcodes (`0x6160..0x6173`).
2. Added opcode identity and canonical feature-key closure for the full remote family (`FG_FDW`, `FG_REMOTE`).
3. Added payload-schema mapping closure for the full bridge family:
   - FDW alter/import opcodes route to FDW DDL schemas.
   - Remote control opcodes route with deterministic schema selection (`SCHEMA_CONTROL_COMMAND` and `SCHEMA_SET_SHOW_RESET` where required by current opcode-name routing contract).
4. Added executor routing closure:
   - FDW alter/import opcodes no longer hit `BRG_0406`/`IRX_0403`.
   - Remote opcode family no longer hits `BRG_0406`/`IRX_0403` and returns deterministic `REMOTE_22xx` rejects pending runtime ABI implementation in `G3`.

## Evidence
1. Build:
   - `cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j$(nproc)`
2. Focused gate tests:
   - `/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_filter='SBLRVNextPayloadSchemaMappingContractTest.BridgeOpcodeRangeMappingsAreDeterministic:SBLRV3OpcodeIdentity.MapsExpandedStatementFamilies:SBLRVNextExecutorDispatchContractTest.FdwDropOpcodesRouteWithoutDeterministicBridgeReject:SBLRVNextExecutorDispatchContractTest.FdwAlterAndImportOpcodesRouteWithoutDeterministicBridgeReject:SBLRVNextExecutorDispatchContractTest.RemoteOpcodeFamilyRoutesWithoutDeterministicBridgeReject'`
   - Result: `5/5 PASSED`.

## Current Status
1. `UDR-G-002`: COMPLETED.
2. Gate status: `UDR-GATE-02` closed.
3. `G3` is unblocked for runtime ABI/policy-preflight implementation (`sys_remote_*` integration and bounded execution controls).
