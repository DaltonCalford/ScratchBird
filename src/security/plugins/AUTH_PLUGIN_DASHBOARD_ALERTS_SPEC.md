# Auth Plugin Dashboard and Alert Specification

Date: 2026-03-04  
Ticket: `AUTH-PROD-E02`  
Acceptance targets: `AT-E02-01`, `AT-E02-02`

## Scope
- Dashboard panels and alert rules for auth plugin production monitoring.
- Plugin scope includes all 17 auth plugins under `src/security/plugins`.
- Uses plugin health counters and host-side auth timing series.

## Dashboard Definition (`AT-E02-01`)
Dashboard manifest file:
- `src/security/plugins/auth_plugin_dashboard_alerts_manifest.json`

Required panels:
1. `auth-latency-p95`  
   Metric: `auth.plugin.request.duration_ms` (p95), grouped by `plugin_id`, `method_id`.
2. `auth-latency-p99`  
   Metric: `auth.plugin.request.duration_ms` (p99), grouped by `plugin_id`, `method_id`.
3. `auth-deny-rate`  
   Metric: `auth.plugin.outcome.count` filtered on `rc=deny`, shown as rate.
4. `auth-challenge-completion-rate`  
   Expression: `auth.plugin.challenge.completed.count / auth.plugin.challenge.started.count`.
5. `auth-provider-error-rate`  
   Metric: `auth.plugin.outcome.count` filtered on `rc=error`, shown as rate.

All panels include per-plugin breakdown and a 5-minute or 15-minute operational window.

## Alert Routing (`AT-E02-02`)
Routing matrix file:
- `src/security/plugins/auth_plugin_alert_routing_matrix.json`

Severities and default routes:
1. `sev1` -> `pagerduty:security-primary`, `slack:#scratchbird-auth-incidents`
2. `sev2` -> `pagerduty:database-runtime`, `slack:#scratchbird-runtime-alerts`
3. `sev3` -> `slack:#scratchbird-auth-observability`

Core alert rules:
1. Security regression (bypass/signature/replay): `sev1`
2. Latency p99 breach: `sev2`
3. Deny-rate regression: `sev2`
4. Provider error-rate regression: `sev2`
5. Challenge completion-rate breach: `sev3`

## Verification
Static coverage selftest:
- `sb_auth_plugin_dashboard_alerts_selftest`
- Validates:
  - required dashboard panel IDs and metric IDs are present,
  - routing severities and destinations are present,
  - all 17 plugins have ownership/routing entries.

Regression and validation commands:
1. `ctest --test-dir build -R sb_auth_plugin_dashboard_alerts_selftest --output-on-failure`
2. `ctest --test-dir build -R 'sb_auth_plugin_.*(selftest|contract_harness)' --output-on-failure`

## Acceptance Mapping
- `AT-E02-01`: satisfied by dashboard manifest panel coverage (`p95/p99`, deny, challenge, provider error).
- `AT-E02-02`: satisfied by alert routing matrix plus `sb_auth_plugin_dashboard_alerts_selftest` pass.
