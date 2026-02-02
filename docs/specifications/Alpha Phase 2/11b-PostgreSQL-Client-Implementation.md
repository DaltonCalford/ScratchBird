# PostgreSQL Client Implementation

PostgreSQL wire protocol client adapter for Remote Database UDR.

See: 11-Remote-Database-UDR-Specification.md and wire_protocols/postgresql_wire_protocol.md

## 1. Scope
- Protocol: PostgreSQL frontend/backend protocol v3.0
- Supported versions: PostgreSQL 9.6 through 17
- Transport: TCP with optional TLS
- No libpq usage; ScratchBird implements protocol directly

## 2. Connection and Authentication

### 2.1 Startup Sequence
1. TCP connect
2. Optional SSLRequest (int32 len=8, int32 code=80877103)
3. If server replies 'S', start TLS; if 'N', continue in cleartext
4. Send StartupMessage (protocol 3.0) with parameters:
   - user, database, client_encoding, application_name, search_path
5. Process:
   - AuthenticationOk or Authentication* messages
   - ParameterStatus entries
   - BackendKeyData (pid + secret key)
   - ReadyForQuery

### 2.2 Authentication Methods
- AuthenticationCleartextPassword
- AuthenticationMD5Password
- AuthenticationSASL / SASLContinue / SASLFinal (SCRAM-SHA-256)
- AuthenticationGSS/SSPI are not required for Alpha (optional)

### 2.3 TLS
- sslmode: disable/allow/prefer/require/verify-ca/verify-full
- verify-full requires hostname verification

## 3. Command Execution

### 3.1 Simple Query
Client sends Query ('Q') and receives:
- RowDescription ('T')
- DataRow ('D') repeated
- CommandComplete ('C')
- ReadyForQuery ('Z')

### 3.2 Extended Query (Prepared)
Use Parse/Bind/Describe/Execute/Sync:
- Parse ('P') creates a prepared statement (by name)
- Bind ('B') binds parameters to a portal
- Execute ('E') executes the portal (supports maxRows for paging)
- PortalSuspended ('s') indicates more rows remain
- Sync ('S') ends the extended query sequence

### 3.3 Portal Paging
To support driver paging:
- Use Execute with maxRows
- On PortalSuspended, client can send additional Execute for same portal

### 3.4 COPY
Handle CopyInResponse / CopyOutResponse / CopyBothResponse:
- COPY FROM STDIN: stream CopyData ('d') until CopyDone ('c')
- COPY TO STDOUT: receive CopyData until CopyDone
- Handle CopyFail ('f')
- Support text format; binary COPY is optional for Alpha

## 4. Cancellation
- Use CancelRequest (separate TCP connection) with backend PID + secret key
- After cancel, expect ErrorResponse and ReadyForQuery on target connection

## 5. Transactions
- Begin: "BEGIN"
- Commit: "COMMIT"
- Rollback: "ROLLBACK"
- Savepoint: "SAVEPOINT name" / "ROLLBACK TO SAVEPOINT name"

Connections must be in ReadyForQuery before reuse.

## 6. Error Mapping
- ErrorResponse includes SQLSTATE; map to ScratchBird error catalog
- Severity INFO/NOTICE/WARNING should not fail the connection

## 7. Type Mapping (Minimum)
Use pg_type OIDs and modifiers. Map to ScratchBird types:
- int2 -> INT16
- int4 -> INT32
- int8 -> INT64
- float4 -> FLOAT
- float8 -> DOUBLE
- numeric -> DECIMAL (precision/scale from typmod)
- bool -> BOOL
- text/varchar/char/name -> STRING (charset UTF-8)
- bytea -> BYTES
- date -> DATE
- timestamp/timestamptz -> TIMESTAMP/TIMESTAMPTZ
- uuid -> UUID
- json/jsonb -> JSON
- array -> ARRAY (see types/POSTGRESQL_ARRAY_TYPE_SPEC.md)

Unsupported types should be returned as STRING with a warning unless
"strict_types" option is enabled.

## 8. Schema Introspection
Use pg_catalog and information_schema (see 03-POSTGRESQL_ADAPTER.md for
reference queries). Introspection must capture:
- schemas, tables, views, matviews
- columns with defaults and nullability
- constraints (pk/unique/fk/check)
- indexes
- routines and triggers
- sequences

## 9. Required Capabilities
- prepared statements
- portal paging
- COPY streaming
- cancellation
- schema introspection

## 10. References
- https://www.postgresql.org/docs/current/protocol.html
- https://www.postgresql.org/docs/current/protocol-message-formats.html
- https://www.postgresql.org/docs/current/sql-copy.html

