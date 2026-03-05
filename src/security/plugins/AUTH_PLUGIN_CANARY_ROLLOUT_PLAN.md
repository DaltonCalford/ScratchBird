# Auth Plugin Canary Rollout Plan

Date: 2026-03-04  
Ticket: `AUTH-PROD-E01`  
Acceptance targets: `AT-E01-01`, `AT-E01-02`

## Objective
Execute progressive production enablement for all auth plugins with measurable stability gates and deterministic rollback controls.

## Rollout Sequence

### Stage 0: Pre-Canary Baseline (0%)
- Keep current production auth routing unchanged.
- Run plugin production regression suite:
  - `ctest --test-dir build -R 'sb_auth_plugin_.*(selftest|contract_harness)' --output-on-failure`
- Record baseline metrics for 24h:
  - auth allow/deny/continue/error rates
  - challenge completion rate (`scram`, `webauthn`, `factor_chain`)
  - auth latency p95/p99 by method

### Stage 1: Canary Wave 1 (1-5%)
- Plugins:
  - `trust_reject`, `peer`, `password_compat`, `token_authkey`, `proxy_assertion`
- Gate window:
  - minimum 60 minutes stable.
- Promote only if all gates pass.

### Stage 2: Canary Wave 2 (10-25%)
- Plugins:
  - `certificate_mtls`, `jwt_oidc`, `oauth_validator`, `workload_identity`
- Gate window:
  - minimum 120 minutes stable.

### Stage 3: Canary Wave 3 (25-50%)
- Plugins:
  - `ident`, `radius`, `pam`, `ldap`, `kerberos`
- Gate window:
  - minimum 180 minutes stable.

### Stage 4: Canary Wave 4 (50-100%)
- Plugins:
  - `scram`, `webauthn`, `factor_chain`
- Gate window:
  - minimum 240 minutes stable plus concurrent-session soak.

## Stability Gates (`AT-E01-01`)
- `error_rate`:
  - no sustained increase greater than +0.20 percentage points vs Stage 0 baseline over 30 minutes.
- `deny_rate`:
  - no sustained increase greater than +1.00 percentage points vs baseline over 30 minutes (excluding planned policy-deny traffic).
- `challenge_completion_rate` (`scram`/`webauthn`/`factor_chain`):
  - must remain >= 95%.
- `auth_latency_p95`:
  - must remain within +20% of baseline.
- `auth_latency_p99`:
  - must remain within +25% of baseline.
- `plugin health counters`:
  - each plugin must continue reporting `allow_count`, `deny_count`, `continue_count`, `error_count`.

## Rollback Controls (`AT-E01-02`)
Primary rollback action:
- set plugin-specific deny control to fail closed.
- if config-backed, reload plugin instance configuration.

Secondary rollback action:
- remove signing/secret material references to force fail-closed secret resolution.

Per-plugin rollback matrix:

| Plugin | Primary Rollback Switch | Type | Effect |
| --- | --- | --- | --- |
| `trust_reject` | `auth.trust_reject.trust_enabled=false` | policy | Trust path denied immediately. |
| `peer` | `auth.peer.accept_ipc=false` | policy | IPC auth path denied. |
| `password_compat` | remove `auth.password_compat.default_credential_ref` | policy/secret | Credential resolution fails closed. |
| `token_authkey` | `auth.token_authkey.expected_issuer=<invalid>` | policy | Token issuer check fails closed. |
| `certificate_mtls` | `auth.certificate_mtls.required_san_prefix=<unmatchable>` | policy | Certificate SAN policy fails closed. |
| `jwt_oidc` | `auth.jwt_oidc.jwt_expected_issuer=<invalid>` | policy | JWT issuer check fails closed. |
| `oauth_validator` | `auth.oauth_validator.expected_issuer=<invalid>` | policy | OAuth issuer check fails closed. |
| `proxy_assertion` | `auth.proxy_assertion.expected_proxy_id=<invalid>` | policy | Assertion proxy-id pin fails closed. |
| `workload_identity` | `auth.workload_identity.oidc_trust_bundle=<invalid>` | policy | OIDC trust bundle pin fails closed. |
| `ident` | `trusted_cidrs=<non-matching CIDR>` | config | IDENT source trust check fails closed. |
| `radius` | `shared_secret_ref=<missing>` | config/secret | Shared secret resolution fails closed. |
| `pam` | `service_name=<non-matching>` | config | Service policy check fails closed. |
| `ldap` | `allowed_ldap_endpoints=<empty/denylist>` | config | Endpoint allowlist denies auth. |
| `kerberos` | `allowed_kdc_endpoints=<empty/denylist>` | config | KDC endpoint allowlist denies auth. |
| `scram` | remove `auth.scram.default_credential_ref` | policy/secret | SCRAM credential lookup fails closed. |
| `webauthn` | `auth.webauthn.allowed_origin=<invalid>` | policy | Origin check fails closed. |
| `factor_chain` | `auth.factor_chain.2fa.sequence=<invalid>` | policy | Factor chain policy parse/validation fails closed. |

Rollback decision rules:
- rollback immediately if any stage gate breaches for 10 continuous minutes.
- rollback immediately on any auth bypass, signature-verification regression, or replay-control regression.
- rollback wave-local first; rollback global if breach persists 10 minutes after wave rollback.

## Verification Artifacts
- Rollback/control matrix static verification script:
  - `src/security/plugins/verify_rollout_switch_matrix.sh`
- Plugin counters coverage:
  - all plugin health payloads include `allow_count`, `deny_count`, `continue_count`, `error_count`.
- Regression suite:
  - `sb_auth_plugin_*selftest` and `sb_auth_plugin_contract_harness`.

## Exit Criteria
- `AT-E01-01`: stage gates defined and executed per wave with stable windows.
- `AT-E01-02`: rollback switches documented per plugin and verified present in plugin source/control surfaces.
