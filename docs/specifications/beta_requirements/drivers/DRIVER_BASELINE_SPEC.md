# ScratchBird V2 Driver Baseline Specification

Status: Draft (Target). This document defines the shared requirements for all official ScratchBird V2 drivers.

## Scope
- **Supported protocol**: ScratchBird Native Wire Protocol (SBWP) v1.1 only.
- **Unsupported**: PostgreSQL/MySQL/Firebird/TDS emulation. Those ecosystems use their own drivers.
- **Transport**: TCP (default port 3092) and optional Unix domain socket where supported by the OS.

## Security and transport
- **TLS 1.3 is mandatory** for SBWP. Drivers must refuse plaintext by default.
- Supported auth methods (per `wire_protocols/scratchbird_native_wire_protocol.md`):
  - SCRAM-SHA-256 (required)
  - Password (legacy, only if server permits)
  - TLS client certificate
  - OAuth2/OIDC token
  - MFA TOTP (if server requests)
- Drivers must validate server certificates and support client certificates.

## Protocol handshake (summary)
- TLS handshake -> STARTUP -> AUTH_REQUEST/AUTH_RESPONSE -> AUTH_OK -> READY.
- After AUTH_OK, drivers must send attachment_id and txn_id in every message header.

## Query execution
- **Simple query**: `QUERY` with SQL text.
- **Extended**: `PARSE` + `BIND` + `EXECUTE` for prepared statements.
- **Optional**: `SBLR_EXECUTE` for precompiled bytecode if the driver implements it.
- **Cancel**: `CANCEL` with MSG_FLAG_URGENT.
- **Streaming**: use `STREAM_CONTROL` for backpressure.

## Parameter placeholders
- V2 SQL parser accepts **$1, $2, ...** and **:name** placeholders.
- The **'?'** placeholder is not supported by V2; drivers must rewrite '?' to $1..N.

## Type encoding
- Binary encoding is the default; text encoding must follow canonical formats in
  `DATA_TYPE_PERSISTENCE_AND_CASTS.md`.
- Drivers must round-trip all supported types without loss where possible.

## Error handling
- Drivers must preserve SQLSTATE codes and server error fields.
- Map SQLSTATE to driver-native exception classes and error codes.

## Session semantics
- Server uses an implicit transaction after AUTH_OK. Drivers implement autocommit
  by issuing COMMIT after each successful statement when autocommit is enabled.
- Timezone handling: use server timezone unless client overrides via session option.

## Performance requirements
- Prefer binary protocol and prepared statements by default.
- Provide statement cache and batch execution where the driver API allows it.
- Support streaming for large result sets.

## Canonical defaults
- LOB streaming: BLOB/TEXT/JSONB/GEOMETRY stream by default over binary protocol.
- Inline threshold: 8 MiB; chunk size: 128 KiB; backpressure via STREAM_CONTROL and native stream primitives.
- COPY/bulk: explicit CopyIn/CopyOut API only; binary default; text only when requested.
- Notifications: dedicated connection required for listen/notify APIs.
- Cancel: send CANCEL; if no response within 5000 ms, close the connection.
- Statement cache: per-connection LRU, default 64 entries; auto-prepare after 5 uses; invalidate on prepare/schema errors.
- Pooling: prefer language-standard pools; drivers without standard pools ship optional pooling disabled by default.
- Logging: disabled by default; integrate with language-standard logging.
- Packaging: idiomatic naming per ecosystem; wrapper types live in the primary namespace.

## References
- `wire_protocols/scratchbird_native_wire_protocol.md`
- `DATA_TYPE_PERSISTENCE_AND_CASTS.md`
- `SECURITY DESIGN SPECIFICATION/*`
- `CONNECTION_POOLING_SPECIFICATION.md`
