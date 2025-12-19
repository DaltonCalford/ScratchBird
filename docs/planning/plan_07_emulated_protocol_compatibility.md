# Plan 07 - Emulated Protocol Compatibility (Firebird/MySQL/PostgreSQL)

## Scope
Deliver full native client compatibility for Firebird/MySQL/PostgreSQL over network protocols. Replication is not required for emulated engines; legacy migration is via UDR-based passthrough and one-way migration into ScratchBird. Use the wire protocol gap documents as requirements.

## Priority
P0 (Alpha requirement).

## References
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/specifications/wire_protocols/mysql_wire_protocol.md`
- `docs/specifications/wire_protocols/postgresql_wire_protocol.md`
- `docs/findings/firebird_wire_protocol_gaps.md`
- `docs/findings/mysql_wire_protocol_gaps.md`
- `docs/findings/postgresql_wire_protocol_gaps.md`
- `docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
## Order of Implementation
1) Protocol parity per engine (handshake, auth, session state, errors, result sets).
2) Statement/transaction behavior parity.
3) Edge-case message flows and feature negotiation.
4) Passthrough/migration UDR integration.

## Implementation Tasks
- Implement complete protocol flows for each engine’s native clients (no omissions in wire behavior).
- Ensure all required authentication mechanisms are supported for each engine.
- Implement full message/error/notice semantics to match client expectations.
- Explicitly disable emulated replication features (binlog/streaming) and document as deferred.
- Integrate passthrough/migration UDRs to support legacy DB connectivity and one-way migration.
- Close all items listed in:
  - `docs/findings/firebird_wire_protocol_gaps.md`
  - `docs/findings/mysql_wire_protocol_gaps.md`
  - `docs/findings/postgresql_wire_protocol_gaps.md`

## Required Data/Schema Changes
- None (protocol layer).

## Completion Checklist (Developer)
- [ ] Firebird/MySQL/PostgreSQL protocol flows fully implemented per spec.
- [ ] Any native client can connect without protocol feature regressions.
- [ ] All protocol gap items are closed per engine.
- [ ] Replication commands are rejected or stubbed with clear errors.
- [ ] Passthrough/migration UDRs exist and are wired into the server.

## Completion Checklist (Auditor)
- [ ] Compatibility tests verify native clients for each engine.
- [ ] Wire protocol traces match expected behavior for handshake/auth/query.
- [ ] Protocol gap documents show no open items.
- [ ] Replication requests are explicitly unsupported and logged.
- [ ] Passthrough/migration paths operate without protocol deviations.

## Testing Requirements
- Native client integration tests for each engine.
- Protocol fuzzing for handshake/auth/edge cases.
- Passthrough/migration end-to-end tests.

## Acceptance Criteria
- Any native client connects without protocol deviation.
- All wire-protocol gap items are closed and validated via tests.

## Implementation Notes (Concrete)
- **Protocol parity**: implement every message type required by native clients (handshake, auth, query, error/notice, cancel).
- **Feature negotiation**: support capability flags and fallbacks exactly as native servers do.
- **Replication**: explicitly reject emulated replication commands with clear errors.
- **Passthrough**: UDR-based passthrough must preserve wire-level semantics.

## Expanded API/Schema Details
- **Firebird**: support op_connect/op_attach/op_allocate_statement/op_prepare/op_execute/op_fetch/op_transaction/op_commit; DPB/TPB parsing must match spec.
- **MySQL**: support COM_QUERY, COM_STMT_* flows, capability negotiation, OK/ERR packet variants.
- **PostgreSQL**: support StartupMessage, AuthenticationRequest, ParameterStatus, ReadyForQuery, Simple/Extended query flows, CancelRequest.

## Full Implementation Detail (No Ambiguity)
- **Firebird**:
  - Support protocol versions 10–13 with version-specific opcodes.
  - Implement SRP and legacy auth as required by native clients.
  - Ensure status vector is populated with correct isc codes and SQLSTATE.
- **MySQL**:
  - Implement full capability flag negotiation (CLIENT_PROTOCOL_41, CLIENT_SECURE_CONNECTION, CLIENT_PLUGIN_AUTH, CLIENT_DEPRECATE_EOF).
  - Return correct OK/ERR packets with status flags and warnings.
- **PostgreSQL**:
  - Send ParameterStatus for required parameters (server_version, client_encoding, DateStyle, TimeZone, integer_datetimes).
  - Implement ErrorResponse with proper severity codes and SQLSTATE.

## Concrete Compatibility Requirements
- **PostgreSQL ParameterStatus**: must include at least `server_version`, `client_encoding`, `DateStyle`, `TimeZone`, `integer_datetimes`, `standard_conforming_strings`.
- **MySQL OK packet**: include status_flags and warnings; support CLIENT_SESSION_TRACK if negotiated.
- **Firebird status vector**: include `isc_arg_gds` and `isc_arg_sql_state` when errors occur.

## Concrete Test Cases
- Firebird isql connects and executes DDL/DML without protocol errors.
- MySQL client connects with caching_sha2_password and executes prepared statements.
- psql connects, runs simple and extended queries, and issues cancel requests.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
