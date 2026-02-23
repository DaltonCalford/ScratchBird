# Cross-OS Portability Guide
Last modified: 2026-02-22

## Objective

Keep shared engine/runtime code portable across Linux and Windows while preserving behavior.

## Adapter Rules

1. Do not introduce direct POSIX-only calls in shared runtime paths (`src/core`, `src/server`, `src/network`) unless routed through platform adapters.
2. Route process lifecycle through:
   - `src/core/process_control.cpp`
   - `src/core/signal_control.cpp`
3. Route filesystem permission logic through:
   - `src/core/file_permissions.cpp`
4. Keep listener/server IPC mediation behind:
   - `src/network/listener_ipc_adapter.cpp`
5. Keep service-mode wiring behind:
   - `src/server/windows_service.cpp`
   - `src/server/daemon.cpp`

## Test Labeling Contract

1. Cross-OS-safe tests are part of the portable lane.
2. Linux-only tests must be explicitly tagged/covered by skip policy.
3. Current linux-only exclusions:
   - `UnixSocketTest.*`
   - `TSAN_*`

## Build/Test Contract

1. Use CMake presets (`CMakePresets.json`) instead of ad hoc toolchain flags.
2. Required build profiles:
   - `linux-gcc-debug`
   - `linux-clang-debug`
   - `windows-msvc-debug`
   - `linux-mingw-windows-x64`
3. Portable lane runner:
   - `scripts/cross_os/run_portable_lane.sh`

## CI Gate Contract

1. Merge gate workflow:
   - `.github/workflows/cross-os-gate.yml`
2. Required aggregator check:
   - `Cross OS Required Checks`
3. Nightly workflow:
   - `.github/workflows/cross-os-nightly.yml`

