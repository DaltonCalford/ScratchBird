# XOS-020 Listener Accept And Handoff Migration To IPC Adapter

Last-Modified: 2026-02-22

## Scope
Migrated `sb_listener_main` runtime accept/control paths to the new listener IPC adapter boundary.

## Code Changes
- Updated `src/network/sb_listener_main.cpp`
  - Added include: `scratchbird/network/listener_ipc_adapter.h`
  - Replaced direct local control-plane objects with adapter instances:
    - `createDefaultLocalControlChannel()` for control and management channels
  - Replaced direct front-door socket create/bind/listen with adapter instance:
    - `createDefaultListenerSocketAcceptor()`
    - `front_door->start(...)`
    - `front_door->accept(...)`
  - Updated shutdown/cleanup paths to use adapter methods:
    - `control_plane->stop()`
    - `management_plane->stop()`
    - `front_door->close()`

## Validation
- Build: `cmake --build build -j4`
- Tests: `ctest --test-dir build -R 'ListenerIpcAdapterTest|ServiceControllerListenerBootstrapTest|SignalControlTest' --output-on-failure`
- Results: `artifacts/cross_os/p6s1w3/xos-019-020-listener-ctest.txt`

## Result
`XOS-020` completed with listener accept-loop migration to the adapter boundary.
