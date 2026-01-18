# Network and Listeners

**Purpose:** Documents ScratchBird's network listener architecture - how connections are accepted, routed to parsers, and managed.

**Status:** Alpha documentation (in progress)

---

## Overview

ScratchBird exposes dedicated listeners per protocol. Each listener:

1. **Accepts** incoming connections on its port
2. **Hands off** the socket to a parser worker
3. **Never** parses SQL (parser does that)

```
┌─────────────────────────────────────────────────────────────┐
│                  NETWORK LAYER                               │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐    │
│  │            PROTOCOL LISTENERS                        │    │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐   │    │
│  │  │ Native  │ │ PG      │ │ MySQL   │ │ Firebird│   │    │
│  │  │ :3092   │ │ :5432   │ │ :3306   │ │ :3050   │   │    │
│  │  └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘   │    │
│  └───────┴───────────┴───────────┴───────────┴────────┘    │
│                          │                                  │
│                          ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              PARSER POOLS (per protocol)             │    │
│  └─────────────────────────────────────────────────────┘    │
│                          │                                  │
│                          ▼                                  │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              ENGINE CORE                             │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Protocol Ports

| Protocol | Default Port | Status |
|----------|-------------|--------|
| ScratchBird Native | 3092 | Alpha |
| PostgreSQL | 5432 | Alpha |
| MySQL | 3306 | Alpha |
| Firebird | 3050 | Alpha |
| TDS/SQL Server | 1433 | Post-gold |

---

## Trust Model

```
┌─────────────────────────────────────────────────────────────┐
│                    TRUST BOUNDARIES                          │
├─────────────────────────────────────────────────────────────┤
│  UNTRUSTED                                                   │
│  ├─ Listener (accepts connections, never parses SQL)        │
│  └─ Wire Protocol Layer (encoding/decoding only)            │
├─────────────────────────────────────────────────────────────┤
│  SEMI-TRUSTED                                                │
│  └─ Parser (generates SBLR, validated by engine)            │
├─────────────────────────────────────────────────────────────┤
│  TRUSTED                                                     │
│  └─ Engine Core (validates SBLR, enforces security)         │
└─────────────────────────────────────────────────────────────┘
```

**Key Principle:** The listener is untrusted and never parses SQL. The parser generates SBLR that the engine validates. Authentication decisions are made by the engine.

---

## Connection Lifecycle

```
1. Listener accepts TCP connection
        │
        ▼
2. Network policy applied (max_connections, queue)
        │
        ▼
3. Parser pool selects worker
        │
        ▼
4. Socket handoff (listener → parser)
        │
        ▼
5. Parser performs protocol handshake
        │
        ▼
6. Engine authenticates via TIP
        │
        ▼
7. Parser handles queries (SQL → SBLR → Engine → Results)
        │
        ▼
8. Connection closes, parser recycled
```

### Connection States (Listener View)

```
ACCEPTED → QUEUED → HANDED_OFF → CLOSED
                  \→ REJECTED
```

### Parser States (Worker View)

```
IDLE → ASSIGNED → SERVING → IDLE or RECYCLE
```

---

## Parser Pool

Each listener maintains a parser pool for its protocol.

### Pool Configuration

```yaml
parser_pool:
  min_size: 4          # Minimum idle workers
  max_size: 64         # Maximum workers
  prewarm: true        # Spawn min_size at startup
  spawn_strategy: prefork  # prefork | on_demand
  max_requests_per_parser: 1000  # Recycle after N sessions
  max_age_seconds: 3600    # Recycle after time limit
  health_check_interval_ms: 5000
  recycle_on_error: true
