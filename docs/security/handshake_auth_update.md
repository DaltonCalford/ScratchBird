# Handshake/Auth Protocol Update (2026-02-21)

Wire/auth updates:
- Server-first auth negotiation challenge carries allowed and required methods.
- Client validates required method is in allowed set and selects compliant method.
- Token auth method is supported and validated against catalog policy.
- Bound DB UUID support added in connect flags for policy-bound sessions.

Primary files:
- `include/scratchbird/protocol/wire_protocol.h`
- `src/protocol/wire_protocol.cpp`
- `src/server/server_session.cpp`
- `src/client/connection.cpp`
