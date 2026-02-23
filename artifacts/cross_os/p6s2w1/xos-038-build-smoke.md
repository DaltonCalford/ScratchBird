# XOS-038 Build Profile Smoke Scripts

## Scope
Added profile-driven smoke runner for local and CI workflows.

## Added file
- `scripts/cross_os/smoke_profiles.sh`

## Script capabilities
- `--mode local|ci`
- `--profiles <csv of configure presets>`
- `--configure-only`
- `--skip-tests`
- `--dry-run`

## Validation runs
- Local configure smoke:
  - `scripts/cross_os/smoke_profiles.sh --mode local --profiles linux-gcc-debug --configure-only`
  - Evidence: `artifacts/cross_os/p6s2w1/xos-038-smoke-local.txt`
- CI dry-run smoke command expansion:
  - `scripts/cross_os/smoke_profiles.sh --mode ci --profiles linux-gcc-debug,linux-clang-debug --configure-only --dry-run`
  - Evidence: `artifacts/cross_os/p6s2w1/xos-038-smoke-ci-dryrun.txt`

## Usage notes
- In CI mode, test smoke uses `ctest --preset <preset>-test` with `PortableRuntimeGuard|SignalControlTest` filter.
- In local mode, default test smoke is narrower (`PortableRuntimeGuard`) for quick checks.
