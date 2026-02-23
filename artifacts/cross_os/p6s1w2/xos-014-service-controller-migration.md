# XOS-014 Service Controller Process Lifecycle Migration
Last-Modified: 2026-02-22

## Migration Summary
`ServiceController` lifecycle paths were migrated from direct OS process APIs to `core::ProcessControl`.

Updated files:
1. `include/scratchbird/server/service_controller.h`
2. `src/server/service_controller.cpp`

## Key Changes
1. Added `process_control_` member initialized via `createDefaultProcessControl()`.
2. Replaced direct spawn logic in:
   - `launchManagerProcess()`
   - `launchListenerProcess()`
3. Replaced direct wait/terminate logic in:
   - `waitForListenerExit()`
   - `waitForManagerExit()`
   - `forceTerminateListener()`
   - `forceTerminateManager()`
4. Replaced direct exit checks in:
   - `checkListeners()`
   - `checkManager()`
5. Normalized listener/manager runtime state to `core::SpawnedProcess`.

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
- Tracker row: `XOS-014`
