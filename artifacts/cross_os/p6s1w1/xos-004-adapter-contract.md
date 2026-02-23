# XOS-004 Platform Adapter Contract
Last-Modified: 2026-02-22

## Purpose
Define the adapter seams required to remove direct POSIX calls from portable runtime code paths.

## Adapter Families
### 1. Process Adapter (`platform::IProcessControl`)
Responsibilities:
1. Spawn child process with argument vector and environment.
2. Poll process exit and retrieve exit code.
3. Terminate process gracefully or forcefully.
4. Query current process identity metadata needed by runtime checks.

Primary migration targets:
1. `src/server/daemon.cpp`
2. `src/server/service_controller.cpp`
3. `src/core/job_scheduler.cpp`

### 2. Filesystem Permission Adapter (`platform::IFilePermissions`)
Responsibilities:
1. Apply secure mode/ACL policy to files and directories.
2. Validate owner/group/mode policy for bootstrap/auth files.
3. Create/remove directories with policy defaults.

Primary migration targets:
1. `src/core/database.cpp`
2. `src/core/auth_provider.cpp`
3. `src/server/config_parser.cpp`

### 3. Signal/Event Adapter (`platform::ISignalControl`)
Responsibilities:
1. Register runtime control callbacks (shutdown/reload/stats).
2. Translate OS-specific control events into canonical runtime events.
3. Handle no-op mappings when an event is unsupported on target OS.

Primary migration targets:
1. `src/server/daemon.cpp`
2. `src/server/service_controller.cpp`
3. `src/server/sb_manager_main.cpp`

### 4. IPC/Socket Adapter (`platform::IIpcEndpoint`)
Responsibilities:
1. Bind/listen/accept local and TCP endpoints through unified contract.
2. Handle local endpoint path/ACL creation policy.
3. Expose peer identity attributes for auth policy integration.

Primary migration targets:
1. `src/server/ipc_unix.cpp`
2. `src/server/ipc_tcp.cpp`
3. `src/network/sb_listener_main.cpp`
4. `src/ipc/parser_agent.cpp`

### 5. Clock/Timer Adapter (`platform::IClockSource`)
Responsibilities:
1. Monotonic and wall-clock time acquisition.
2. Sleep/wait abstractions for retry loops and backoff.
3. Consistent timestamp conversion across OS targets.

Primary migration targets:
1. `src/core/job_scheduler.cpp`
2. `src/network/event_loop.cpp`
3. `src/server/service_controller.cpp`

## Non-negotiable Rules
1. Adapter interfaces must not encode dialect/parser semantics.
2. Adapters must be deterministic and testable with mock implementations.
3. Linux-specific and Windows-specific code must remain behind adapter boundaries.
4. New direct POSIX includes in portable modules are blocked by guard checks in `XOS-024`.

## Gate Binding
- Gate: `XOS-GATE-01`
- Tracker row: `XOS-004`
