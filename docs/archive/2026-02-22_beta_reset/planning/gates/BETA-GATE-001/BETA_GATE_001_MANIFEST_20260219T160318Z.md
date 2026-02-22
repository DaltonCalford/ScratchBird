# BETA-GATE-001 Clean Build + Full Suite Manifest
Last modified: 2026-02-19

## Run Metadata

- Run timestamp (UTC): `20260219T160318Z`
- Repository root: `.`
- Build directory: `build/`
- Jobs: `14`
- Wall clock seconds: `1436`

## Commands Executed

1. `rm -rf build`
2. `cmake -S . -B build`
3. `cmake --build build -j 14`
4. `ctest --test-dir build --output-on-failure`

## ctest Result

- Exit code: `0`
- Summary: `100% tests passed, 0 tests failed out of 3390`
- Passed: `3390`
- Failed: `0`
- Total: `3390`
- Real test time (sec): `697.01`

## Evidence Files

- Run log: `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_RUN_20260219T160318Z.log`
- ctest log: `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_CTEST_20260219T160318Z.log`
- Summary env: `docs/planning/gates/BETA-GATE-001/BETA_GATE_001_SUMMARY_20260219T160318Z.env`
