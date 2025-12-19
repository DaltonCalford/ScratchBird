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

## Order of Implementation
1) AuthKey model and storage.
2) Session/AuthKey binding in ConnectionContext.
3) Policy epoch (global + per-table) tracking.
4) Audit persistence and tamper-evident logging.
5) Quorum-aware caching with configurable behavior.
6) Role switching bound to transaction with default action policy.

## Implementation Tasks
- Add AuthKey entity: UUID, issuer, validity window, role/group scope, usage limits.
- Bind AuthKey UUID + session UUID + emulation mode into ConnectionContext SecurityContext.
- Enforce immutable security context per transaction.
- Add policy epoch tracking (global + per-table) and plan invalidation.
- Implement audit sink configuration: catalog, filesystem, broadcast.
- Add tamper-evident audit integrity (hash chain/signature/WORM options).
- Implement quorum checks for security cache with configurable fail behavior.
- Enforce role switching only at transaction boundaries with default action per user/role/group.

## Required Data/Schema Changes
- AuthKey catalog table with issuer, validity window, role/group scope, usage limits.
- Session catalog must link to AuthKey and security context fields.
- Policy epoch storage (global + per-table).
- Audit log storage with integrity metadata (hash chain/signature).

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
- AuthKey lifecycle tests (issue, expire, revoke).
- Security context immutability tests (transaction-scoped).
- Audit persistence + integrity validation tests.
- Quorum simulation tests (partition/fail modes).
- Role switch behavior tests (default actions).

## Acceptance Criteria
- AuthKey/session binding present in all security decisions and audit events.
- Policy epoch changes invalidate cached plans.
- Quorum configuration enforces cache gating as configured.
- Audit logs persist across restart and verify integrity chain.

## Implementation Notes (Concrete)
- **AuthKey table schema** (example): `sb_authkeys(authkey_id, issuer, valid_from, valid_to, usage_limit, role_scope_oid, group_scope_oid, status)`.
- **Session table linkage**: `sb_sessions(session_id, user_id, authkey_id, emulation_mode, last_seen)`.
- **Security context fields**: extend `ConnectionContext::SecurityContext` to include `session_id`, `authkey_id`, `emulation_mode`, `policy_epoch_global`, `policy_epoch_table`.
- **Audit storage**: `sb_audit_log(event_id, timestamp, session_id, authkey_id, user_id, role_id, action, object_id, details, hash_prev, hash_curr)`.
- **Quorum config**: `security_quorum_n`, `security_quorum_m`, `quorum_failure_mode` with optional key escrow/decryption requirement.
- **Role switching**: require explicit `SET ROLE ... WITH COMMIT|ROLLBACK` or apply per-user default policy.

## Expanded API/Schema Details
- **CatalogManager session APIs**:
  - `createSession(user_id, authkey_id, emulation_mode, SessionInfo& out, ErrorContext* ctx)`
  - `getSession(session_id, SessionInfo& out, ErrorContext* ctx)`
  - `closeSession(session_id, ErrorContext* ctx)`
  - `updateSessionActivity(session_id, ErrorContext* ctx)`
- **ConnectionContext APIs**:
  - `setCurrentUser(const ID& user_id, bool is_superuser)`
  - `setActiveRole(const ID& role_id)`
  - `pushSecurityContext(...)` / `popSecurityContext()`
- **Audit APIs**:
  - `AuditLogger::logEvent(AuditEvent&, ErrorContext*)`
  - `AuditLogger::queryAuditLog(const AuditQuery&, std::vector<AuditEvent>&, ErrorContext*)`
- **Policy epochs**:
  - global epoch stored in catalog (e.g., `sb_security_policy_epoch`).
  - per-table epoch stored in `sb_table` or a policy table.

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
- **Quorum checks**:
  - Configurable `N-of-M` with fail mode; if required, decrypt audit/security cache using key retrieved from another cluster member.

## Concrete Schema DDL (Example)
- `sb_authkeys`:
  - `authkey_id UUID PRIMARY KEY`
  - `issuer TEXT`
  - `valid_from TIMESTAMP`
  - `valid_to TIMESTAMP`
  - `usage_limit INT`
  - `status SMALLINT`
  - `role_scope_oid OID` (TOAST)
  - `group_scope_oid OID` (TOAST)
- `sb_sessions`:
  - `session_id UUID PRIMARY KEY`
  - `user_id UUID`
  - `authkey_id UUID`
  - `emulation_mode TEXT`
  - `last_seen TIMESTAMP`
- `sb_audit_log`:
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
- `sb_security_policy_epoch`:
  - `global_epoch BIGINT`

## Full Catalog DDL (Required)
```sql
CREATE TABLE sb_authkeys (
  authkey_id UUID PRIMARY KEY,
  issuer TEXT NOT NULL,
  valid_from TIMESTAMP NOT NULL,
  valid_to TIMESTAMP NOT NULL,
  usage_limit INT,
  status SMALLINT NOT NULL,
  role_scope_oid OID,
  group_scope_oid OID
);

CREATE TABLE sb_sessions (
  session_id UUID PRIMARY KEY,
  user_id UUID NOT NULL,
  authkey_id UUID NOT NULL,
  emulation_mode TEXT NOT NULL,
  last_seen TIMESTAMP NOT NULL
);

CREATE TABLE sb_audit_log (
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

CREATE TABLE sb_security_policy_epoch (
  global_epoch BIGINT NOT NULL
);
```

## Concrete Config Keys (Example)
- `security_quorum_n`, `security_quorum_m`, `security_quorum_failure_mode`
- `audit_sink_catalog`, `audit_sink_file`, `audit_sink_kafka`
- `audit_integrity_mode`, `audit_encryption_mode`, `audit_key_source`
- `role_switch_default_action` (per user/role/group)

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
