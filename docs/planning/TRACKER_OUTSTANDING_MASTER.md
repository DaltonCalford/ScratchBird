# Outstanding Work Tracker (Master)

Status: In Progress
Last Updated: 2026-02-03

This tracker is the **single source of truth** for remaining work. All other
planning docs can be archived after this is in place.

Findings review (2026-02-02): all findings in `docs/findings` are closed or
superseded; no additional gaps beyond the items listed below.

## A) Git Config Key Normalization (PLAN_GIT_CONFIG_KEY_NORMALIZATION.md)

### A1. YAML Parser Updates (src/git/GitConfigParser.cpp)
- [ ] Parse canonical keys: `repo_type`, `repo_url`, `repo_path`, `repo_mode`,
      `repo_branch`, `integration_mode`, `sign_commits`, `commit_template`.
- [ ] Accept legacy aliases (`url`, `branch`, `path`, `mode`, `type`) with
      canonical precedence when both are present.
- [ ] Parse `auto_pull` consistently with auto_commit/auto_push.
- [ ] Update `validate()` to require `repo_url` (allow legacy alias).
- [ ] Update `toYAML()` to emit canonical `repo_*` keys.

### A2. sb_config.ini Support
- [ ] Add INI parsing path for `[git.*]` sections.
- [ ] Map INI sections to the same config structures as YAML.
- [ ] Apply canonical precedence + legacy alias handling for INI inputs.

### A3. Tests + Diagnostics
- [ ] Unit tests for canonical keys (YAML).
- [ ] Unit tests for legacy aliases + precedence (YAML).
- [ ] Unit tests for INI parsing (repository/schema/migrations/envs).
- [ ] Add a config lint/diagnostic message for deprecated keys.

### A4. Docs Sync (post-implementation)
- [ ] Confirm user docs reflect updated parser behavior.
- [ ] Update config examples to use canonical keys.

## B) SBLR Type Opcode Remediation Tests (SBLR_TYPE_OPCODE_REMEDIATION_PLAN.md)

### B1. SBLR Unit Tests
- [ ] Add bytecode round-trip tests for all SBLR type markers.
- [ ] Add typed literal parsing tests for new literal opcodes.

### B2. DDL/DML Coverage
- [ ] Minimal CREATE TABLE + INSERT/SELECT coverage for each new type.

## C) Emulated Engine 1:1 Parity Gaps (docs/findings/EMULATED_ENGINE_1TO1_AUDIT.md)

### C1. PostgreSQL Protocol Adapter Parity
- [ ] Add TLS support for SSLRequest and encrypted StartupMessage (respect ssl_mode).
- [ ] Add GSSENC handling or explicit negotiated disable per spec.
- [ ] Implement SCRAM-PLUS channel binding or block when requested with correct SQLSTATE.
- [ ] Validate MD5 auth path and remove TODO (use full MD5 flow).
- [ ] Align ParameterStatus keys and server_version string with emulation target.

### C2. PostgreSQL Parser Parity
- [ ] Implement JSONPATH in PostgreSQL emulation.
- [ ] Implement array domains.
- [ ] Implement table-level CHECK constraints.
- [ ] Implement CREATE DOMAIN base type support.
- [ ] Implement ALTER TABLE DROP CONSTRAINT.
- [ ] Implement ALTER TABLE ALTER COLUMN SET/DROP DEFAULT/NOT NULL.
- [ ] Implement ALTER TABLE ALTER COLUMN ... USING.
- [ ] Implement TRUNCATE options.
- [ ] Implement JOIN USING support.
- [ ] Implement DEFAULT values in multi-row INSERT.
- [ ] Implement MERGE USING subqueries.

### C3. MySQL Protocol Adapter Parity
- [ ] Implement TLS negotiation (CLIENT_SSL) and encryption path.
- [ ] Implement proper password validation (mysql_native_password + caching_sha2_password).
- [ ] Validate database existence on connection and COM_INIT_DB.
- [ ] Make server capability flags version-aware (5.7 vs 8.0 vs MariaDB).
- [ ] Align server_version string to emulation target.

### C4. MySQL Parser Parity
- [ ] Implement window frame offsets.
- [ ] Implement named windows.
- [ ] Implement DEFAULT values in multi-row INSERT/REPLACE.
- [ ] Implement ALTER TABLE CHANGE COLUMN rename.
- [ ] Implement ALTER TABLE ALTER COLUMN SET/DROP DEFAULT.
- [ ] Implement GRANT/REVOKE ON ALL bytecode support.

### C5. Firebird Protocol Adapter Parity
- [ ] Implement SRP/legacy auth validation (remove accept-any-auth paths).
- [ ] Implement full XDR framing/packet length parsing.
- [ ] Implement additional opcodes (blob ops, events, services, batch, etc.).
- [ ] Expand BLR parser to support full datatype set (beyond short/long/int64/text/varying).
- [ ] Align protocol version negotiation to Firebird 3.0-5.0 targets.

