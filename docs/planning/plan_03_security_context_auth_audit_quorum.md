# Plan 03 - Security Context, AuthKey, Audit, Quorum

## Scope
Implement the security architecture draft: AuthKey, session binding, immutable transaction security context, audit persistence, quorum-aware caching.

## Priority
P0 (security baseline for server/cluster modes).

## References
- `docs/specifications/draft_security_architecture_specification.md`
- `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`
- `docs/specifications/SECURITY_IMPLIMENTATION_DETAILS.md`
- `docs/findings/engine_gap_report.md` (security gaps + matrix)
- `docs/planning/plan_16_attachment_transaction_model.md` (transaction defaults + attachment scoping)

## Order of Implementation
1) AuthKey model and storage.
2) Session/AuthKey binding in ConnectionContext.
3) Policy epoch (global + per-table) tracking.
4) Audit persistence and tamper-evident logging.
5) Quorum-aware caching with configurable behavior.
6) Role switching bound to transaction with default action policy.

## Concrete Code Touchpoints (Exact Files + Functions)
- `include/scratchbird/core/catalog_manager.h`
  - Add `AuthKeyInfo`, `AuthKeyStatus`, `AuthKeyUsage`.
  - Extend `SessionInfo` with `authkey_id`, `emulation_mode`, `policy_epoch_global`, `policy_epoch_table`.
  - Add APIs: `createAuthKey`, `getAuthKey`, `revokeAuthKey`, `consumeAuthKey`, `listAuthKeys`.
  - Add policy epoch APIs: `getSecurityPolicyEpoch`, `bumpSecurityPolicyEpoch`, `getTablePolicyEpoch`, `bumpTablePolicyEpoch`.
- `src/core/catalog_manager.cpp`
  - Add `AuthKeyRecord` near other security records.
  - Allocate table pages for `sys.cluster.security.authkeys`, `sys.security.sessions`,
    `sys.security.audit_log`, `sys.cluster.security.security_policy_epoch`.
  - Extend `CatalogRootPage` and update `writeCatalogRoot`/`readCatalogRoot` for new pages.
  - Implement AuthKey CRUD + usage-limit decrements.
  - Persist sessions (currently in-memory only) to `sys.security.sessions`.
- `include/scratchbird/core/connection_context.h` / `src/core/connection_context.cpp`
  - Add `session_id_`, `authkey_id_`, `emulation_mode_`, `policy_epoch_global_`, `policy_epoch_table_`.
  - Enforce immutability of user/role/authkey while a transaction is active.
  - Add `setSessionContext(session_id, authkey_id, emulation_mode, policy_epoch_global, policy_epoch_table)`.
- `src/core/auth_provider.cpp`
  - Create AuthKey on authentication, bind to session, log audit with authkey_id/session_id.
- `include/scratchbird/core/audit_logger.h` / `src/core/audit_logger.cpp`
  - Add fields to `AuditEvent` for `session_id` and `authkey_id` as UUIDs (not string-only).
  - Implement `writeEventToCatalog` and `queryAuditLog` using `sys.security.audit_log`.
  - Add hash-chain support for tamper-evident mode.
  - Add multi-sink support (catalog/file/broadcast).
- `src/core/permission_cache.cpp`
  - Add quorum gate in `PermissionCache::checkPermission` before using cached data.
  - Invalidate cache when policy epoch changes.
- `src/sblr/executor.cpp`
  - Ensure permission checks include session/authkey metadata from `ConnectionContext`.
  - Enforce role-switch actions at transaction boundaries (commit/rollback/default policy).
- `src/server/config_parser.cpp` / `include/scratchbird/server/config_parser.h`
  - Add config keys for audit/quorum and policy defaults (see Config section below).

## Implementation Tasks
- Add AuthKey entity: UUID, issuer, validity window, role/group scope, usage limits.
- Bind AuthKey UUID + session UUID + emulation mode into ConnectionContext SecurityContext.
- Enforce immutable security context per transaction.
- Add policy epoch tracking (global + per-table) and plan invalidation.
- Implement audit sink configuration: catalog, filesystem, broadcast.
- Add tamper-evident audit integrity (hash chain/signature/WORM options).
- Implement quorum checks for security cache with configurable fail behavior.
- Enforce role switching only at transaction boundaries with default action per user/role/group.
- Enforce cluster-wide DOMAIN DDL privileges (CREATE/ALTER/DROP) for all domain operations.

