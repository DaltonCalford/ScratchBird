# XOS-013 Daemon Startup/Shutdown Process Adapter Migration
Last-Modified: 2026-02-22

## Migration Summary
Daemon lifecycle now uses `core::ProcessControl` for fork-based startup transitions.

Updated files:
1. `include/scratchbird/server/daemon.h`
2. `src/server/daemon.cpp`
3. `include/scratchbird/core/process_control.h`
4. `src/core/process_control.cpp`
5. `tests/unit/test_process_control.cpp`

## Changes Implemented
1. Added `ProcessControl::forkSelf()` to the adapter contract.
2. Implemented Linux `forkSelf()` in `PlatformProcessControl` (`fork`-backed).
3. Added Windows `forkSelf()` stub returning `Status::NOT_SUPPORTED`.
4. Added `process_control_` member to `Daemon`.
5. Migrated `Daemon::doFork()` to call `process_control_->forkSelf()` instead of direct `fork()`.
6. Added unit coverage for `forkSelf()` child creation and reap behavior.

## Scope Notes
1. This row migrates daemon process forking lifecycle.
2. Signal translation and control-event abstraction remains in `XOS-017` (`ISignalControl` scope).

## Validation
Build:
1. `cmake --build build -j4`

Focused tests:
1. `ProcessControlTest.*` (3/3 passed)
2. `ServiceControllerListenerBootstrapTest.*` (8/8 passed)

Evidence:
1. `artifacts/cross_os/p6s1w2/xos-013-014-process-ctest.txt`
2. `artifacts/cross_os/p6s1w2/xos-013-014-command-log.txt`

## Gate Binding
- Gate: `XOS-GATE-02`
- Tracker row: `XOS-013`
