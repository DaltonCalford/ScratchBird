Last updated: 2026-02-24

# Compatibility CTest Integration Status

## Scope
- Register emulated-engine compatibility lanes in full `ctest`.
- Ensure compatibility runners execute dedicated protocol clients only (`sb_fb_isql`, `sb_my_isql`, `sb_pg_isql`).
- Record current execution and blockers for direct downloadable verification.

## CTest Registration (in-tree)
- Added CTest entries in `tests/CMakeLists.txt`:
  - `CompatibilityFirebird`
  - `CompatibilityMySQL`
  - `CompatibilityPostgreSQL`
  - `CompatibilityEmulationEvidence`

## Current Execution Result
- `ctest --test-dir build -R '^Compatibility(Firebird|MySQL|PostgreSQL)$' --output-on-failure`
  - `CompatibilityFirebird`: skipped (`SKIP_RETURN_CODE=77`)
  - `CompatibilityMySQL`: skipped (`SKIP_RETURN_CODE=77`)
  - `CompatibilityPostgreSQL`: skipped (`SKIP_RETURN_CODE=77`)
- `ctest --test-dir build -R '^CompatibilityEmulationEvidence$' --output-on-failure`
  - passed (`465.75 sec`)

## Why Three Compatibility Lanes Are Skipped
- Dedicated protocol clients are not currently buildable in local `ScratchBird-driver`.
- Generic `sb_isql` is intentionally rejected for emulated wire-protocol parity.

## Driver Build Blocker Evidence
- Attempted build:
  - `cmake -S /home/dcalford/CliWork/ScratchBird-driver -B /home/dcalford/CliWork/ScratchBird-driver/build -DSB_BUILD_CLI_FDW=ON`
  - `cmake --build /home/dcalford/CliWork/ScratchBird-driver/build -j8 --target sb_fb_isql sb_my_isql sb_pg_isql`
- Link failures:
  - `sb_my_isql`: unresolved `scratchbird::fdw::MySQLAdapter::*`
  - `sb_pg_isql`: unresolved `scratchbird::fdw::PostgreSQLAdapter::*`
  - `sb_fb_isql`: unresolved engine/core/sblr symbols (`Database`, `Executor`, `FirebirdQueryCompiler`, `CatalogManager`, `TypedValue`)

## Status
- CTest integration is implemented and discoverable in full test enumeration.
- Full emulated-engine execution is blocked pending driver-side linker/runtime integration for dedicated protocol clients.
