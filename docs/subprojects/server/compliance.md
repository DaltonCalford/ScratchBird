## Server/Protocol/Auth compliance status

This document tracks implementation coverage against the Phase 11 overall server/protocol/auth spec.

### Scope
- Listener/session/process model
- Firebird wire protocol compatibility
- ScratchBird native protocol
- Authentication (Password, Trusted OS, 2FA)
- TLS
- Provider dispatch (Y‑Valve)
- Connection pooling
- Buffers, batching, compression

### Status summary

- Listener/session/process: In place (accept, session creation, worker management)
  - Anchors:
```326:368:/workspace/src/engine/network_server.cpp
TcpListener::TcpListener ... bool TcpListener::start()
```
```558:574:/workspace/src/engine/network_server.cpp
Session(...) and initialize() via ConnectionManager::handle_connection
```

- Protocol detection/handlers: Implemented framework; Firebird native handlers scaffolded
  - Anchors:
```334:355:/workspace/src/engine/protocol_handler.cpp
ProtocolHandlerFactory::detect_protocol(...)
```
```405:452:/workspace/src/engine/protocol_handler.cpp
ProtocolHandlerManager lifecycle and queues
```
```289:376:/workspace/include/scratchbird/engine/firebird_protocol.h
FirebirdProtocolHandler API
```
```77:136:/workspace/include/scratchbird/engine/scratchbird_protocol.h
ScratchBirdProtocolHandler API
```
  - Gaps: Full Firebird op handling and end-to-end tests are partial (see TODOs in handler).

- Firebird wire framing/opcodes: Defined; framer present
  - Anchors:
```31:74:/workspace/include/scratchbird/engine/firebird_protocol.h
op_connect/op_accept/... op_cond_accept
```
```205:232:/workspace/include/scratchbird/engine/firebird_protocol.h
FirebirdMessageFramer
```

- Authentication providers: Implemented (Password, TrustedOS, 2FA) with manager, lockout, audit
  - Anchors:
```271:353:/workspace/include/scratchbird/engine/password_auth.h
PasswordAuthenticationProvider
```
```368:425:/workspace/include/scratchbird/engine/trusted_auth.h
TrustedOSAuthenticationProvider
```
```496:535:/workspace/include/scratchbird/engine/two_factor_auth.h
TwoFactorAuthenticationProvider
```
```290:372:/workspace/src/engine/authentication.cpp
AuthenticationManager::authenticate_user
```
  - Gaps: 2FA SMS/Email backends require external integrations; certificate auth pending.

- TLS: Config/context/session/server implemented; stats and audit present
  - Anchors:
```58:97:/workspace/include/scratchbird/engine/tls_server.h
TLSConfiguration
```
```212:330:/workspace/include/scratchbird/engine/tls_server.h
TLSServer lifecycle and stats
```
```724:758:/workspace/src/engine/tls_server.cpp
perform_tls_handshake and audit
```
  - Gaps: End-to-end integration with listener accept path; OCSP stapling mocked.

- Provider dispatch (Y‑Valve): Interfaces and dispatcher with load balancing/failover knobs
  - Anchors:
```341:373:/workspace/include/scratchbird/engine/provider_dispatch.h
YValveDispatcher routing APIs
```
  - Gaps: Concrete provider registrations and end-to-end wiring in session flow.

- Connection pooling: Process-style pool with health monitor and stats
  - Anchors:
```34:51:/workspace/include/scratchbird/engine/connection_pool.h
ConnectionPoolConfig
```
```241:297:/workspace/include/scratchbird/engine/connection_pool.h
ConnectionPool
```
```148:178:/workspace/src/engine/connection_pool.cpp
PooledConnection::perform_health_check
```
  - Gaps: Pool integration with listener/session; cross-platform worker bootstrap.

- Buffers, batching, compression: APIs present
  - Anchors:
```28:49:/workspace/include/scratchbird/engine/network_buffer.h
NetworkBufferConfig
```
```39:72:/workspace/include/scratchbird/engine/batch_operations.h
BatchConfig
```
```127:158:/workspace/include/scratchbird/engine/wire_compression.h
CompressionConfig
```
  - Gaps: Wiring into protocol handlers; adaptive tuning heuristics validation.

### Performance items (Phase 11.7)

- Planned: TCP tuning, wire compression, message batching, adaptive pool sizing, buffer tuning.
  - Reference: `ProjectPlan/Phase_11.7_Performance_Optimization_TODO.md`

### Compliance matrix (high level)
- Firebird compatibility: Partial (framing/opcodes/versioning defined; full op coverage in progress)
- ScratchBird native: Implemented base messaging; feature build-out ongoing
- Auth providers: Implemented core flows; external integrations pending
- TLS: Implemented core; production hardening in progress
- Provider dispatch: Interfaces + dispatcher; provider impl/wiring pending
- Pooling/buffers/batching/compression: APIs present; integration work remaining