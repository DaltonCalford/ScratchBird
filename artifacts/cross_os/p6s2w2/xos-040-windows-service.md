# XOS-040 Windows Service Wrapper
Last-Modified: 2026-02-22

## Implemented
- Added `WindowsServiceHost` abstraction:
  - `include/scratchbird/server/windows_service.h`
  - `src/server/windows_service.cpp`
- Added `sb_server` launcher support for service flags:
  - `--windows-service`
  - `--windows-service-name <name>`
- Wired launcher to use service host callbacks:
  - console path uses `runConsole()`
  - service path uses `runAsService()` with shutdown callback into `ServiceController`
- Added service host unit tests:
  - `tests/unit/test_windows_service_host.cpp`

## Behavior
- On Windows:
  - `runAsService()` uses SCM dispatcher + stop handler and runs the existing server lifecycle callback on a worker thread.
- On non-Windows:
  - `runAsService()` returns `Status::NOT_SUPPORTED` with explicit context.

## Validation
- `ctest --test-dir build -R 'WindowsServiceHostTest.*' --output-on-failure`
  - pass
- Included in aggregate XOS gate run:
  - `artifacts/cross_os/p6s2w2/xos-040-043-ctest.txt`
  - `artifacts/cross_os/p6s2w2/xos-040-043-command-log.txt`
