# Auth Plugin Incident and Rollback Runbook

Date: 2026-03-04  
Ticket: `AUTH-PROD-E03`

## Purpose
Provide operational procedures for detection, triage, containment, rollback, and recovery for production auth-plugin incidents.

## Scope
- All 17 auth plugins under `src/security/plugins`.
- Incidents affecting authentication correctness, latency, availability, or security controls.
- This runbook is used together with:
  - `AUTH_PLUGIN_CANARY_ROLLOUT_PLAN.md`
  - `AUTH_PLUGIN_DASHBOARD_ALERTS_SPEC.md`

## Severity Model
1. `SEV1`: auth bypass/signature verification regression/replay-control regression.
2. `SEV2`: sustained p99 latency breach, deny-rate regression, provider error-rate regression.
3. `SEV3`: challenge-completion-rate regression without user lockout.

## Detection and Triage
1. Confirm alert source and severity from routing matrix.
2. Identify impacted plugin and method via:
   - dashboard panel breakdown by `plugin_id` and `method_id`,
   - plugin audit event stream (`auth_plugin.<plugin>.begin/continue`).
3. Establish blast radius:
   - affected traffic percent,
   - whether incident is wave-local or global.
4. Capture first timeline marker:
   - `incident_opened_utc` and triggering alert rule.

## Immediate Containment
1. Freeze further rollout progression for current wave.
2. Increase logging verbosity only for affected plugin boundaries if available via host controls.
3. If `SEV1`, execute rollback immediately (skip extended diagnostics).

## Rollback Procedure
1. Apply plugin-local rollback switch (from matrix below).
2. Reload policy/config for affected plugin instances.
3. Verify rollback by checking:
   - deny path active for rolled-back plugin,
   - `error_count` stabilizes and no new bypass indicators,
   - downstream auth success on unaffected plugins remains within baseline.
4. If wave-local rollback does not stabilize within 10 minutes, execute global rollback.

## Plugin-Specific Rollback Controls
| Plugin | Rollback Control |
| --- | --- |
| `trust_reject` | `auth.trust_reject.trust_enabled=false` |
| `peer` | `auth.peer.accept_ipc=false` |
| `password_compat` | remove `auth.password_compat.default_credential_ref` |
| `token_authkey` | `auth.token_authkey.expected_issuer=<invalid>` |
| `certificate_mtls` | `auth.certificate_mtls.required_san_prefix=<unmatchable>` |
| `jwt_oidc` | `auth.jwt_oidc.jwt_expected_issuer=<invalid>` |
| `oauth_validator` | `auth.oauth_validator.expected_issuer=<invalid>` |
| `proxy_assertion` | `auth.proxy_assertion.expected_proxy_id=<invalid>` |
| `workload_identity` | `auth.workload_identity.oidc_trust_bundle=<invalid>` |
| `ident` | `trusted_cidrs=<non-matching CIDR>` |
| `radius` | `shared_secret_ref=<missing>` |
| `pam` | `service_name=<non-matching>` |
| `ldap` | `allowed_ldap_endpoints=<denylist>` |
| `kerberos` | `allowed_kdc_endpoints=<denylist>` |
| `scram` | remove `auth.scram.default_credential_ref` |
| `webauthn` | `auth.webauthn.allowed_origin=<invalid>` |
| `factor_chain` | `auth.factor_chain.2fa.sequence=<invalid>` |

## Validation Steps After Rollback
1. Confirm active alerts transition from firing to recovering.
2. Confirm auth plugin health counters (`allow_count`, `deny_count`, `continue_count`, `error_count`) are still emitted.
3. Run plugin regression gate:
   - `ctest --test-dir build -R 'sb_auth_plugin_.*(selftest|contract_harness)' --output-on-failure`
4. Record rollback completion timestamp and status.

## Communication and Escalation
1. `SEV1`: notify `pagerduty:security-primary` and `slack:#scratchbird-auth-incidents`.
2. `SEV2`: notify `pagerduty:database-runtime` and `slack:#scratchbird-runtime-alerts`.
3. `SEV3`: notify `slack:#scratchbird-auth-observability`.
4. Post update cadence:
   - every 15 minutes for `SEV1/SEV2`,
   - every 30 minutes for `SEV3`.

## Recovery and Exit Criteria
1. Metrics stable for one full gate window after rollback.
2. No active high/critical auth security findings.
3. Incident commander and on-call approvers agree on closure.

## Post-Incident Actions
1. Open corrective ticket(s) for root-cause fixes.
2. Add/adjust tests covering trigger condition.
3. Update runbook and dashboard alert thresholds if required.
4. Capture final incident summary in operations records.
