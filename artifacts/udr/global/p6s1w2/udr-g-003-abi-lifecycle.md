# UDR-G-003 ABI Lifecycle + Policy Preflight
Last-Modified: 2026-02-23

## Scope
Start `UDR-G-003`:
1. Implement runtime UDR connector ABI lifecycle entry points.
2. Implement policy preflight and bounded execution controls (`timeout`, `max_rows`, `max_bytes`).
3. Move remote control opcodes from deterministic `REMOTE_22xx` rejects to runtime-dispatch closures.

## Baseline
1. Catalog lifecycle/state-machine primitives are present and tested:
   - `RemoteConnectorState` transition controls.
   - Passthrough policy rows (`allow_*`, `max_rows`, `max_bytes`, `timeout_ms`, `required_capabilities`).
   - Prepared statement / txn binding / execution audit catalog contracts.
2. Connector factory/runtime surface exists for PostgreSQL/MySQL/Firebird/ScratchBird.
3. Runtime bridge entry points are still hard rejects:
   - `sys_remote_exec`, `sys_remote_query`, `sys_remote_call` currently return `NOT_IMPLEMENTED`.
4. `G2` is complete: parser/emitter/executor dispatch now routes correctly to remote opcode family without `BRG_0406`/`IRX_0403`.

## Initial G3 Implementation Plan
1. Add a runtime context resolver that maps remote control payload identity to:
   - remote connector record,
   - attached passthrough policy,
   - connector capabilities.
2. Implement policy preflight helper:
   - operation-class allow-list checks,
   - hard bounds for `max_rows`, `max_bytes`, `timeout_ms`,
   - required capability checks.
3. Implement `sys_remote_*` lifecycle scaffolding:
   - connector acquisition/health check,
   - execution path stubs for `QUERY` vs `COMMAND`,
   - deterministic audit row persistence on allow/reject/failure.
4. Wire executor remote opcode handler to call runtime preflight + `sys_remote_*` entry points.

## Status
1. `UDR-G-003`: COMPLETED.
2. Gate status: `UDR-GATE-03` closed.
3. Next closure slice: `UDR-G-004` metadata snapshot + projection runtime.

## Implemented in this cycle
1. Added bound runtime entry points and lifecycle dispatch in `src/udr/udr_connector.cpp`:
   - `sys_remote_exec_bound`
   - `sys_remote_query_bound`
   - `sys_remote_call_bound`
   - legacy `sys_remote_exec/query/call` now resolve connection config and delegate to bound dispatch.
2. Implemented executor policy preflight + runtime dispatch in `src/sblr/executor.cpp`:
   - foreign server resolution, user mapping fallback, connector row selection by state,
   - passthrough policy and capability checks,
   - request bounds (`max_rows`, `max_bytes`, `timeout_ms`) enforcement,
   - remote opcode execution routing via bound runtime entry points,
   - deterministic `REMOTE_23xx` runtime/preflight rejection model replacing deterministic `REMOTE_22xx` bridge rejects.
3. Added stable-link behavior:
   - weak fallback stubs for bound runtime functions inside `scratchbird_sblr` to keep targets linkable without `scratchbird_udr`,
   - explicit `scratchbird_udr` linkage for `sb_server`, `scratchbird`, `scratchbird_tests`, and `scratchbird_tests_sequential`.
4. Updated tests:
   - `tests/unit/test_udr_connector_factory.cpp` now validates argument-level preflight for bound paths.
   - `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp` now asserts remote family is routed through `REMOTE_23xx` and no longer `REMOTE_22xx`.

## Validation Evidence
1. Build:
   - `cmake --build /home/dcalford/CliWork/ScratchBird/build --target sb_server scratchbird scratchbird_tests -j$(nproc)` (pass)
2. Unit tests:
   - `/home/dcalford/CliWork/ScratchBird/build/tests/test_udr_connector_factory --gtest_color=no` (pass)
   - `/home/dcalford/CliWork/ScratchBird/build/tests/scratchbird_tests --gtest_color=no --gtest_filter="*RemoteOpcodeFamilyRoutesWithoutDeterministicBridgeReject*"` (pass)