## Required Data/Schema Changes
- AuthKey catalog table with issuer, validity window, role/group scope, usage limits.
- Session catalog must link to AuthKey and security context fields.
- Policy epoch storage (global + per-table).
- Audit log storage with integrity metadata (hash chain/signature).
- Object permission support for DOMAIN object type (cluster-wide scope).
- Extend `CatalogRootPage` with: `authkeys_page`, `sessions_page`, `audit_log_page`, `security_policy_epoch_page`.

## Completion Checklist (Developer)
- [ ] AuthKey catalog/table implemented with validation logic.
- [ ] ConnectionContext stores AuthKey/session/emulation/policy epoch.
- [ ] Security context is immutable per transaction.
- [ ] Policy epoch changes invalidate plans.
- [ ] Audit sinks persist events; tamper-evident mode available.
- [ ] Quorum checks gate security cache usage.
- [ ] Role switching requires commit/rollback with default action policy.

## Completion Checklist (Auditor)
- [ ] AuthKey enforcement denies expired/invalid keys.
- [ ] Audit logs survive restart and show integrity chain.
- [ ] Quorum failure behavior matches configuration.
- [ ] Role switching mid-transaction is blocked or triggers configured action.
- [ ] Policy epoch changes invalidate cached plans.

## Testing Requirements
- AuthKey lifecycle tests (issue, expire, revoke, usage-limit consumption).
- Security context immutability tests (transaction-scoped).
- Audit persistence + integrity validation tests.
- Quorum simulation tests (partition/fail modes).
- Role switch behavior tests (default actions).
- Domain DDL permission tests (allowed/denied by role).
- Update/add tests in:
  - `tests/unit/test_audit_logger.cpp`
  - `tests/unit/test_connection_context.cpp`
  - `tests/unit/test_security_issues.cpp`
  - `tests/unit/test_session_timeout.cpp`
  - `tests/unit/test_password_policy.cpp`

## Acceptance Criteria
- AuthKey/session binding present in all security decisions and audit events.
- Policy epoch changes invalidate cached plans.
- Quorum configuration enforces cache gating as configured.
- Audit logs persist across restart and verify integrity chain.

## Implementation Notes (Concrete)
- **AuthKey table schema** (example): `sys.cluster.security.authkeys(authkey_id, issuer, valid_from, valid_to, usage_limit, role_scope_oid, group_scope_oid, status)`.
- **Session table linkage**: `sys.security.sessions(session_id, user_id, authkey_id, emulation_mode, last_seen, policy_epoch_global, policy_epoch_table)`.
- **Security context fields**: extend `ConnectionContext::SecurityContext` to include `session_id`, `authkey_id`, `emulation_mode`, `policy_epoch_global`, `policy_epoch_table`.
- **Audit storage**: `sys.security.audit_log(event_id, timestamp, session_id, authkey_id, user_id, role_id, action, object_id, details, hash_prev, hash_curr)`.
- **Quorum config**: `security_quorum_n`, `security_quorum_m`, `quorum_failure_mode` with optional key escrow/decryption requirement.
- **Role switching**: require explicit `SET ROLE ... WITH COMMIT|ROLLBACK` or apply per-user default policy.

## Expanded API/Schema Details
- **CatalogManager session APIs**:
  - `createSession(user_id, authkey_id, emulation_mode, SessionInfo& out, ErrorContext* ctx)`
  - `getSession(session_id, SessionInfo& out, ErrorContext* ctx)`
  - `closeSession(session_id, ErrorContext* ctx)`
  - `updateSessionActivity(session_id, ErrorContext* ctx)`
