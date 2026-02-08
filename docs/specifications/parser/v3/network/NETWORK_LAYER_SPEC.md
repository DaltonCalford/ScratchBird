# ScratchBird Network Layer Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Overview

ScratchBird's network layer is responsible for accepting client connections,
negotiating protocol handshakes, dispatching to the correct parser pool, and
enforcing connection-level limits. The network layer MUST NOT parse SQL or
interpret database semantics; it only performs protocol framing and delegation.

TDS/MSSQL is not supported in V3. Any attempt to negotiate TDS MUST be rejected
with `ERR_FEATURE_DISABLED`.

## Scope

In scope:
- Listener sockets and TLS handling
- Protocol detection and parser dispatch
- Backpressure and connection limits
- Connection lifecycle and timeouts

Out of scope:
- SQL parsing (parser pool responsibility)
- Statement execution (engine responsibility)
- Cluster-aware listener coordination (rejected in V3)

## Architecture

1. Listener accepts connection on configured protocol ports.
2. Listener reads minimal preface bytes for protocol detection.
3. Listener selects a protocol-specific parser worker from the pool.
4. Listener hands off the socket to the parser via the control-plane protocol.
5. Parser performs protocol handshake and authenticates via engine.

## Protocol Detection

- Explicit per-port listeners are preferred.
- If protocol detection is required, the listener MUST use the minimum number
  of bytes specified by each wire protocol to disambiguate.
- Ambiguous detection MUST yield `ERR_PROTOCOL_MISMATCH` and close the socket.

## TLS Rules

- TLS is optional for ScratchBird native and required by configuration for
  PostgreSQL and MySQL emulation.
- TLS negotiation occurs at the listener; the parser MUST receive `tls_active`
  and any peer identity details as metadata.

## Connection Lifecycle

States:
- `ACCEPTED` -> `PROTOCOL_DETECTED` -> `HANDED_OFF` -> `CLOSED`

Timeouts (configurable):
- `accept_timeout_ms`
- `handshake_timeout_ms`
- `idle_timeout_ms`

If a timeout occurs, the listener MUST close the connection and emit a metric.

## Backpressure and Limits

- Max connections per protocol and total (`max_conn_total`, `max_conn_postgres`, etc.).
- When limit exceeded, listener returns a protocol-appropriate error and closes.
- Listener MUST not block on parser pool creation; use `SPAWN_REQUEST` and
  reject if no worker becomes available before `handshake_timeout_ms`.

## Error Mapping

Listener-level errors map to protocol-specific errors:
- `ERR_FEATURE_DISABLED` for unsupported protocols (TDS)
- `ERR_PROTOCOL_MISMATCH` for ambiguous prefaces
- `ERR_SERVER_BUSY` for backpressure rejection
- `ERR_TLS_REQUIRED` when TLS is mandated by configuration

Exact wire-level error frames are defined in:
- `docs/specifications/parser/v3/wire_protocols/`

## Observability

The listener MUST emit metrics for:
- connection_accept_total
- connection_reject_total (by reason)
- handoff_latency_ms
- active_connections (by protocol)

Metric schemas are defined in `operations/PROMETHEUS_METRICS_REFERENCE.md`.

## Related Specs

- `docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md`
- `docs/specifications/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `docs/specifications/parser/v3/network/WIRE_PROTOCOL_SPECIFICATIONS.md`
