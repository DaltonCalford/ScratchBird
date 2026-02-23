# XOS-056 Nightly Extended and Performance Lanes
Last-Modified: 2026-02-22

## Implemented
- Added scheduled workflow:
  - `.github/workflows/cross-os-nightly.yml`

## Nightly Jobs
- `linux_extended_suite`
  - full suite minus quarantine (`ctest -E quarantine`)
- `linux_performance_sample`
  - performance sample lane using:
  - `scripts/cross_os/run_portable_lane.sh --lane performance_sample`

## Trigger Modes
- scheduled nightly cron
- manual `workflow_dispatch`
