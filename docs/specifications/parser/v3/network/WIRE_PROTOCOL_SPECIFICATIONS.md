# ScratchBird Wire Protocol Requirements (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the protocol compatibility baselines and required behaviors for
ScratchBird's protocol parsers. This document is the normative baseline for
PostgreSQL/MySQL/Firebird emulation at the wire level.

## Protocol Support Matrix (Normative)

- PostgreSQL v3 wire protocol: supported (PostgreSQL 16+ behavior)
- MySQL client/server protocol: supported (MySQL 8.x behavior)
- Firebird remote protocol: supported (Firebird 5.x behavior)
- TDS/MSSQL: NOT supported; MUST reject with `ERR_FEATURE_DISABLED`

## Required Wire Protocols

### PostgreSQL v3

Required behaviors:
- StartupMessage parsing (protocol version 3.0)
- Authentication: cleartext, MD5, SCRAM-SHA-256
- Simple query protocol (Q)
- Extended query protocol (Parse/Bind/Describe/Execute/Sync)
- COPY protocol (COPY IN/OUT/BOTH)
- Text and binary data formats for all V3 supported types

### MySQL 8.x

Required behaviors:
- Handshake v10
- Capability negotiation
- Authentication plugins: mysql_native_password, caching_sha2_password, sha256_password
- COM_QUERY, COM_PREPARE, COM_EXECUTE, COM_FIELD_LIST, COM_QUIT
- Result set: column definition packets, row data (text/binary)
- Prepared statement binary result format

### Firebird 5.x

Required behaviors:
- op_connect / op_accept
- op_attach / op_create
- Transaction API (TPB handling)
- op_prepare_statement / op_execute / op_execute2 / op_fetch
- XDR encoding for data transfer
- Blob and array protocol

## Error Handling

- Unsupported protocol negotiation MUST close the socket with `ERR_FEATURE_DISABLED`.
- Protocol mismatch MUST close the socket with `ERR_PROTOCOL_MISMATCH`.
- Authentication failure MUST return protocol-appropriate auth error (no engine details).

## Backpressure

Wire-level backpressure behavior is defined in `NETWORK_LAYER_SPEC.md`.
The listener MUST reject when no parser worker is available within
`handshake_timeout_ms`.

## Required Tests

- Protocol handshake conformance for each supported protocol
- Authentication method acceptance/rejection matrix
- Basic query execution over simple and extended protocols
- COPY protocol validation (PostgreSQL)
- Prepared statement execution (MySQL, PostgreSQL)

## Related Specs

- `docs/specifications/parser/v3/network/NETWORK_LAYER_SPEC.md`
- `docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md`
- `docs/specifications/parser/v3/network/DIALECT_AUTH_MAPPING_SPEC.md`
- `docs/specifications/parser/v3/wire_protocols/`
