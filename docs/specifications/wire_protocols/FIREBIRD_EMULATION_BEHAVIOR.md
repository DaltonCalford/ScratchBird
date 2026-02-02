# Firebird Emulation Protocol Behavior Specification

Status: Draft (Target). This document defines the server-side wire protocol
behavior for the Firebird emulation listener. It is intended to be compatible
with Firebird 5.0 for the covered features.

References:
- firebird_wire_protocol.md
- parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md
- https://raw.githubusercontent.com/FirebirdSQL/firebird/master/src/remote/protocol.h

## 1. Startup/Handshake and Authentication

### 1.1 Protocol Negotiation
- Accept TCP connections on port 3050 (or configured port).
- Expect op_connect / op_accept handshake.
- Negotiate protocol version (10-17). Default target: 17 (Firebird 5.0).

### 1.2 Attach
- Client sends op_attach with DPB.
- Required DPB items:
  - isc_dpb_user_name
  - isc_dpb_password or auth plugin data
  - isc_dpb_auth_plugin_list (optional)
- Server responds with op_accept/op_accept_data/op_cond_accept depending on
  auth continuation.

### 1.3 Authentication
- Prefer SRP (Srp, Srp256) when available.
- Support legacy auth for older clients (protocol 10).
- Use op_cont_auth for multi-step auth continuation.

## 2. Capability Defaults

### 2.1 Protocol Version
- Default target: 17 (Firebird 5.0) or configured override.
- If client requests unsupported protocol, respond with op_reject.

### 2.2 Feature Flags
- Advertise batch support only if implemented (protocol 17+).
- Event support via op_que_events/op_event (if enabled).

## 3. Error/Status Mapping

### 3.1 Status Vector
- All errors returned as Firebird status vector in op_response.
- Map engine errors to GDS codes.

### 3.2 Warnings
- Include warnings in status vector with error code 0 followed by warning codes.

## 4. Metadata and Result Shapes

### 4.1 Prepare/Execute
- Use op_allocate_statement, op_prepare_statement, op_execute/op_execute2.
- Return statement metadata in P_SQLDATA.

### 4.2 Fetch
- op_fetch/op_fetch_response returns row buffers.
- NULL handling follows Firebird SQLDA semantics.

### 4.3 Blob
- Use op_open_blob/op_get_segment/op_put_segment.

## 5. Version-Specific Deltas

### 5.1 Firebird 3.0
- SRP authentication is available.
- Protocol 13 behavior by default.

### 5.2 Firebird 4.0
- Extended decimal and data type support; ensure type mapping correctness.

### 5.3 Firebird 5.0
- Protocol 16/17 with batch operations; advertise only when supported.

## 6. Test Vectors

### 6.1 Attach + Auth
- Client sends op_attach with SRP plugin list.
- Server responds with op_cond_accept.
- Client continues with op_cont_auth until success.

### 6.2 Simple Query
- Allocate statement, prepare, execute, fetch rows, free statement.

### 6.3 Cancel
- Client sends op_cancel on active request.
- Server returns op_response with error code indicating cancelled.
