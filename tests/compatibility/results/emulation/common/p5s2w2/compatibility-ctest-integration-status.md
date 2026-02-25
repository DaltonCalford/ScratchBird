Last updated: 2026-02-25

# Compatibility CTest Integration Status

## Scope
- Register emulated-engine compatibility lanes in full `ctest`.
- Ensure compatibility runners execute protocol-accurate clients (`sb_my_isql`, `sb_pg_isql`, and Firebird-compatible `isql-fb`/`sb_fb_isql`).
- Record current execution and blockers for direct downloadable verification.

## CTest Registration (in-tree)
- Added CTest entries in `tests/CMakeLists.txt`:
  - `CompatibilityFirebird`
  - `CompatibilityMySQL`
  - `CompatibilityPostgreSQL`
  - `CompatibilityEmulationEvidence`

## Current Execution Result
- `ctest --test-dir build -R '^Compatibility(Firebird|MySQL|PostgreSQL)$' --output-on-failure`
  - `CompatibilityFirebird`: passed (`5.18 sec`)
  - `CompatibilityMySQL`: skipped (`SKIP_RETURN_CODE=77`, endpoint/auth precheck)
  - `CompatibilityPostgreSQL`: skipped (`SKIP_RETURN_CODE=77`, endpoint/auth precheck)
- `ctest --test-dir build -R '^CompatibilityEmulationEvidence$' --output-on-failure`
  - passed (`472.95 sec`)

## Why Two Compatibility Lanes Are Skipped
- Dedicated wrappers now build for MySQL/PostgreSQL in local `ScratchBird-driver`.
- Lane runners now perform endpoint/auth prechecks and skip when no compatible server/auth profile is reachable for the selected host/port.
- Generic `sb_isql` remains intentionally rejected for emulated wire-protocol parity.

## Driver Build Blocker Evidence
- Attempted build:
  - `cmake -S /home/dcalford/CliWork/ScratchBird-driver -B /home/dcalford/CliWork/ScratchBird-driver/build -DSB_BUILD_CLI_FDW=ON`
  - `cmake --build /home/dcalford/CliWork/ScratchBird-driver/build -j8 --target sb_fb_isql sb_my_isql sb_pg_isql`
- Current state:
  - `sb_my_isql`: buildable
  - `sb_pg_isql`: buildable
  - `sb_fb_isql`: buildable after engine/static-link closure in driver CMake
  - Forcing `sb_fb_isql` in curated Firebird compatibility currently exposes parser gap:
    - `functional/dml/join_02.sql` -> `Error: V3 SELECT join missing ON/USING condition`

## Status
- CTest integration is implemented and discoverable in full test enumeration.
- Firebird lane is executable via Firebird-protocol clients; runner currently prefers native `isql-fb` for stable upstream parity unless `SCRATCHBIRD_FB_ISQL` is explicitly set.
- Full MySQL/PostgreSQL lane closure requires endpoint/auth profiles that match the wrappers' supported auth negotiation paths.
