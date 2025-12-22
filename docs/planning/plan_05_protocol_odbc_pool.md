# Plan 05 - Protocols, ODBC, Connection Pool

## Scope
Complete network protocol adapters, ODBC driver, and connection pool implementation, aligned with full native client compatibility for emulated engines and ScratchBird's always-in-transaction model. Include attachment multiplexing support on the native protocol.

## Priority
P1 (required for external connectivity).

## References
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
- `docs/specifications/ODBC_DRIVER_SPECIFICATION.md`
- `docs/specifications/CONNECTION_POOLING_SPECIFICATION.md`
- `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- `docs/findings/firebird_wire_protocol_gaps.md`
- `docs/findings/mysql_wire_protocol_gaps.md`
- `docs/findings/postgresql_wire_protocol_gaps.md`
- `docs/planning/plan_16_attachment_transaction_model.md`

## Order of Implementation
1) Native/PostgreSQL/MySQL/Firebird protocol gaps (auth, cancel, COPY, describe).
2) Native protocol attachment multiplexing (ScratchBird only).
3) ODBC core execution and catalog queries.
4) Connection pool lifecycle and reset validation (always-in-transaction).

## Concrete Code Touchpoints (Exact Files + Functions)
- Protocol adapters:
  - `src/protocol/adapters/native_adapter.cpp`:
    - `NativeAdapter::handleAuthRequest()` (TODO auth)
    - `NativeAdapter::handleQueryCancel()` (TODO cancel)
    - `NativeAdapter::handleDescribe()` (TODO describe)
  - `src/protocol/adapters/mysql_adapter.cpp`:
    - `MySqlAdapter::processHandshakeResponse()` (TODO password validation)
    - `MySqlAdapter::handleComInitDb()` (TODO DB exists check)
  - `src/protocol/adapters/postgresql_adapter.cpp`:
    - `PostgresqlAdapter::handleCancelRequest()` (TODO)
    - `PostgresqlAdapter::handleAuthentication()` (TODO password validation)
    - `PostgresqlAdapter::handleCopyInData()` / `handleCopyDone()` / `handleCopyFail()` (TODO)
    - `PostgresqlAdapter::computeMd5Hash()` (TODO)
  - `src/protocol/adapters/firebird_adapter.cpp`:
    - TODO `op_drop_database` handling
- ODBC:
  - `src/odbc/odbc_handles.cpp`:
    - `OdbcConnection::connect()` (TODO actual connection)
    - `OdbcStatement::executeDirect()` / `execute()` (TODO wire execution)
    - `OdbcStatement::prepare()` / `executePrepared()` (TODO)
    - `OdbcStatement::cancel()` (TODO)
    - Catalog functions near `SQLTables/SQLColumns/SQLGetTypeInfo` (TODO)
  - `src/odbc/odbc_driver.cpp`: pooling config stub
- Connection pool:
  - `src/core/connection_pool.cpp`:
    - Add `resetConnection(ConnectionContext*)` and call in `release()`.
  - `include/scratchbird/pool/connection_pool.h`:
    - Add config flags for reset actions.
  - `src/core/connection_context.cpp`:
    - Add attachment-aware reset helpers.
  - `src/server/server_session.cpp`:
    - Route queries by attachment_id (native protocol only).

## Implementation Tasks
- Implement native adapter authentication, cancel, and describe.
- Implement PostgreSQL adapter cancel, COPY IN, MD5/SCRAM auth; align with `docs/findings/postgresql_wire_protocol_gaps.md`.
- Implement MySQL adapter password validation and DB existence checks; align with `docs/findings/mysql_wire_protocol_gaps.md`.
- Implement Firebird adapter authentication, result parsing, and DB-drop handling; align with `docs/findings/firebird_wire_protocol_gaps.md`.
- Ensure emulated replication commands are rejected or deferred with clear errors (no replication support in Alpha).
- Implement ODBC connect/execute/prepare/cancel and catalog queries.
- Implement connection pool reset: rollback transaction, reset search_path, reset role, reset session vars, clear cached prepared statements.
- Implement attachment multiplexing for ScratchBird native protocol:
  - Allow multiple attachments per TCP connection.
  - Route each request to the correct attachment context.
  - Ensure each attachment has its own transaction lifecycle.

## Required Data/Schema Changes
- None (protocol/client layer changes only).

## Completion Checklist (Developer)
- [ ] All adapters handle authentication and cancel paths per native client expectations.
- [ ] PostgreSQL COPY IN works end-to-end.
- [ ] MySQL and Firebird protocol edge cases match documented gaps.
- [ ] Emulated replication commands are rejected/deferred with explicit errors.
- [ ] ODBC can connect, execute, prepare, and retrieve metadata.
- [ ] Connection pool manages lifecycle, reset, and validation behavior.
- [ ] Native protocol supports multi-attachment routing with correct transaction isolation per attachment.

## Completion Checklist (Auditor)
- [ ] Protocol integration tests pass for each adapter.
- [ ] ODBC tests cover DDL/DML and catalog queries.
- [ ] Connection pool passes leak and concurrency tests.

## Testing Requirements
- Protocol integration tests for auth/cancel/COPY.
- ODBC end-to-end tests.
- Pool stress tests with timeouts and resets.
- Native protocol tests for attachment multiplexing (two attachments, isolated transactions).
- Tests should live under `tests/unit` unless `tests/CMakeLists.txt` is updated to re-enable integration tests.

## Acceptance Criteria
- Native clients connect without protocol negotiation errors.
- ODBC executes DDL/DML and catalog queries end-to-end.
- Connection pool passes concurrency and reset tests.
- Multi-attachment requests on native protocol are routed correctly and do not leak transaction state.

## Implementation Notes (Concrete)
- **Adapters**: implement auth, cancel, describe, and COPY/large payload flows per protocol.
- **MySQL**: support auth plugins `mysql_native_password` and `caching_sha2_password`.
- **PostgreSQL**: support MD5 + SCRAM; implement CancelRequest + BackendKeyData.
- **Firebird**: implement DPB/TPB handling and SQLDA/XSQLDA parsing.
- **ODBC**: implement connection, execution, prepared statements, and catalog metadata queries.
- **Pool**: enforce reset of session variables, search path, and transaction state.
- **Always-in-transaction**: after any COMMIT/ROLLBACK or pool reset, immediately start a new default transaction (per user/role default settings). There is never a "no transaction" state.
- **Autocommit** (client-layer): autocommit ON means each statement runs in its own transaction; the adapter must COMMIT after each statement and immediately start a new transaction for the next statement.

## Full Implementation Detail (No Ambiguity)
### 1) Native Adapter
- `NativeAdapter::handleAuthRequest()`:
  - Validate credentials against `CatalogManager` user records and `AuthKey` rules (Plan 03).
- `NativeAdapter::handleQueryCancel()`:
  - Implement cancel request to set termination flag in `ConnectionContext`.
- `NativeAdapter::handleDescribe()`:
  - Use prepared statement metadata from `ConnectionContext::PreparedStatement` and emit columns.

### 2) MySQL Adapter
- `processHandshakeResponse()`:
  - Validate auth plugin + password hash per plugin spec.
  - If invalid, return ERR packet with SQLSTATE `28000`.
- `handleComInitDb()`:
  - Verify database exists or return `ER_BAD_DB_ERROR`.
- `Capability negotiation`:
  - Correctly handle CLIENT_PROTOCOL_41, CLIENT_PLUGIN_AUTH, CLIENT_DEPRECATE_EOF, CLIENT_SESSION_TRACK.

### 3) PostgreSQL Adapter
- `handleCancelRequest()`:
  - Validate backend_pid_ and backend_secret_key_ and trigger termination for the active connection.
- `MD5/SCRAM auth`:
  - Implement proper hash computations and SASL flows.
- `COPY IN`:
  - Parse CopyData/CopyDone/CopyFail frames and stream into executor.

### 4) Firebird Adapter
- Implement DPB/TPB parsing per spec and map to transaction settings.
- Implement DROP DATABASE op handling (TODO in `firebird_adapter.cpp`).
- Emit correct status vectors with isc + SQLSTATE codes.

### 5) ODBC Driver
- `OdbcConnection::connect()`:
  - Parse DSN/connection string, create `client::Connection`, and connect to server.
- `OdbcStatement::executeDirect()`:
  - Use protocol adapter to execute query and populate `OdbcResultSet`.
- `OdbcStatement::prepare()/executePrepared()`:
  - Use prepared statement cache on `ConnectionContext`.
- Catalog functions:
  - Implement `SQLTables`, `SQLColumns`, `SQLGetTypeInfo` using catalog queries.

### 6) Connection Pool Reset
- Add `resetConnection(ConnectionContext*)`:
  - Rollback active transaction if any.
  - Immediately start a new default transaction (always-in-transaction contract).
  - Reset search_path to default.
  - Reset current role/user to session defaults.
  - Clear prepared statement cache.
- Call `resetConnection()` in `ConnectionPool::release()` for non-invalidated returns.

### 7) Native Protocol Attachment Multiplexing (ScratchBird Only)
- Add attachment routing fields to the native protocol header (see Plan 16 for protocol layout).
- When a connection is created:
  - Create a default attachment, return its attachment_id, and immediately start a default transaction.
- Support explicit attachment lifecycle messages:
  - CREATE ATTACHMENT -> returns attachment_id.
  - DETACH ATTACHMENT -> rolls back its current transaction and destroys the attachment context.
- After AUTH_OK, every native message must carry non-zero attachment_id and txn_id in the header (ATTACH_CREATE uses the caller's attachment/transaction).
- Enforce: transactions are strictly scoped to one attachment and cannot be shared across attachments.

## Concrete Protocol Coverage (Minimum)
- **MySQL**: COM_QUERY, COM_STMT_PREPARE, COM_STMT_EXECUTE, COM_STMT_CLOSE, COM_PING, COM_INIT_DB, COM_CHANGE_USER, COM_RESET_CONNECTION.
- **PostgreSQL**: StartupMessage, SSLRequest, AuthenticationRequest, ParameterStatus, ReadyForQuery, Query, Parse, Bind, Execute, Describe, Sync, CancelRequest.
- **Firebird**: op_connect, op_accept, op_attach, op_allocate_statement, op_prepare_statement, op_execute, op_fetch, op_transaction, op_commit, op_rollback, op_info_sql.

## Concrete Test Cases
- **PostgreSQL**: libpq connects, cancels, and executes simple + extended queries.
- **MySQL**: mysql CLI authenticates with both auth plugins.
- **Firebird**: isql attaches, prepares, executes, and fetches.
- **ODBC**: standard ODBC tests for catalog queries and prepared statements.
- **Pool**: acquire/release across many threads; verify reset on reuse.
- **Native protocol**: open two attachments on one connection, start independent transactions, verify isolation and independent commit/rollback.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