### C6. Firebird Parser Parity
- [ ] Implement ALTER ROLE/USER/MAPPING/SHADOW.
- [ ] Implement DROP USER/MAPPING/SHADOW.
- [ ] Implement ALTER DATABASE options.
- [ ] Implement RECREATE ROLE/USER/MAPPING/SHADOW.
- [ ] Implement ALTER TABLE SET.

## D) Remote Engine UDR Connectors (docs/planning/PLAN_UDR_REMOTE_CONNECTORS.md)

### D0. Shared Connector Framework
- [ ] Finalize UDR connector manifest schema and signing verification.
- [ ] Implement connection pool per 11a-Connection-Pool-Implementation.md.
- [ ] Implement server/user mapping options handling.
- [ ] Implement sys.remote_exec/sys.remote_query/sys.remote_call.
- [ ] Implement common error mapping to ScratchBird SQLSTATE.
- [ ] Implement common metrics for connection pools and protocol adapters.

### D1. PostgreSQL UDR
- [ ] Implement PostgreSQL protocol client (v3) startup/auth (SCRAM/MD5).
- [ ] Implement TLS support (SSLRequest + verify-ca/full).
- [ ] Implement extended query (Parse/Bind/Execute/Sync) + portal paging.
- [ ] Implement COPY text streaming.
- [ ] Implement CancelRequest support.
- [ ] Implement schema introspection (pg_catalog/info_schema).
- [ ] Implement type mapping (including arrays).
- [ ] Implement SQLSTATE error mapping.

### D2. MySQL UDR
- [ ] Implement MySQL Handshake V10 + capability negotiation.
- [ ] Implement TLS support (CLIENT_SSL).
- [ ] Implement auth plugins (caching_sha2_password + mysql_native_password).
- [ ] Implement prepared statements (COM_STMT_PREPARE/EXECUTE/FETCH).
- [ ] Implement cursor paging via COM_STMT_FETCH.
- [ ] Implement cancellation (KILL QUERY).
- [ ] Implement schema introspection (information_schema).
- [ ] Implement type mapping + charset/collation handling.
- [ ] Implement SQLSTATE + error code mapping.

### D3. Firebird UDR
- [ ] Implement protocol negotiation (op_connect/op_accept).
- [ ] Implement SRP + legacy auth validation.
- [ ] Implement full XDR framing/packet parsing.
- [ ] Implement DSQL flow (allocate/prepare/execute/fetch).
- [ ] Implement blob streaming operations.
- [ ] Implement cancel op.
- [ ] Implement schema introspection (RDB$ tables).
- [ ] Implement BLR parser expansion for full datatype set.
- [ ] Implement status vector mapping to SQLSTATE.

### D4. ScratchBird UDR
- [ ] Implement SBWP client startup/auth/handshake.
- [ ] Implement TLS (required by default).
- [ ] Implement prepared statements + paging.
- [ ] Implement COPY streaming.
- [ ] Implement cancel/interrupt support.
- [ ] Implement sys.* schema introspection.

### D5. Conformance and Integration
- [ ] Add sbdriver-conformance adapter coverage for UDRs.
- [ ] Add conformance manifest entries per connector.
- [ ] Add end-to-end remote exec/query tests per protocol.
- [ ] Add negative tests (auth failure, TLS mismatch, permission denial).

## E) IPC + SBWP v1.1 End-to-End Support (docs/planning/PLAN_IPC_PROTOCOL_UPGRADE.md)

### E1. IPC Contract v1.1
- [ ] Extend IPC header to include protocol version + feature flags.
- [ ] Add IPC message types for startup/feature negotiation.
- [ ] Add IPC message types for prepared statement lifecycle (PARSE/BIND/DESCRIBE/EXECUTE/CLOSE).
- [ ] Add IPC message types for COPY/streaming (COPY_DATA/COPY_DONE/COPY_FAIL).
- [ ] Add IPC message types for notifications (SUBSCRIBE/UNSUBSCRIBE/DELIVER).
- [ ] Add IPC cancel and transaction lifecycle messages.
- [ ] Define attachment_id/txn_id mapping and lifetime rules.
- [ ] Define compression/checksum flags and payload framing.
- [ ] Update ENGINE_PARSER_IPC_CONTRACT.md.

### E2. Engine IPC Server
- [ ] Implement handlers for new IPC message types.
- [ ] Map IPC prepared statements to engine statement cache.
- [ ] Implement streaming/COPY with backpressure.
- [ ] Implement notification delivery to parser sessions.
- [ ] Implement cancel/interrupt propagation.

### E3. Parser Agents (Native + Emulated)
- [ ] Forward SBWP startup/feature negotiation to engine.
- [ ] Map emulated protocol features to IPC prepared/streaming paths.
- [ ] Pass attachment_id/txn_id on every IPC request.
- [ ] Map IPC errors to protocol-specific formats.

### E4. Validation
- [ ] Unit tests for IPC framing + message types.
- [ ] Integration tests for SBWP v1.1 features (prepare, COPY, notify, cancel).
- [ ] Cross-protocol tests for emulated parsers using IPC features.

## Exit Criteria

- All checklist items are complete.
- Tests for A3 and B1/B2 pass.
