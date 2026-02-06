# Conformance Server Runbook (Native Driver)

This document tells another agent how to verify driver conformance against the running native listener and where to find the full ScratchBird wire protocol spec.

## Current Conformance Server

- **Host:** localhost
- **Port:** 5439
- **Database:** /tmp/sb_conformance.sbdb
- **Auth user/pass:** SYSARCH / ScratchBirdBeta1!
- **Protocol:** scratchbird native
- **TLS:** disabled for this conformance run (use `sslmode=disable`)

## TLS Conformance Server (Optional)

If you need TLS-enabled driver runs, use the pre-staged config at:

- `/home/dcalford/CliWork/sb_server_conformance_tls.conf`

Example TLS DSN:

```
scratchbird://SYSARCH:ScratchBirdBeta1!@localhost:5440/sb_conformance_tls?sslmode=require&sslrootcert=/tmp/sb_tls/ca.crt
```

The server is running from this workspace build and is started with a fixed config file at `/tmp/sb_server_conformance.conf`.

## How to Check It Is Running

```bash
pgrep -af sb_server
pgrep -af sb_listener_native
pgrep -af sb_parser_native
ss -ltnp | rg ':5439'
```

Expected: `sb_listener_native` bound to 0.0.0.0:5439 and a `sb_parser_native` worker.

## How to (Re)Start the Conformance Server

```bash
cat <<'CONF' > /tmp/sb_server_conformance.conf
[server]
mode = single-database
database = /tmp/sb_conformance.sbdb
auto_create = true
pid_file = /tmp/scratchbird/sb_server.pid

[network]
bind_address = 0.0.0.0
control_socket_dir = /tmp/scratchbird
native_port = 5439
native_pool_min = 1
native_pool_max = 1
native_health_check_interval_ms = 0
pg_port = 0
mysql_port = 0
fb_port = 0
CONF

nohup env PATH="/home/dcalford/CliWork/ScratchBird/build/src:$PATH" \
  /home/dcalford/CliWork/ScratchBird/build/src/sb_server -F -v \
  -c /tmp/sb_server_conformance.conf --control-socket-dir /tmp/scratchbird \
  > /tmp/sb_conformance_server.log 2>&1 &
```

## How to Stop the Conformance Server

```bash
pkill -f sb_server
pkill -f sb_listener_native
pkill -f sb_parser_native
```

## Conformance Harness Command

```bash
python scripts/run_driver_conformance.py \
  --dsn 'scratchbird://SYSARCH:ScratchBirdBeta1!@localhost:5439/sb_conformance?sslmode=disable' \
  --adapter ./build/src/sbdriver-conformance
```

Notes:
- The adapter is built by the normal `cmake --build build -j 24` step.
- The harness reads JSON from the manifest in `docs/fixtures/driver_conformance_manifest.json`.

## ScratchBird Wire Protocol (SBWP) Reference

The **canonical** protocol specification is:
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`

Use that file for the full and authoritative details. Below is a compact guide to the essentials to orient new contributors.

### Protocol Basics

- **Name:** ScratchBird Native Wire Protocol (SBWP)
- **Port:** 3092 by spec (conformance uses 5439)
- **Byte order:** little-endian
- **Encoding:** UTF-8 for text fields
- **Compression:** zstd (optional via flags)
- **TLS:** required by spec (alpha builds often run without TLS for local testing)

### Connection Lifecycle (Summary)

1. TCP connect
2. TLS handshake (mandatory in spec)
3. STARTUP message
4. AUTH negotiation (AUTH_REQUEST/AUTH_RESPONSE)
5. AUTH_OK returns `attachment_id` + `txn_id`
6. READY state; every request must include `attachment_id` + `txn_id`

### Message Header (40 bytes)

Each message starts with a fixed-size header:
- magic: `0x53425750` ("SBWP")
- version_major, version_minor
- msg_type, flags
- payload length
- sequence
- attachment_id (UUID, zero before AUTH_OK)
- txn_id (uint64, zero before AUTH_OK)

### Common Message Types (High-Level)

Client → Server:
- `STARTUP`, `AUTH_RESPONSE`, `QUERY`, `PARSE`, `BIND`, `DESCRIBE`, `EXECUTE`, `CLOSE`, `SYNC`
- `COPY_DATA`, `COPY_DONE`, `COPY_FAIL`
- `TXN_BEGIN`, `TXN_COMMIT`, `TXN_ROLLBACK` (+ savepoints)
- `CANCEL`, `TERMINATE`, `PING`
- `SBLR_EXECUTE` (bytecode path)
- `SUBSCRIBE`/`UNSUBSCRIBE` (notifications)

Server → Client:
- `AUTH_REQUEST`, `AUTH_OK`, `READY`
- `ROW_DESCRIPTION`, `DATA_ROW`, `COMMAND_COMPLETE`
- `ERROR`, `NOTICE`
- `PARSE_COMPLETE`, `BIND_COMPLETE`, `PORTAL_SUSPENDED`, `CLOSE_COMPLETE`

### Query & Result Flow (Simplified)

- Simple path: `QUERY` → `ROW_DESCRIPTION` → `DATA_ROW*` → `COMMAND_COMPLETE` → `READY`
- Prepared path: `PARSE` → `PARSE_COMPLETE` → `BIND` → `BIND_COMPLETE` → `EXECUTE`
- Result metadata is in `ROW_DESCRIPTION` and row values stream as `DATA_ROW` frames.

### Type Serialization

- Each column carries a type OID in `ROW_DESCRIPTION`.
- Values are length-prefixed and encoded per native type rules (see spec section 15).

### Errors

- Fatal/non-fatal conditions are conveyed via `ERROR` or `NOTICE` messages.
- For auth failures, the server returns `AUTH_REQUEST` with a failure reason or `ERROR`.

For full detail (including exact field layouts, message structures, and type encodings), use:
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
