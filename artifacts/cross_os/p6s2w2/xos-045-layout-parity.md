# XOS-045 Filesystem Layout Parity
Last-Modified: 2026-02-22

## Implemented
- Cross-platform runtime identity and filesystem layout checks are enforced for:
  - IPC endpoint naming
  - PID file naming
  - Platform-specific runtime roots
- Core path contract implementation:
  - `src/server/ipc_common.cpp`
- Unit contract coverage:
  - `tests/unit/test_ipc_server.cpp`

## Validation
- Layout parity contract tests:
  - `IPCPathTest.LayoutParityUsesSharedSanitizedObjectIdentity`
  - `IPCPathTest.LayoutParityRuntimeRootsAreDeterministicByPlatform`
  - Evidence: `artifacts/cross_os/p6s2w2/xos-044-048-ctest.txt`
- Front-door IPC lifecycle coverage:
  - `ListenerIpcAdapterTest.FrontDoorSocketLifecycle`
  - Evidence: `artifacts/cross_os/p6s2w2/xos-047-cross-os-smoke-ctest.txt`
- Windows cross-build parity closure:
  - `artifacts/cross_os/p6s2w2/xos-044-048-mingw-configure.txt`
  - `artifacts/cross_os/p6s2w2/xos-044-048-mingw-build.txt`

