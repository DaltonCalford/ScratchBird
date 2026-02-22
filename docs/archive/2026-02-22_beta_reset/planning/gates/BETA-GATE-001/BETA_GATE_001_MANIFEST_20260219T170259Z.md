# BETA-GATE-001 Clean Build + Full Suite Manifest
Last modified: 2026-02-19

## Run Metadata

- Run timestamp (UTC): `20260219T170259Z`
- Repository root: `.`
- Build directory: `build/`
- Jobs: `14`
- Wall clock seconds: `1530`

## Commands Executed

1. `rm -rf build`
2. `cmake -S . -B build`
3. `cmake --build build -j 14`
4. `ctest --test-dir build --output-on-failure`

## ctest Result

- Exit code: `8`
- Summary: `81% tests passed, 644 tests failed out of 3390`
- Passed: `2746`
- Failed: `644`
- Total: `3390`
- Real test time (sec): `612.18`

## Evidence Files

- Run log: `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_RUN_20260219T170259Z.log`
- ctest log: `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_CTEST_20260219T170259Z.log`
- Summary env: `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_SUMMARY_20260219T170259Z.env`
