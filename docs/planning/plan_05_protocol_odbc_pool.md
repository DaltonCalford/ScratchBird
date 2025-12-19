# Plan 05 - Protocols, ODBC, Connection Pool

## Scope
Complete network protocol adapters, ODBC driver, and connection pool implementation, aligned with full native client compatibility for emulated engines.

## Priority
P1 (required for external connectivity).

## References
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
- `docs/specifications/ODBC_DRIVER_SPECIFICATION.md`
- `docs/specifications/CONNECTION_POOLING_SPECIFICATION.md`
- `docs/findings/firebird_wire_protocol_gaps.md`
- `docs/findings/mysql_wire_protocol_gaps.md`
- `docs/findings/postgresql_wire_protocol_gaps.md`

## Order of Implementation
1) Native/PostgreSQL/MySQL protocol gaps (auth, cancel, COPY, describe).
2) ODBC core execution and catalog queries.
3) Connection pool lifecycle and validation.

## Implementation Tasks
- Implement native adapter authentication, cancel, and describe.
- Implement PostgreSQL adapter cancel, COPY IN, MD5/SCRAM auth; align with `docs/findings/postgresql_wire_protocol_gaps.md`.
- Implement MySQL adapter password validation and DB existence checks; align with `docs/findings/mysql_wire_protocol_gaps.md`.
- Implement Firebird adapter authentication and result parsing; align with `docs/findings/firebird_wire_protocol_gaps.md`.
- Ensure emulated replication commands are rejected or deferred with clear errors (no replication support in Alpha).

## Required Data/Schema Changes
- None (protocol/client layer changes only).
- Implement ODBC connect/execute/prepare/cancel and catalog queries.
- Implement connection pool: connect, exec, validation, reset, caching.

## Completion Checklist (Developer)
- [ ] All adapters handle authentication and cancel paths per native client expectations.
- [ ] PostgreSQL COPY IN works end-to-end.
- [ ] MySQL and Firebird protocol edge cases match documented gaps.
- [ ] Emulated replication commands are rejected/deferred with explicit errors.
- [ ] ODBC can connect, execute, prepare, and retrieve metadata.
- [ ] Connection pool manages lifecycle and reset behavior.

## Completion Checklist (Auditor)
- [ ] Protocol integration tests pass for each adapter.
- [ ] ODBC tests cover DDL/DML and catalog queries.
- [ ] Connection pool passes leak and concurrency tests.

## Testing Requirements
- Protocol integration tests for auth/cancel/COPY.
- ODBC end-to-end tests.
- Pool stress tests with timeouts and resets.

## Acceptance Criteria
- Native clients connect without protocol negotiation errors.
- ODBC executes DDL/DML and catalog queries end-to-end.
- Connection pool passes concurrency and reset tests.

## Implementation Notes (Concrete)
- **Adapters**: implement auth, cancel, describe, and COPY/large payload flows per protocol.
- **MySQL**: support auth plugins `mysql_native_password` and `caching_sha2_password`.
- **PostgreSQL**: support MD5 + SCRAM; implement CancelRequest + BackendKeyData.
- **Firebird**: implement DPB/TPB handling and SQLDA/XSQLDA parsing.
- **ODBC**: implement connection, execution, prepared statements, and catalog metadata queries.
- **Pool**: enforce reset of session variables, search path, and transaction state.

## Expanded API/Schema Details
- **Protocol adapters**:
  - `src/protocol/adapters/native_adapter.cpp` (auth, cancel, describe).
  - `src/protocol/adapters/postgresql_adapter.cpp` (MD5/SCRAM, COPY IN, CancelRequest).
  - `src/protocol/adapters/mysql_adapter.cpp` (auth plugins, DB selection).
  - `src/protocol/adapters/firebird_adapter.cpp` (DPB/TPB, SQLDA/XSQLDA).
- **ODBC**:
  - Implement in `src/odbc/odbc_handles.cpp`: connect, exec, prepare, cancel, catalog queries.
  - Ensure error codes map to ODBC SQLSTATE values.
- **Connection pool**:
  - `src/pool/connection_pool.cpp` must implement create/close/validate/reset/cache behaviors.

## Full Implementation Detail (No Ambiguity)
- **PostgreSQL adapter**:
  - Implement BackendKeyData and CancelRequest support.
  - Support MD5 and SCRAM-SHA-256 with correct SASL flow.
- **MySQL adapter**:
  - Implement `mysql_native_password` and `caching_sha2_password` with SSL/RSA fallback.
  - Support capability negotiation for CLIENT_DEPRECATE_EOF, CLIENT_SESSION_TRACK.
- **Firebird adapter**:
  - Implement DPB/TPB parsing per spec; implement SQLDA/XSQLDA metadata.
  - Ensure opcodes align with wire protocol version 10–13.
- **ODBC**:
  - Map ODBC SQLSTATEs to native errors; implement catalog functions (tables, columns, indexes).
- **Connection pool reset**:
  - Must rollback active transaction, reset search_path, reset session variables, and clear caches.

## Concrete Protocol Coverage (Minimum)
- **MySQL**: COM_QUERY, COM_STMT_PREPARE, COM_STMT_EXECUTE, COM_STMT_CLOSE, COM_PING, COM_INIT_DB, COM_CHANGE_USER, COM_RESET_CONNECTION.
- **PostgreSQL**: StartupMessage, SSLRequest, AuthenticationRequest, ParameterStatus, ReadyForQuery, Query, Parse, Bind, Execute, Describe, Sync, CancelRequest.
- **Firebird**: op_connect, op_accept, op_attach, op_allocate_statement, op_prepare_statement, op_execute, op_fetch, op_transaction, op_commit, op_rollback, op_info_sql.

## Concrete Test Cases
- **PostgreSQL**: libpq connects, cancels, and executes simple + extended queries.
- **MySQL**: mysql CLI authenticates with both auth plugins.
- **Firebird**: isql attaches, prepares, executes, and fetches.
- **ODBC**: standard ODBC tests for catalog queries and prepared statements.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
