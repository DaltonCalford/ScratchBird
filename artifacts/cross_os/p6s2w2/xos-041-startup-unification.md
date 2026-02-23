# XOS-041 Startup Unification
Last-Modified: 2026-02-22

## Implemented
- Added explicit startup-mode override surface in service controller:
  - `ServiceController::StartupMode`
  - `ServiceController::runWithStartupMode(...)`
  - Files:
    - `include/scratchbird/server/service_controller.h`
    - `src/server/service_controller.cpp`
- Updated `sb_server` entrypoint to route through one startup path:
  - Parses launcher-only options.
  - Runs `ServiceController::parseCommandLine(...)`.
  - Uses `runWithStartupMode(...)` for both normal and service launches.
  - File:
    - `src/server/sb_server_main.cpp`

## Result
- Foreground, daemon, and service launcher flows now converge on the same service-controller lifecycle logic, reducing mode-specific drift.

## Validation
- `ctest --test-dir build -R 'ServiceControllerListenerBootstrapTest.*' --output-on-failure`
  - pass
- Included in aggregate gate run:
  - `artifacts/cross_os/p6s2w2/xos-040-043-ctest.txt`
