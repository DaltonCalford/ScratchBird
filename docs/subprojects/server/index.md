## Server architecture and protocol/auth overview

### Process model

- Listener -> Session -> Protocol handler -> Provider dispatch.
  - Listener accepts TCP and hands off to `Session`.
  - `Session` initializes protocol negotiation and authentication, then dispatches to providers.

Code anchors:
```324:382:/workspace/src/engine/network_server.cpp
TcpListener::TcpListener(const NetworkServerConfig& config)
...
bool TcpListener::start()
```
```546:574:/workspace/src/engine/network_server.cpp
bool ConnectionManager::handle_connection(std::unique_ptr<TcpConnection> connection)
```
```682:715:/workspace/src/engine/network_server.cpp
NetworkServer::NetworkServer(const NetworkServerConfig& config, CatalogManager* catalog)
...
initialize_worker_threads();
```
```49:76:/workspace/include/scratchbird/engine/session.h
/// Session lifecycle
bool initialize();
void run();
```

### Protocol stack

- Firebird wire protocol compatibility and ScratchBird native protocol.
- Protocol detection and handler lifecycle via `ProtocolHandlerManager`.

Code anchors:
```334:355:/workspace/src/engine/protocol_handler.cpp
ProtocolHandlerFactory::detect_protocol(const std::vector<std::uint8_t>& initial_data)
...
return ProtocolType::ScratchBirdNative;
```
```405:433:/workspace/src/engine/protocol_handler.cpp
ProtocolHandlerManager::ProtocolHandlerManager(...)
bool ProtocolHandlerManager::initialize()
```
```595:619:/workspace/src/engine/protocol_handler.cpp
bool ProtocolHandlerManager::detect_and_initialize_protocol(const std::vector<std::uint8_t>& initial_data)
```

### Firebird wire protocol (compat)

- Versions and opcodes sourced from Firebird 6.0.
- Message framing: 4-byte header + op + payload.
- Auth flow: CONNECT -> AUTH/CONT_AUTH/TRUSTED -> ATTACH -> RESPONSE.

Code anchors:
```14:30:/workspace/include/scratchbird/engine/firebird_protocol.h
namespace FirebirdProtocol { ... PROTOCOL_VERSIONxx }
```
```31:74:/workspace/include/scratchbird/engine/firebird_protocol.h
constexpr std::uint32_t op_connect ... op_cond_accept
```
```205:232:/workspace/include/scratchbird/engine/firebird_protocol.h
class FirebirdMessageFramer : public MessageFramer { ... }
```
```289:376:/workspace/include/scratchbird/engine/firebird_protocol.h
class FirebirdProtocolHandler : public ProtocolHandler { ... }
```
```19:48:/workspace/include/scratchbird/engine/firebird_protocol_handler.h
enum FirebirdProtocolOp : std::uint32_t { ... }
```

Auth negotiation examples from the remote provider client:
```430:447:/workspace/src/engine/remote_provider.cpp
if (!send_protocol_message(FirebirdProtocol::op_connect)) { ... }
```
```481:499:/workspace/src/engine/remote_provider.cpp
if (!send_protocol_message(FirebirdProtocol::op_attach, auth_data)) { ... }
```

### ScratchBird native protocol

- Textual message types with framed payloads.
- Auth flow: CONNECT -> AUTH -> QUERY/HEARTBEAT.

Code anchors:
```8:18:/workspace/include/scratchbird/engine/scratchbird_protocol.h
namespace ScratchBirdMessages { ... }
```
```55:75:/workspace/include/scratchbird/engine/scratchbird_protocol.h
class ScratchBirdFramer : public MessageFramer { ... }
```
```77:136:/workspace/include/scratchbird/engine/scratchbird_protocol.h
class ScratchBirdProtocolHandler : public ProtocolHandler { ... }
```

### Authentication providers

- Password, Trusted OS, and Two-Factor providers managed by `AuthenticationManager`.

Code anchors:
```214:294:/workspace/include/scratchbird/engine/authentication.h
class AuthenticationManager { ... }
```
```271:353:/workspace/include/scratchbird/engine/password_auth.h
class PasswordAuthenticationProvider : public AuthenticationProvider { ... }
```
```368:425:/workspace/include/scratchbird/engine/trusted_auth.h
class TrustedOSAuthenticationProvider : public AuthenticationProvider { ... }
```
```496:535:/workspace/include/scratchbird/engine/two_factor_auth.h
class TwoFactorAuthenticationProvider : public AuthenticationProvider { ... }
```
```290:372:/workspace/src/engine/authentication.cpp
AuthenticationManager::authenticate_user(AuthenticationContext& context, ...)
```

### TLS setup

- TLS configuration, context, sessions, and server lifecycle using OpenSSL.

Code anchors:
```58:97:/workspace/include/scratchbird/engine/tls_server.h
struct TLSConfiguration { ... }
```
```102:142:/workspace/include/scratchbird/engine/tls_server.h
class TLSSession { ... }
```
```147:207:/workspace/include/scratchbird/engine/tls_server.h
class TLSContext { ... }
```
```212:330:/workspace/include/scratchbird/engine/tls_server.h
class TLSServer { ... }
```
```281:306:/workspace/src/engine/tls_server.cpp
bool TLSContext::initialize(const TLSConfiguration& config)
```

### Provider dispatch (Y‑Valve)

- Route database operations to embedded/remote/legacy providers with failover and load-balancing.

Code anchors:
```20:41:/workspace/include/scratchbird/engine/provider_dispatch.h
enum class ProviderType ... struct ProviderCapabilities { ... }
```
```341:373:/workspace/include/scratchbird/engine/provider_dispatch.h
class YValveDispatcher { ... route_connection ... }
```

### Connection pooling

- Process-style pool with health monitoring, IPC setup, and stats.

Code anchors:
```34:51:/workspace/include/scratchbird/engine/connection_pool.h
struct ConnectionPoolConfig { ... }
```
```241:297:/workspace/include/scratchbird/engine/connection_pool.h
class ConnectionPool { ... }
```
```194:234:/workspace/src/engine/connection_pool.cpp
std::unique_ptr<PooledConnection> ConnectionFactory::create_connection(...)
```

### Buffers, batching, compression

- Network buffers (64KB default), auto-tuning, and monitoring.
- Network and statement batching; wire compression (zlib-compatible).

Code anchors:
```28:49:/workspace/include/scratchbird/engine/network_buffer.h
struct NetworkBufferConfig { ... }
```
```156:227:/workspace/include/scratchbird/engine/wire_compression.h
struct CompressionConfig { ... }
```
```39:72:/workspace/include/scratchbird/engine/batch_operations.h
struct BatchConfig { ... }
```

### Performance notes (Phase 11.7)

- TCP_NODELAY, keepalive tuning, buffer sizes: see Phase 11.7 plan.
- Wire compression and network batching recommended for WAN.
- Pool sizing and adaptive management for throughput.

References:
- `ProjectPlan/Phase_11.7_Performance_Optimization_TODO.md`

### Security considerations

- TLS v1.2/1.3 with client verification modes; PFS via ECDHE/DHE.
- Account lockout and 2FA flows in `AuthenticationManager` and 2FA providers.