```

### Pool Operations

- **Prewarm:** Spawn `min_size` workers at startup
- **Scale up:** Spawn workers on demand up to `max_size`
- **Health check:** Ping workers periodically
- **Recycle:** Replace workers after max requests/age
- **Replace:** Restart crashed workers

---

## Socket Handoff

The listener hands off accepted sockets to parser workers.

### Handoff Mechanism

- **Unix:** `sendmsg` + `SCM_RIGHTS`
- **Windows:** `WSADuplicateSocket` + IPC channel

### Handoff Metadata

```cpp
struct ListenerHandoff {
    uint64_t connection_id;
    int socket_fd;
    char protocol[16];        // scratchbird/postgresql/mysql/firebird
    char listener_name[64];
    char client_addr[48];
    uint16_t client_port;
    bool tls_active;
    uint32_t initial_bytes_len;
    uint8_t initial_bytes[64];  // For protocol detection
};
```

---

## Configuration

### Listener Configuration (YAML)

```yaml
server:
  listeners:
    - name: native
      protocol: scratchbird
      enabled: true
      address: 0.0.0.0
      port: 3092
      max_connections: 2000
      handshake_timeout_ms: 5000
      queue_limit: 256
      queue_timeout_ms: 2000
      parser_pool:
        min_size: 4
        max_size: 64
        prewarm: true
      tls:
        mode: required          # disabled | optional | required
        cert_path: /etc/scratchbird/tls/server.crt
        key_path: /etc/scratchbird/tls/server.key

    - name: postgresql
      protocol: postgresql
      enabled: true
      address: 0.0.0.0
      port: 5432
      max_connections: 1000
      parser_pool:
        min_size: 8
        max_size: 128
        prewarm: true

    - name: mysql
      protocol: mysql
      enabled: true
      address: 0.0.0.0
      port: 3306
      max_connections: 1000
      parser_pool:
        min_size: 8
        max_size: 128

    - name: firebird
      protocol: firebird
      enabled: true
      address: 0.0.0.0
      port: 3050
      max_connections: 500
      parser_pool:
        min_size: 4
        max_size: 64
```

### Config Precedence

1. Command line flags
2. Environment variables
3. Server config file
4. Built-in defaults

---

## Server Startup Sequence

1. Parse command line and load config
2. Verify directories and permissions
3. Check for existing PID file
4. Clean up stale IPC artifacts
5. Initialize TLS contexts
6. Create listener instances
7. Prewarm parser pools
8. Start accept loops
9. Emit READY status

---

## Shutdown and Reload

### Graceful Shutdown

1. Stop accepting new connections
2. Signal parsers to drain
3. Wait for active connections (with timeout)
4. Force terminate remaining parsers
5. Persist metrics and exit

### Reload (SIGHUP)

1. Re-read config
2. Add/remove listeners as needed
3. Resize parser pools
4. Apply TLS changes to new connections only

---

## Failure Handling

| Failure | Response |
|---------|----------|
| Port bind failure | Listener disabled, log error |
| Pool exhausted | Queue up to limit, then reject |
| Parser crash | Replace worker, report metrics |
| Handshake timeout | Close connection, recycle parser |
| Protocol mismatch | Reject with protocol error |

---

## Observability

### Metrics per Listener

- `active_connections` - Current connections
- `queued_connections` - Waiting connections
- `accept_rate` - Accepts per second
- `handshake_time_ms` - Handshake latency
- `parser_pool_idle` - Idle workers
- `parser_pool_active` - Active workers
- `parser_restarts` - Worker restarts
- `protocol_errors` - Protocol-level errors

### Log Events

- `listener_start`, `listener_stop`
- `parser_spawn`, `parser_exit`
- `connection_handoff`
- `tls_handshake_failure`
- `auth_rejection`

---

## TLS Configuration

### TLS Modes

| Mode | Description |
|------|-------------|
| `disabled` | No TLS |
| `optional` | TLS if client supports |
| `required` | TLS mandatory |

### Certificate Files

```yaml
tls:
  mode: required
  cert_path: /etc/scratchbird/tls/server.crt
  key_path: /etc/scratchbird/tls/server.key
  ca_path: /etc/scratchbird/tls/ca.crt    # For client certs
```

---

## Source Code Reference

| Component | Location |
|-----------|----------|
| Server main | `src/server/sb_server_main.cpp` |
| Service controller | `src/server/service_controller.cpp` |
| IPC common | `src/server/ipc_common.cpp` |

---

## Related Documents

- [Architecture](Architecture.md) - Overall system design
- [Parsers](Parsers.md) - Parser layer that receives from listeners
- [Security](Security.md) - Authentication and authorization
