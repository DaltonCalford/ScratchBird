# PostgreSQL Emulation Protocol Behavior Specification

Status: Draft (Target). This document defines the server-side wire protocol
behavior for the PostgreSQL emulation listener. It is intended to be
byte-compatible with PostgreSQL 9.6-17 for the covered features.

References:
- postgresql_wire_protocol.md
- parser/POSTGRESQL_PARSER_SPECIFICATION.md
- https://www.postgresql.org/docs/current/protocol.html

## 1. Startup/Handshake and Authentication

### 1.1 Connection Startup
- Accept TCP connections on the PostgreSQL listener port.
- If the client sends SSLRequest or GSSENCRequest:
  - Respond with a single byte 'S' only if TLS/GSSENC is enabled for the
    emulated listener; otherwise respond 'N'.
  - If 'S'/'G' is sent, complete TLS/GSSENC and expect the StartupMessage on
    the encrypted channel.
  - Read exactly 1 byte for SSL/GSSENC response and do not send ErrorResponse
    to SSL/GSSENC requests.

### 1.2 StartupMessage Handling
- Require `user` parameter; if missing, send FATAL ErrorResponse with SQLSTATE
  28000 and close connection.
- `database` defaults to `user` when omitted.
- `client_encoding`: accept UTF8/UTF-8 only; if unsupported value, send FATAL
  ErrorResponse with SQLSTATE 22021.
- Unknown parameters are accepted and ignored unless explicitly configured.

### 1.3 Authentication Methods
- Default authentication order: SCRAM-SHA-256, then MD5 (legacy).
- If the configured user supports only one method, advertise only that method.
- SCRAM flow must follow RFC 5802 and match PostgreSQL SASL framing:
  - AuthenticationSASL -> AuthenticationSASLContinue -> AuthenticationSASLFinal.
- On auth success, send:
  - AuthenticationOk
  - ParameterStatus (at least: server_version, server_encoding, client_encoding,
    DateStyle, TimeZone)
  - BackendKeyData (PID + secret key)
  - ReadyForQuery

### 1.4 Cancellation
- Must accept CancelRequest on a new connection using PID + secret key.
- If cancel succeeds, the target connection emits ErrorResponse and ReadyForQuery.

## 2. Capability and Defaults

### 2.1 ParameterStatus Keys (Minimum Set)
- server_version: "16.0" (or configured emulation target)
- server_encoding: "UTF8"
- client_encoding: "UTF8"
- DateStyle: "ISO, MDY"
- TimeZone: "UTC"
- integer_datetimes: "on"
- standard_conforming_strings: "on"

### 2.2 Server Version Target
- Default emulation target: PostgreSQL 16.
- Allow override to 9.6-17 for compatibility testing.

## 3. Error/Notice Response Mapping

### 3.1 ErrorResponse
- Always send SQLSTATE in ErrorResponse.
- Severity values: ERROR, FATAL, PANIC (PANIC not used in emulation).
- Map engine errors to PostgreSQL SQLSTATE codes.

### 3.2 NoticeResponse
- Use for warnings and informational messages.
- Must not abort the current transaction.

## 4. Metadata and Result Shapes

### 4.1 RowDescription
- Provide table OID, column attribute number, type OID, type length, type
  modifier, and format code (0 = text).
- For types without native OIDs, map to TEXT (OID 25) and set format 0.

### 4.2 DataRow
- Use text format for all columns unless binary mode is negotiated explicitly.
- NULL values encoded as length = -1.

### 4.3 CommandComplete Tags
- Follow native tags, e.g.,
  - SELECT n
  - INSERT 0 n
  - UPDATE n
  - DELETE n
  - COPY n

## 5. Version-Specific Deltas

### 5.1 9.6-11
- No `prokind` in pg_proc; emulate where required in system catalogs.
- SCRAM-SHA-256 may be unavailable; default to MD5.

### 5.2 12-17
- SCRAM-SHA-256 preferred.
- `NegotiateProtocolVersion` may be sent if unknown parameters are rejected
  (optional; default behavior is to ignore unknowns).

## 6. Test Vectors

### 6.1 Startup + MD5
- Client sends StartupMessage with user/db.
- Server sends AuthenticationMD5 + salt.
- Client responds with PasswordMessage.
- Server sends AuthenticationOk, ParameterStatus*, BackendKeyData, ReadyForQuery.

### 6.2 Simple Query
- Client sends Query "SELECT 1".
- Server sends RowDescription, DataRow, CommandComplete, ReadyForQuery.

### 6.3 Cancel
- Client starts long query.
- Client sends CancelRequest on separate socket.
- Server sends ErrorResponse (SQLSTATE 57014) then ReadyForQuery.

