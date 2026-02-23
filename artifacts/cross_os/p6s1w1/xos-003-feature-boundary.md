# XOS-003 Feature Boundary Classification
Last-Modified: 2026-02-22

## Inventory Basis
Source inventory: `artifacts/cross_os/p6s1w1/xos-002-posix-inventory.csv`

Observed owned-code surface (excluding vendored upstream repos/runtime copies):
1. POSIX include matches: `312`
2. POSIX API matches (core set): `247`
3. Highest-concentration source files:
   - `src/server/daemon.cpp`
   - `src/core/job_scheduler.cpp`
   - `src/server/ipc_unix.cpp`
   - `src/server/service_controller.cpp`
   - `src/core/database.cpp`
   - `src/network/sb_listener_main.cpp`

## Boundary Decision
### A. Portable Core (Must work on Linux and Windows)
1. Parser/emitter/executor semantics.
2. Listener/session/auth flow and policy behavior.
3. Catalog, storage, transaction, and SBLR semantics.
4. Wire protocol framing and message contracts.

### B. Linux-Specific Runtime Paths (Feature-gated)
1. `systemd` notification integration.
2. `fork`/`waitpid` process model paths.
3. Unix domain socket path and chmod ownership semantics.
4. Signal map tied to POSIX signal set (`SIGTERM`, `SIGHUP`, `SIGUSR*`).

### C. Windows-Required Equivalents
1. Service lifecycle wrapper for foreground/service operation.
2. Local IPC policy equivalent (Windows local transport + ACL policy).
3. Process control abstraction for listener/manager launch and shutdown.
4. Event/signal abstraction for stop/reload/stats controls.

### D. Test-Lane Boundary
1. Portable tests: required on Linux and Windows.
2. Linux-only tests: retained but explicitly tagged (`linux_only`) with documented reason.
3. Upstream emulation suites remain Linux-gated in this cycle.

## Acceptance Constraint
No semantic feature may be moved into Linux-only scope unless explicitly approved as a gate decision.

## Gate Binding
- Gate: `XOS-GATE-01`
- Tracker row: `XOS-003`
