# XOS-018 Runtime Termination And Reload Migration

Last-Modified: 2026-02-22

## Scope
Migrated runtime termination/reload handling paths to the new core signal adapter.

## Code Changes
- Updated `include/scratchbird/server/daemon.h`
  - Added `core::SignalControl` member
  - Removed direct static signal handler ownership state
- Updated `src/server/daemon.cpp`
  - Removed in-file static signal handler wiring
  - `setupSignals()` now installs the core adapter with daemon lifecycle policy
  - `checkSignals()` now polls adapter events and maps them to `DaemonSignal`
  - `cleanup()` now uninstalls adapter
- Updated `include/scratchbird/server/scratchbird_server.h`
  - Added `core::SignalControl` member
  - Added `checkControlSignals()` helper
- Updated `src/server/scratchbird_server.cpp`
  - Removed global pointer + direct `signal(...)` registration
  - Installed signal policy through adapter at startup
  - Added adapter polling in `acceptLoop()`
  - Mapped `SHUTDOWN`/`IMMEDIATE_STOP` to graceful shutdown
  - Mapped `RELOAD` to deterministic graceful restart request (shutdown path)

## Validation
- Build: `cmake --build build -j4`
- Test gate: `ctest --test-dir build -R 'SignalControlTest|ServiceControllerListenerBootstrapTest|ProcessControlTest|FilePermissionsControlTest|AuthBootstrapClaimTest|AuthBootstrapClaimStandaloneTest' --output-on-failure`
- Results log: `artifacts/cross_os/p6s1w3/xos-017-018-runtime-ctest.txt`

## Result
`XOS-018` migration completed for runtime daemon/server termination and reload control paths.
