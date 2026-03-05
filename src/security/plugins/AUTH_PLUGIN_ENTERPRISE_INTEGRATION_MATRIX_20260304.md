# Auth Plugin Enterprise Integration Matrix

Date: 2026-03-04
Ticket: `AUTH-PROD-F07` (completed)

## Scope
- Enterprise/provider-heavy auth plugins:
  - `ident`
  - `radius`
  - `pam`
  - `ldap`
  - `kerberos`
- Matrix goal:
  - Validate provider-driven success/error/timeout/policy branches.
  - Validate no fail-open regression after Phase F continuation updates.

## Matrix Gate Command
```bash
src/security/plugins/auth_plugin_enterprise_matrix_gate.sh build
```

## Gate Result Summary
1. Targeted enterprise matrix gate:
   - `ctest -R 'sb_auth_plugin_(ident|radius|pam|ldap|kerberos)_selftest|sb_auth_plugin_hardening_h1_selftest'`
   - Result: `6/6` passed.
2. Full plugin fail-open regression gate:
   - `ctest -R 'sb_auth_plugin_.*(selftest|contract_harness)'`
   - Result: `26/26` passed.

## Branch Coverage Matrix
| Plugin | Success | Error | Timeout/Replay | Policy/Hardening | Evidence Source |
| --- | --- | --- | --- | --- | --- |
| `ident` | `AT-B10-01` | `AT-B10-02` spoof/user mismatch | N/A | `AT-B10-02` untrusted CIDR deny | `sb_auth_plugin_ident_selftest` |
| `radius` | primary/secondary failover allow | bad secret / reject deny | timeout directive branch | production-profile test-directive deny | `sb_auth_plugin_radius_selftest` + `sb_auth_plugin_hardening_h1_selftest` + `evaluateRadiusProviderDecision` |
| `pam` | `AT-B12-01` | malformed/blocked/deny branches | timeout branch | production-profile test-directive deny | `sb_auth_plugin_pam_selftest` + `sb_auth_plugin_hardening_h1_selftest` + `evaluatePamProviderDecision` |
| `ldap` | `AT-B13-01` | bind/starttls/mapping deny branches | timeout branch | production-profile test-directive deny | `sb_auth_plugin_ldap_selftest` + `sb_auth_plugin_hardening_h1_selftest` + `evaluateLdapProviderDecision` |
| `kerberos` | `AT-B14-01` | ticket invalid/malformed deny | replay + expired branches | synthetic-marker-free in production baseline | `sb_auth_plugin_kerberos_selftest` + `auth_plugin_provider_gap_audit.sh` |

## Fail-Open Regression Statement
- With all enterprise plugin selftests and full plugin regression passing, no fail-open path was observed in tested begin/continue/health contract flows for this matrix pass.

## Completion Notes
1. `ldap`, `radius`, and `pam` now route nominal allow/deny/timeout/policy outcomes through explicit provider-decision helpers (not inline synthetic-first branches).
2. Enterprise selftests for `ldap`, `radius`, and `pam` now assert `healthCheck` metadata for `runtime_profile=production` and native `provider_driver` identifiers.
3. This artifact is the recorded matrix evidence for `AT-F07-01` and `AT-F07-02`.
