# XOS-046 Runtime Lifecycle Integration Tests
Last-Modified: 2026-02-22

## Implemented
- Lifecycle recovery test coverage for listener and manager proxy restart behavior.
- Startup mode/service host contract coverage maintained for cross-platform runtime paths.
- Primary test suite:
  - `tests/unit/test_service_controller_listener_bootstrap.cpp`
  - `tests/unit/test_windows_service_host.cpp`

## Validation
- Lifecycle and service tests passed:
  - `ServiceControllerListenerBootstrapTest.ListenerCrashRecoveryRestartsExitedListenerProcess`
  - `ServiceControllerListenerBootstrapTest.ManagerProxyCrashRecoveryRestartsExitedManagerProcess`
  - `WindowsServiceHostTest.ConsoleModeExecutesCallback`
  - `WindowsServiceHostTest.ConsoleModeReportsFailure`
  - `WindowsServiceHostTest.ServiceModeContractIsPlatformGuarded`
  - Evidence: `artifacts/cross_os/p6s2w2/xos-044-048-ctest.txt`
- Linux rebuild completed before ctest gate:
  - `artifacts/cross_os/p6s2w2/xos-044-048-linux-build.txt`

