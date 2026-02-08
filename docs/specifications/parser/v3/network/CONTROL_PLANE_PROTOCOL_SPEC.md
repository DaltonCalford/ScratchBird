# Listener <-> Parser Control Plane Protocol Specification

Version: 1.1
Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the control-plane protocol between network listeners and parser pools.
This protocol is used for spawning, assigning, monitoring, and recycling
parser workers. It is NOT used for client data-plane traffic.

## Scope

In scope:
- Listener <-> parser pool messaging (spawn, handoff, health, shutdown)
- Cross-platform socket handoff (Unix + Windows)
- Pool backpressure and error reporting
- Versioning and compatibility rules

Not covered in this document:
- Client wire protocols (authoritative in `/docs/specifications/parser/v3/wire_protocols/`)
- Parser <-> engine SBLR execution contract (authoritative in `ENGINE_PARSER_IPC_CONTRACT.md`)
- Cluster listener coordination (rejected in V3)

## Transport

- Local IPC only (same host):
  - Unix: AF_UNIX stream socket + SCM_RIGHTS for fd pass
  - Windows: Named pipe + WSADuplicateSocket for socket handle duplication
- Listener is the control-plane server; parser pool workers connect as clients.
- TLS is not required for local IPC; rely on IPC permissions and credential checks.

## Protocol Identifiers

Protocol ID values are fixed:
- `0x01` ScratchBird native
- `0x02` PostgreSQL
- `0x03` MySQL
- `0x04` Firebird
- `0xFF` Unknown/unsupported

TDS/MSSQL is not supported in V3. Any attempt to negotiate TDS MUST be rejected
with `SB_NET_UNSUPPORTED_PROTOCOL`.

## Framing

All control-plane messages use a fixed header followed by a payload.
All integer fields are little-endian.

Header (fixed 24 bytes):
- `magic[4]`   : "SBCT" (ScratchBird Control)
- `version_u16`: protocol version (start at 1)
- `msg_u16`    : message type (see below)
- `flags_u16`  : flags (bitfield, message-specific)
- `reserved_u16`: 0
- `request_id_u64`: request/response correlation id
- `payload_len_u64`: payload length in bytes

Constraints:
- Maximum payload length: 1 MiB
- Unknown message types MUST respond with `ERROR` and be ignored.

## Message Types

### 0x0001 HELLO
Parser -> Listener. Identify worker capabilities.
Payload (binary):
- `protocol_id:u8`
- `worker_pid:u32`
- `worker_id:u64`
- `parser_version_u32`
- `dialect_baseline:string` (e.g., "postgresql-16", "mysql-8", "firebird-5", "scratchbird")

### 0x0002 HELLO_ACK
Listener -> Parser. Confirms registration.
Payload (binary):
- `accepted:u8` (1 yes, 0 no)
- `reason_len:u16` + reason bytes (optional)

### 0x0010 SPAWN_REQUEST
Listener -> Pool Manager. Request new worker.
Payload (binary):
- `protocol_id:u8`
- `desired_count:u16`
- `reason:u16` (1=pool_min,2=burst,3=replacement)

### 0x0011 SPAWN_READY
Pool Manager -> Listener. Worker is ready.
Payload (binary):
- `protocol_id:u8`
- `worker_id:u64`
- `worker_pid:u32`

### 0x0020 HANDOFF_SOCKET
Listener -> Worker. Assign a client connection.
Payload (binary):
- `connection_id:u64`
- `protocol_id:u8`
- `client_addr:string`
- `client_port:u16`
- `tls_active:u8` (0/1)
- `initial_bytes_len:u16`
- `initial_bytes[initial_bytes_len]`

Socket handle/fd is sent out-of-band:
- Unix: SCM_RIGHTS with one fd
- Windows: WSADuplicateSocket + handle blob

### 0x0021 HANDOFF_ACK
Worker -> Listener. Confirms acceptance.
Payload (binary):
- `connection_id:u64`
- `status:u8` (0=ok,1=reject)
- `reason_len:u16` + reason bytes

### 0x0030 HEALTH_CHECK
Listener -> Worker. Ping.
Payload: none.

### 0x0031 HEALTH_REPORT
Worker -> Listener. Health + load.
Payload (binary):
- `worker_id:u64`
- `state:u8` (0=idle,1=serving,2=draining,3=fault)
- `active_sessions:u16`
- `last_error_code:u32`

### 0x0040 POOL_STATS
Worker -> Listener. Periodic stats (optional JSON).
Payload (JSON):
- protocol, worker_id, sessions_total, errors_total, avg_session_ms

### 0x0050 RECYCLE
Listener -> Worker. Graceful shutdown after current session.
Payload (binary):
- `reason:u16`

### 0x0051 SHUTDOWN
Listener -> Worker. Immediate shutdown.
Payload (binary):
- `reason:u16`

### 0x00FF ERROR
Either direction. Protocol error.
Payload (binary):
- `error_code:u32`
- `message_len:u16` + message bytes

## Error Codes

- `SB_NET_UNSUPPORTED_PROTOCOL` (0x0001)
- `SB_NET_VERSION_MISMATCH` (0x0002)
- `SB_NET_BAD_PAYLOAD` (0x0003)
- `SB_NET_AUTH_REQUIRED` (0x0004)

## Backpressure Rules

- If pool is at `max_size` and no idle worker exists, listener returns
  a protocol-appropriate rejection to the client (wire-protocol specific).
- Listener emits POOL_STATS and reject counters for visibility.

## Security Rules

- Listener MUST validate IPC peer credentials (uid/gid or SID).
- Worker MUST reject HANDOFF_SOCKET for unsupported protocols.
- Control-plane is local only; no remote connections.

## Versioning

- Major version in header. Incompatible changes bump major.
- Unknown message types MUST be ignored with ERROR response.

## Required Implementations (V3)

- HELLO/HELLO_ACK
- HANDOFF_SOCKET/HANDOFF_ACK
- HEALTH_CHECK/HEALTH_REPORT
- RECYCLE/SHUTDOWN

## Related Specs

- `docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `docs/specifications/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md`
- `docs/specifications/parser/v3/Security Design Specification/05_IPC_SECURITY.md`
