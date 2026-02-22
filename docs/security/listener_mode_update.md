# Listener Mode Update (2026-02-21)

Direct vs managed enforcement:
- Direct mode enforces fixed database UUID pinning where configured.
- Managed mode enforces LPREFACE/DBBT validation path before auth handling.
- Parser handoff marks bound DB UUID requirement via connect flags.

Primary files:
- `src/network/sb_listener_main.cpp`
- `src/server/sb_manager_main.cpp`
- `src/server/server_session.cpp`
- `src/parser/sb_parser_main.cpp`