- **CatalogManager AuthKey APIs**:
  - `createAuthKey(const AuthKeyInfo& in, ID& out_id, ErrorContext* ctx)`
  - `getAuthKey(const ID& authkey_id, AuthKeyInfo& out, ErrorContext* ctx)`
  - `revokeAuthKey(const ID& authkey_id, ErrorContext* ctx)`
  - `consumeAuthKey(const ID& authkey_id, uint32_t uses, ErrorContext* ctx)`
- **ConnectionContext APIs**:
  - `setCurrentUser(const ID& user_id, bool is_superuser)`
  - `setActiveRole(const ID& role_id)`
  - `pushSecurityContext(...)` / `popSecurityContext()`
  - `setSessionContext(const ID& session_id, const ID& authkey_id, const std::string& emulation_mode,
     uint64_t policy_epoch_global, uint64_t policy_epoch_table)`
- **Audit APIs**:
  - `AuditLogger::logEvent(AuditEvent&, ErrorContext*)`
  - `AuditLogger::queryAuditLog(const AuditQuery&, std::vector<AuditEvent>&, ErrorContext*)`
- **Policy epochs**:
  - global epoch stored in catalog (e.g., `sys.cluster.security.security_policy_epoch`).
  - per-table epoch stored in `sys.catalog.tables.policy_epoch`.

## Full Implementation Detail (No Ambiguity)
- **AuthKey fields**:
  - `authkey_id` (UUID), `issuer` (text), `valid_from` (timestamp), `valid_to` (timestamp), `usage_limit` (int), `status` (enum), `role_scope` (UUID list or TOAST), `group_scope` (UUID list or TOAST).
- **Session binding**:
  - On authentication: create session row and store `session_id` + `authkey_id` in `ConnectionContext`.
  - Session must be attached to every transaction.
- **Security context immutability**:
  - Disallow changes to `current_user_id_`, `active_role_id_`, or `authkey_id` during active transaction; enforce via `commit/rollback` path only.
- **Audit persistence**:
  - Append-only table or file with hash chain (`hash_prev`, `hash_curr`).
  - Must survive restart and be queryable via `AuditLogger::queryAuditLog`.
- **Policy epoch**:
  - `sys.cluster.security.security_policy_epoch` has one row with `global_epoch` (BIGINT).
  - `sys.catalog.tables` gains `policy_epoch` (BIGINT) for per-table tracking.
  - Any GRANT/REVOKE/ALTER POLICY bumps `global_epoch`; any table-specific policy change bumps table epoch.
- **Quorum checks**:
  - `PermissionCache::checkPermission` must call `SecurityQuorum::isQuorumSatisfied(ctx)` before using cached data.
  - On quorum failure: follow config `quorum_failure_mode` (`fail_closed`, `fail_open`, `require_remote`).

## Concrete Schema DDL (Example)
- `sys.cluster.security.authkeys`:
  - `authkey_id UUID PRIMARY KEY`
  - `issuer TEXT`
  - `valid_from TIMESTAMP`
  - `valid_to TIMESTAMP`
  - `usage_limit INT`
  - `status SMALLINT`
  - `role_scope_oid OID` (TOAST)
  - `group_scope_oid OID` (TOAST)
- `sys.security.sessions`:
  - `session_id UUID PRIMARY KEY`
  - `user_id UUID`
  - `authkey_id UUID`
  - `emulation_mode TEXT`
  - `last_seen TIMESTAMP`
  - `policy_epoch_global BIGINT`
  - `policy_epoch_table BIGINT`
- `sys.security.audit_log`:
  - `event_id BIGINT PRIMARY KEY`
  - `timestamp BIGINT`
  - `session_id UUID`
  - `authkey_id UUID`
  - `user_id UUID`
  - `role_id UUID`
  - `action TEXT`
  - `object_id UUID`
  - `details TEXT`
  - `hash_prev BINARY(32)`
  - `hash_curr BINARY(32)`
- `sys.cluster.security.security_policy_epoch`:
  - `global_epoch BIGINT`

## Catalog Schema Placement (Required)
- **Authoritative security tables**: `sys.cluster.security` schema path.
- **Node-local runtime/persistence**: `sys.security` schema path.
- **Local cache tables** (if persisted): `sys.security.cache` schema path.

