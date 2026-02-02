# Plan: Remote Engine UDR Connectors (ScratchBird/Firebird/MySQL/PostgreSQL)

Status: Draft
Last Updated: 2026-02-03
Owner: Core

## Scope
Implement production-grade UDR connectors for remote ScratchBird, Firebird,
MySQL, and PostgreSQL using native wire protocols. The plan builds on:
- docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md
- docs/specifications/Alpha Phase 2/11a-Connection-Pool-Implementation.md
- docs/specifications/Alpha Phase 2/11b-11i client implementation specs
- docs/specifications/udr_connectors/UDR_CONNECTOR_BASELINE.md
- docs/specifications/wire_protocols/*
- docs/specifications/wire_protocols/*_EMULATION_BEHAVIOR.md

## Non-goals
- JDBC/ODBC connectors (separate workstream)
- Cross-database query optimizer improvements

## Milestones
M0: Shared UDR connector framework ready
M1: PostgreSQL UDR complete
M2: MySQL UDR complete
M3: Firebird UDR complete
M4: ScratchBird UDR complete
M5: Conformance and integration tests green

## Workstreams

### WS-0: Shared UDR Connector Framework
- [ ] Finalize UDR connector manifest schema and signing verification
- [ ] Implement connection pool per 11a-Connection-Pool-Implementation.md
- [ ] Implement server/user mapping options handling
- [ ] Implement sys.remote_exec/sys.remote_query/sys.remote_call
- [ ] Implement common error mapping to ScratchBird SQLSTATE
- [ ] Implement common metrics for connection pools and protocol adapters

Deliverables:
- ConnectionPool module (thread-safe, health checks)
- UDR connector base class with lifecycle hooks
- sys.* passthrough procedures

### WS-1: PostgreSQL UDR
- [ ] Implement Postgres protocol client (v3) startup/auth (SCRAM, MD5)
- [ ] TLS support (SSLRequest, verify-ca/full)
- [ ] Simple query + extended query (Parse/Bind/Execute/Sync)
- [ ] Portal paging and cursor support
- [ ] COPY text streaming (binary optional)
- [ ] CancelRequest support
- [ ] Schema introspection queries (pg_catalog, information_schema)
- [ ] Type mapping (incl. arrays via POSTGRESQL_ARRAY_TYPE_SPEC.md)
- [ ] Error mapping (SQLSTATE)

Deliverables:
- postgresql_udr module + manifest
- Conformance tests (auth, prepared statements, paging, COPY)

### WS-2: MySQL UDR
- [ ] Handshake V10, capability negotiation, server status
- [ ] TLS support (CLIENT_SSL)
- [ ] Auth plugins (caching_sha2_password + mysql_native_password)
- [ ] COM_QUERY + text protocol results
- [ ] Prepared statements (COM_STMT_PREPARE/EXECUTE/FETCH)
- [ ] Cursor paging via COM_STMT_FETCH
- [ ] Cancellation (KILL QUERY)
- [ ] Schema introspection (information_schema)
- [ ] Type mapping + charset/collation handling
- [ ] Error mapping (SQLSTATE + error codes)

Deliverables:
- mysql_udr module + manifest
- Conformance tests (auth, prepared statements, paging)

### WS-3: Firebird UDR
- [ ] Protocol negotiation (op_connect/op_accept)
- [ ] Auth validation (SRP + legacy)
- [ ] Full XDR framing/packet parsing
- [ ] DSQL flow (allocate, prepare, execute, fetch)
- [ ] Blob streaming operations
- [ ] Events (op_que_events/op_event) if required
- [ ] Cancel op
- [ ] Schema introspection (RDB$ system tables)
- [ ] BLR parser expansion for full datatype set
- [ ] Error mapping (status vector to SQLSTATE)

Deliverables:
- firebird_udr module + manifest
- Conformance tests (auth, execute, fetch, blob)

### WS-4: ScratchBird UDR
- [ ] SBWP client implementation (startup/auth/handshake)
- [ ] TLS required by default
- [ ] Prepared statements + paging
- [ ] COPY streaming
- [ ] Cancel/interrupt support
- [ ] Schema introspection (sys.* catalogs)
- [ ] Type mapping (1:1)

Deliverables:
- scratchbird_udr module + manifest
- Conformance tests (auth, prepared statements, paging, COPY)

### WS-5: Conformance and Integration
- [ ] sbdriver-conformance adapter(s) for UDR testing
- [ ] Feature manifest entries for each connector
- [ ] End-to-end remote exec/query tests for each protocol
- [ ] Negative tests (auth failure, TLS mismatch, permission denial)

## Exit Criteria
- All four connectors implement required protocol capabilities
- Conformance tests pass for each connector
- sys.remote_* APIs validated for DDL/DML/PSQL passthrough
- Documentation updated with runbooks and configuration examples

