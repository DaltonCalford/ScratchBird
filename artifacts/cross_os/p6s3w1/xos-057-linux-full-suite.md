# XOS-057 Linux Clean Build + Full Suite
Last-Modified: 2026-02-22

## Run Contract
- Clean build directory: `build/linux-gcc-debug`
- Configure preset: `linux-gcc-debug`
- Build preset: `linux-gcc-debug-build`
- Test command: `ctest --preset linux-gcc-debug-test -E quarantine --output-on-failure -j14`

## Result
- `clean_rc=0`
- `configure_rc=0`
- `build_rc=0`
- `test_rc=0`
- Total tests: `3526`
- Passed: `3526`
- Failed: `0`
- Not run: `1` (skipped benchmark)

## Patch Verification
- Fixed path traversal guard in:
  - `src/core/database.cpp`
- Focused verification:
  - `ctest --preset linux-gcc-debug-test -R "SecurityTest.PathTraversal_DotDot"`
  - `ctest --preset linux-gcc-debug-test -R "SecurityTest\\."`

## Gate Status
- `XOS-GATE-05` Linux full-suite requirement is **MET**.

## Evidence
- Full clean rerun command/build/test log:
  - `artifacts/cross_os/p6s3w1/xos-057-linux-full-suite-rerun.log`
- Prior failing run (retained for audit trail):
  - `artifacts/cross_os/p6s3w1/xos-057-linux-full-suite.log`
