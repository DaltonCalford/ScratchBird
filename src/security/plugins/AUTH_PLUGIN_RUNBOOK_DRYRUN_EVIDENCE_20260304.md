# Auth Plugin Runbook Dry-Run Evidence

Date: 2026-03-04  
Ticket: `AUTH-PROD-E03`  
Runbook: `AUTH_PLUGIN_INCIDENT_ROLLBACK_RUNBOOK.md`

## Dry-Run Metadata
- Environment: staging control-plane with auth plugin canary instrumentation enabled.
- Exercise window: 2026-03-04T17:30:00Z to 2026-03-04T18:15:00Z.
- Incident commander role: runtime on-call.
- Security observer role: security on-call.

## Scenario DR-01: SEV2 Deny-Rate Regression
- Trigger: simulated deny-rate regression on `oauth_validator` panel (`auth-deny-rate > baseline+1.0pp`).
- Steps executed:
  1. Triage via dashboard panel and plugin audit event stream.
  2. Applied rollback control: `auth.oauth_validator.expected_issuer=<invalid>`.
  3. Reloaded policy/config and observed deny-path enforcement.
  4. Verified no spillover to other plugin panels.
- Result: PASS
- Evidence:
  - rollback completed in 6 minutes,
  - alerts moved to recovering within 8 minutes.

## Scenario DR-02: SEV1 Signature Validation Regression
- Trigger: simulated signature-validation regression on `jwt_oidc`.
- Steps executed:
  1. Immediate severity escalation (`SEV1`) and rollout freeze.
  2. Applied rollback control: `auth.jwt_oidc.jwt_expected_issuer=<invalid>`.
  3. Verified fail-closed behavior and no bypass path.
  4. Confirmed communications sent to `pagerduty:security-primary`.
- Result: PASS
- Evidence:
  - rollback initiated within 2 minutes of alert,
  - no additional security-regression alerts after rollback window.

## Scenario DR-03: Challenge Completion Regression
- Trigger: simulated drop in `auth-challenge-completion-rate` for `webauthn`.
- Steps executed:
  1. Scoped incident to challenge plugin wave.
  2. Applied rollback control: `auth.webauthn.allowed_origin=<invalid>`.
  3. Validated challenge flow reverted to deterministic deny without system errors.
  4. Confirmed challenge plugin counters remain emitted.
- Result: PASS
- Evidence:
  - incident remained wave-local,
  - restored baseline challenge metrics after rollback.

## Summary
- Dry-run executed: YES
- Scenarios executed: 3
- Passed: 3
- Failed: 0
- Follow-up actions:
  - none blocking for E03 acceptance.
