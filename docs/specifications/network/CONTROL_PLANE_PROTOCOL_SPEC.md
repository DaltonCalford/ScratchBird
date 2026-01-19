# Listener <-> Parser Control Plane Protocol Specification

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

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

Out of scope:
- Client wire protocols (see docs/specifications/wire_protocols/)
- Parser <-> engine SBLR execution contract
- Cluster listener coordination (Beta)

## Transport

- Local IPC only (same host):
  - Unix: AF_UNIX stream socket + SCM_RIGHTS for fd pass
  - Windows: Named pipe + WSADuplicateSocket for socket handle duplication
- Listener is the control-plane server; parser pool workers connect as clients.
- TLS is not required for local IPC; rely on IPC permissions and credential checks.

## Socket Naming and Location

Control sockets are configurable via `control_socket_dir` in server config.

Recommended names (per protocol):
- Unix: `${control_socket_dir}/sb_listener.<protocol>.sock`
- Windows: `\\\\.\\pipe\\scratchbird\\<protocol>\\listener`

These names isolate protocol types and avoid collisions.

## Framing

All control-plane messages use a fixed header followed by a payload.
All integer fields are little-endian.

Header (fixed 24 bytes):

- magic[4]   : "SBCT" (ScratchBird Control)
- version_u16: protocol version (start at 1)
- msg_u16    : message type (see below)
- flags_u16  : flags (bitfield, message-specific)
- reserved_u16: 0
- request_id_u64: request/response correlation id
- payload_len_u64: payload length in bytes

Payload: msg-specific structure (binary or UTF-8 JSON where noted).

## Message Types

### 0x0001 HELLO
Parser -> Listener. Identify worker capabilities.
Payload (binary):
- protocol[16] : "scratchbird" | "postgresql" | "mysql" | "firebird"
- worker_pid_u32
- worker_id_u64
- parser_version_u32

### 0x0002 HELLO_ACK
Listener -> Parser. Confirms registration.
Payload (binary):
- accepted_u8 (1 yes, 0 no)
- reason_len_u16 + reason bytes (optional)

### 0x0010 SPAWN_REQUEST
Listener -> Pool Manager. Request new worker.
Payload (binary):
- protocol[16]
- desired_count_u16
- reason_u16 (1=pool_min,2=burst,3=replacement)

### 0x0011 SPAWN_READY
Pool Manager -> Listener. Worker is ready.
Payload (binary):
- protocol[16]
- worker_id_u64
- worker_pid_u32

### 0x0020 HANDOFF_SOCKET
Listener -> Worker. Assign a client connection.
Payload (binary):
- connection_id_u64
- protocol[16]
- client_addr[48] (string, zero-terminated)
- client_port_u16
- tls_active_u8 (0/1)
- initial_bytes_len_u16
- initial_bytes[64] (optional, prefix for protocol detection)

Socket handle/fd is sent out-of-band:
- Unix: SCM_RIGHTS with one fd
- Windows: WSADuplicateSocket + handle blob

### 0x0021 HANDOFF_ACK
Worker -> Listener. Confirms acceptance.
Payload (binary):
- connection_id_u64
- status_u8 (0=ok,1=reject)
- reason_len_u16 + reason bytes

### 0x0030 HEALTH_CHECK
Listener -> Worker. Ping.
Payload: none.

### 0x0031 HEALTH_REPORT
Worker -> Listener. Health + load.
Payload (binary):
- worker_id_u64
- state_u8 (0=idle,1=serving,2=draining,3=fault)
- active_sessions_u16
- last_error_code_u32

### 0x0040 POOL_STATS
Worker -> Listener. Periodic stats (optional JSON).
Payload (JSON):
- protocol, worker_id, sessions_total, errors_total, avg_session_ms

### 0x0050 RECYCLE
Listener -> Worker. Graceful shutdown after current session.
Payload (binary):
- reason_u16

### 0x0051 SHUTDOWN
Listener -> Worker. Immediate shutdown.
Payload (binary):
- reason_u16

### 0x00FF ERROR
Either direction. Protocol error.
Payload (binary):
- error_code_u32
- message_len_u16 + message bytes

## Backpressure Rules

- If pool is at max_size and no idle worker exists, listener returns
  a REJECT to the client with a protocol-appropriate error.
- Listener emits POOL_STATS and reject counters for visibility.

## Security Rules

- Listener must validate IPC peer credentials (uid/gid or SID).
- Worker must reject HANDOFF_SOCKET for unsupported protocols.
- Control-plane is local only; no remote connections.

## Versioning

- Major version in header. Incompatible changes bump major.
- Unknown message types must be ignored with ERROR response.

## Required Implementations (Alpha)

- HELLO/HELLO_ACK
- HANDOFF_SOCKET/HANDOFF_ACK
- HEALTH_CHECK/HEALTH_REPORT
- RECYCLE/SHUTDOWN

## Related Specs

- docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- docs/specifications/network/ENGINE_PARSER_IPC_CONTRACT.md
- docs/specifications/Security Design Specification/05_IPC_SECURITY.md
