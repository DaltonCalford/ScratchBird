# XOS-042 Local IPC Policy Per OS
Last-Modified: 2026-02-22

## Implemented
- Added explicit local IPC policy contract:
  - `LocalIPCPolicy`
  - `getDefaultLocalIPCPolicy()`
  - `resolveLocalIPCPolicy(IPCMethod requested_method)`
  - Files:
    - `include/scratchbird/server/ipc_server.h`
    - `src/server/ipc_common.cpp`
- Added server listener startup policy wiring:
  - `ScratchBirdServer::startListener()` now resolves preferred method + fallback from policy.
  - Falls back from preferred local method to policy fallback when enabled.
  - File:
    - `src/server/scratchbird_server.cpp`

## Policy Defaults
- Linux/Unix:
  - preferred: `UNIX_SOCKET`
  - fallback: `TCP_LOCALHOST`
  - peer credentials supported: `true`
- Windows:
  - preferred: `NAMED_PIPE`
  - fallback: `TCP_LOCALHOST`
  - peer credentials supported: `false` (PID-only in this cycle)

## Validation
- Added and passed unit coverage:
  - `tests/unit/test_ipc_policy.cpp`
  - `LocalIPCPolicyTest.*`
- Included in aggregate gate run:
  - `artifacts/cross_os/p6s2w2/xos-040-043-ctest.txt`
