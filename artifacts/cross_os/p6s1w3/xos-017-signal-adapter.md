# XOS-017 Signal And Control Event Abstraction

Last-Modified: 2026-02-22

## Scope
Implemented a platform signal/control adapter so runtime targets no longer need direct in-file signal handler setup logic for lifecycle control events.

## Code Changes
- Added `include/scratchbird/core/signal_control.h`
  - `ControlSignal` lifecycle event model (`SHUTDOWN`, `RELOAD`, `ROTATE_LOGS`, `DUMP_STATS`, `IMMEDIATE_STOP`)
  - `SignalInstallSpec` install-time policy
  - `SignalControl` interface (`install`, `uninstall`, `poll`, `inject`)
- Added `src/core/signal_control.cpp`
  - Platform implementation with install/uninstall/poll/inject
  - POSIX path uses `sigaction`
  - Windows path uses `std::signal`
  - Global active-instance guard to prevent concurrent conflicting signal installs
  - Poll-based dispatch model with atomic pending signal state
- Added `tests/unit/test_signal_control.cpp`
  - inject/poll behavior
  - invalid argument guards

## Gate Evidence
- Test output: `artifacts/cross_os/p6s1w3/xos-017-018-runtime-ctest.txt`
- Signal surface inventory: `artifacts/cross_os/p6s1w3/xos-017-018-signal-surface.txt`

## Result
`XOS-017` implementation completed with adapter contract + unit coverage.
