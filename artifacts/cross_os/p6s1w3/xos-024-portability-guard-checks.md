# XOS-024 Portability Guard Checks

## Implemented checks
- Added portability guard scanner: `tools/compliance/portable_runtime_guard.sh`
- Added CTest gate: `PortableRuntimeGuard` in `tests/CMakeLists.txt`

## Guard rules
- Portable runtime headers (`include/scratchbird/server/daemon.h`, `include/scratchbird/server/scratchbird_server.h`, `include/scratchbird/server/service_controller.h`) must not contain:
  - POSIX headers (`unistd.h`, `signal.h`, `sys/*`, `pwd.h`, `grp.h`, `fcntl.h`, `poll.h`)
  - POSIX public types (`pid_t`, `uid_t`, `gid_t`, `mode_t`)
- Runtime entrypoints (`src/server/scratchbird_server.cpp`, `src/server/service_controller.cpp`, `src/network/sb_listener_main.cpp`) must not use direct signal registration (`std::signal` / `::signal`).

## Verification
- `ctest --test-dir build -R 'PortableRuntimeGuard|SignalControlTest|ListenerIpcAdapterTest|ClockControlTest|ServiceControllerListenerBootstrapTest|ProcessControlTest|FilePermissionsControlTest|AuthBootstrapClaimTest|AuthBootstrapClaimStandaloneTest|JobScheduler' --output-on-failure`
- Result: PASS (`54/54` tests), including `PortableRuntimeGuard`.

## Notes
- Linux-specific process control in listener/scheduler remains intentionally gated for later portability slices (`XOS-040+`).