## Full Catalog DDL (Required)
```sql
CREATE TABLE sys.cluster.security.authkeys (
  authkey_id UUID PRIMARY KEY,
  issuer TEXT NOT NULL,
  valid_from TIMESTAMP NOT NULL,
  valid_to TIMESTAMP NOT NULL,
  usage_limit INT,
  status SMALLINT NOT NULL,
  role_scope_oid OID,
  group_scope_oid OID
);

CREATE TABLE sys.security.sessions (
  session_id UUID PRIMARY KEY,
  user_id UUID NOT NULL,
  authkey_id UUID NOT NULL,
  emulation_mode TEXT NOT NULL,
  last_seen TIMESTAMP NOT NULL,
  policy_epoch_global BIGINT NOT NULL,
  policy_epoch_table BIGINT NOT NULL
);

CREATE TABLE sys.security.audit_log (
  event_id BIGINT PRIMARY KEY,
  timestamp BIGINT NOT NULL,
  session_id UUID NOT NULL,
  authkey_id UUID NOT NULL,
  user_id UUID NOT NULL,
  role_id UUID,
  action TEXT NOT NULL,
  object_id UUID,
  details TEXT,
  hash_prev BINARY(32),
  hash_curr BINARY(32)
);

CREATE TABLE sys.cluster.security.security_policy_epoch (
  global_epoch BIGINT NOT NULL
);
```

## Concrete Config Keys (Example)
- `security_quorum_n`, `security_quorum_m`, `security_quorum_failure_mode`
- `audit_sink_catalog`, `audit_sink_file`, `audit_sink_kafka`
- `audit_integrity_mode`, `audit_encryption_mode`, `audit_key_source`
- `role_switch_default_action` (per user/role/group)

## Step-by-Step Implementation (No Gaps)
1) **Catalog tables + root page**:
   - Add `authkeys_table_page_`, `sessions_table_page_`, `audit_log_table_page_`, `security_policy_epoch_table_page_` to
     `include/scratchbird/core/catalog_manager.h`.
   - Extend `CatalogRootPage` in `src/core/catalog_manager.cpp` to store those page IDs and update read/write.
   - Allocate pages in `CatalogManager::initializeCatalogTables()` (same block as users/roles).
2) **AuthKey persistence**:
   - Add `AuthKeyRecord` in `src/core/catalog_manager.cpp`.
   - Implement `createAuthKey/getAuthKey/revokeAuthKey/consumeAuthKey` in `CatalogManager`.
   - Enforce `valid_from/valid_to/status/usage_limit` at lookup time.
3) **Session persistence**:
   - Extend `SessionInfo` with authkey/session metadata and policy epochs.
   - Persist sessions in `sys.security.sessions` inside `CatalogManager::createSession` and `closeSession`.
4) **ConnectionContext binding**:
   - Add fields for `session_id_`, `authkey_id_`, `emulation_mode_`, `policy_epoch_global_`, `policy_epoch_table_`.
   - Disallow `setCurrentUser/setActiveRole` when `current_xid_ != 0` (active transaction) unless a commit/rollback is done.
5) **Audit logger persistence**:
   - Implement `AuditLogger::writeEventToCatalog()` to insert into `sys.security.audit_log`.
   - Add hash-chain: `hash_curr = H(hash_prev + canonical_event_bytes)`; store `hash_prev/hash_curr`.
6) **Quorum-aware cache**:
   - Add `SecurityQuorum` helper (new file `src/core/security_quorum.cpp` + header).
   - Gate `PermissionCache::checkPermission` on quorum status.
7) **Role switching rules**:
   - Implement `SET ROLE` behavior in executor: if a transaction is active and default action is not provided,
     apply per-user default policy (`commit`, `rollback`, `reject`).

## Concrete Test Cases
- **AuthKey lifecycle**: issue, expire, revoke; ensure denied when invalid.
- **Session binding**: verify session_id/authkey_id included in audit events.
- **Immutability**: attempt role change mid-transaction must fail or follow configured default action.
- **Quorum**: simulate partition; ensure cache behavior matches configured mode.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
