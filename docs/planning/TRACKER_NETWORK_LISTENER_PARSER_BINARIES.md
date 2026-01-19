# Tracker: Network Listener + Parser Binaries (Alpha IP Layer)

Status: In progress
Last Updated: 2026-01-18

## Scope
Scaffold the listener and parser agent executables plus their CLI surfaces.
The first phase focuses on getting binaries, config loading, and
protocol-specific wiring in place so subsequent iterations can implement
socket handoff, pool management, and TLS.

## Specifications
- docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- docs/specifications/network/PARSER_AGENT_SPEC.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/network/DIALECT_AUTH_MAPPING_SPEC.md
- docs/specifications/admin/SB_SERVER_NETWORK_CLI_SPECIFICATION.md
- docs/specifications/operations/LISTENER_POOL_METRICS.md
- docs/specifications/wire_protocols/ (dialect-specific details)

## Deliverables
- Listener binaries: `sb_listener_native`, `sb_listener_pg`,
  `sb_listener_mysql`, `sb_listener_fb`
- Parser agent binaries: `sb_parser_native`, `sb_parser_pg`,
  `sb_parser_mysql`, `sb_parser_fb`
- CLI flags aligned with specs, config file support
- sb_server updated to spawn listeners (native default, others by config)

## Work Items
1) Add binaries to CMake and create shared CLI/parsing utilities. (done)
2) Implement listener main:
   - Parse CLI/config (done)
   - Bind port (done)
   - Basic accept loop (handoff integration planned) (in progress)
3) Implement parser agent main:
   - Parse CLI/config (done)
   - TLS config loading (no plaintext) (done)
   - Engine IPC endpoint validation (done)
4) Update sb_server CLI parsing and config overrides for listener/pool flags. (done)
5) Wire listener process spawn/recycle (per protocol) into service controller. (in progress)
6) Emit baseline listener/pool metrics (status + counts). (in progress)

## Current Focus
- Listener spawn + restart handling and baseline metrics wiring.

## Notes
- Socket handoff and control-plane protocol are tracked separately in
  TRACKER_CONTROL_PLANE_SOCKET.md.
- Driver wiring is tracked in TRACKER_DRIVER_BOOTSTRAP.md.
