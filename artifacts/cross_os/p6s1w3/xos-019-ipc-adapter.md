# XOS-019 Socket And Local IPC Abstraction Boundary

Last-Modified: 2026-02-22

## Scope
Added an explicit listener IPC boundary for front-door socket accept and local control-channel accept paths.

## Code Changes
- Added `include/scratchbird/network/listener_ipc_adapter.h`
  - `ListenerSocketConfig`
  - `ListenerSocketAcceptor` abstraction (`start`, `accept`, `close`, `isRunning`, `boundAddress`)
  - `LocalControlChannel` abstraction (`start`, `accept`, `stop`, `isRunning`, `path`)
  - factory functions for default implementations
- Added `src/network/listener_ipc_adapter.cpp`
  - `DefaultListenerSocketAcceptor`: wraps `network::Socket` lifecycle
  - `DefaultLocalControlChannel`: wraps `network::ControlPlaneServer`

## Test Coverage
- Added `tests/unit/test_listener_ipc_adapter.cpp`
  - front-door acceptor lifecycle
  - local control channel lifecycle (platform-aware expectation)

## Gate Evidence
- Test output: `artifacts/cross_os/p6s1w3/xos-019-020-listener-ctest.txt`
- Code surface inventory: `artifacts/cross_os/p6s1w3/xos-019-020-listener-surface.txt`

## Result
`XOS-019` completed.
