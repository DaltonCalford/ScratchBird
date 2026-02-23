# XOS-055 Required Cross-OS Gate Checks
Last-Modified: 2026-02-22

## Implemented
- Added required-check aggregator job in:
  - `.github/workflows/cross-os-gate.yml`
  - job id: `cross_os_required_checks`

## Required Inputs
- `linux_gcc`
- `linux_clang`
- `windows_msvc`
- `linux_mingw_cross`

## Enforcement Behavior
- Aggregator executes with `if: always()` and fails if any required upstream job result is not `success`.
- Branch protection can pin `Cross OS Required Checks` as required for merge.
