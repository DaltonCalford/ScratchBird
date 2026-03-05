# Auth Plugin Threat Model (Phase D01)

Date: 2026-03-04
Scope: all auth plugins under `src/security/plugins` (`trust_reject`, `peer`, `password_compat`, `token_authkey`, `certificate_mtls`, `jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `ident`, `radius`, `pam`, `ldap`, `kerberos`, `scram`, `webauthn`, `factor_chain`).

## 1. Security Objectives

1. Prevent unauthorized principal resolution and session establishment.
2. Guarantee fail-closed behavior on malformed payloads, policy errors, and secret/provider failures.
3. Prevent replay across challenge and non-challenge methods.
4. Keep secret material out of audit payloads and health endpoints.
5. Preserve deterministic auth outcomes for incident response and compliance traceability.

## 2. Protected Assets

1. Principal UUID resolution path (`resolve_user_by_name`, `resolve_user_by_external_subject`).
2. Secret material from policy and literal references (shared secrets, signing keys, keytabs, credential payloads).
3. Challenge lifecycle state (exchange IDs, nonce/challenge values, replay caches).
4. Policy controls (issuer/audience/RP/transport/endpoint allowlists, method sequencing).
5. Audit stream and plugin health payloads.

## 3. Abuse-Case Matrix

| Abuse ID | Attack Pattern | Primary Plugins | Required Mitigation |
| --- | --- | --- | --- |
| AC-01 | Malformed payload parser confusion | all | Strict parse + explicit deny code; bounded payload sizes |
| AC-02 | Unsupported method downgrade or method confusion | all | Method-id allowlist and unsupported return path |
| AC-03 | Replay of challenge exchange or final proof/assertion | `scram`, `webauthn`, `factor_chain`, `kerberos` | Exchange replay key + proof/challenge replay key enforcement |
| AC-04 | Token/assertion signature forgery | `token_authkey`, `jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `webauthn`, `factor_chain`, `kerberos` | Per-request key resolution + deterministic signature verification + const-time compare |
| AC-05 | Expired token/ticket/assertion acceptance | `token_authkey`, `jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `webauthn`, `kerberos` | `exp`/timestamp checks against host time |
| AC-06 | Issuer/audience/RP/origin confusion | `token_authkey`, `jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `webauthn` | Policy-pinned issuer/audience/rp/origin enforcement |
| AC-07 | Untrusted network boundary bypass | `peer`, `proxy_assertion`, `ident`, `radius`, `ldap`, `kerberos`, `certificate_mtls` | Transport gating + endpoint/CIDR/KDC/proxy validation |
| AC-08 | Challenge step skipping or factor-order bypass | `factor_chain` | Policy sequence engine + strict next-step validation |
| AC-09 | Subject switching mid-flow | `scram`, `webauthn`, `factor_chain` | Subject continuity checks across begin/continue |
| AC-10 | Secret exfiltration through audit and health channels | all | Audit payload minimization + health payload count-only outputs + hygiene tests |
| AC-11 | Provider outage or missing secret causing silent allow | all provider-backed plugins | Fail-closed on missing secret/provider, deterministic deny codes |
| AC-12 | Ambiguous incident diagnostics | all | Stable plugin error codes and rc mapping in audit events |

## 4. Plugin Coverage Map

| Plugin | Mapped Abuse IDs |
| --- | --- |
| `trust_reject` | AC-01, AC-02, AC-11, AC-12 |
| `peer` | AC-01, AC-02, AC-07, AC-11, AC-12 |
| `password_compat` | AC-01, AC-02, AC-11, AC-12 |
| `token_authkey` | AC-01, AC-02, AC-04, AC-05, AC-06, AC-11, AC-12 |
| `certificate_mtls` | AC-01, AC-02, AC-05, AC-07, AC-11, AC-12 |
| `jwt_oidc` | AC-01, AC-02, AC-04, AC-05, AC-06, AC-11, AC-12 |
| `oauth_validator` | AC-01, AC-02, AC-04, AC-05, AC-06, AC-11, AC-12 |
| `proxy_assertion` | AC-01, AC-02, AC-04, AC-05, AC-06, AC-07, AC-11, AC-12 |
| `workload_identity` | AC-01, AC-02, AC-04, AC-05, AC-06, AC-07, AC-11, AC-12 |
| `ident` | AC-01, AC-02, AC-07, AC-11, AC-12 |
| `radius` | AC-01, AC-02, AC-07, AC-11, AC-12 |
| `pam` | AC-01, AC-02, AC-11, AC-12 |
| `ldap` | AC-01, AC-02, AC-07, AC-11, AC-12 |
| `kerberos` | AC-01, AC-02, AC-03, AC-04, AC-05, AC-07, AC-11, AC-12 |
| `scram` | AC-01, AC-02, AC-03, AC-09, AC-11, AC-12 |
| `webauthn` | AC-01, AC-02, AC-03, AC-04, AC-05, AC-06, AC-09, AC-11, AC-12 |
| `factor_chain` | AC-01, AC-02, AC-03, AC-04, AC-08, AC-09, AC-11, AC-12 |

## 5. Mitigation-to-Test Traceability

| Mitigation | Evidence Tests |
| --- | --- |
| Contract and ABI invariants (`AC-01`, `AC-02`, `AC-12`) | `sb_auth_plugin_contract_harness` |
| Shared challenge replay controls (`AC-03`) | `sb_auth_plugin_challenge_state_store_selftest`, `sb_auth_plugin_scram_selftest`, `sb_auth_plugin_webauthn_selftest`, `sb_auth_plugin_factor_chain_selftest`, `sb_auth_plugin_kerberos_selftest` |
| Secret resolution fail-closed (`AC-11`) | `sb_auth_plugin_secret_provider_selftest` plus plugin-specific selftests (`token_authkey`, `jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `kerberos`, `radius`, `scram`, `webauthn`, `factor_chain`) |
| Signature/claims/path policy verification (`AC-04`, `AC-05`, `AC-06`, `AC-07`) | plugin selftests for `token_authkey`, `jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `certificate_mtls`, `ident`, `ldap`, `radius`, `kerberos`, `webauthn` |
| Factor chain sequencing and subject continuity (`AC-08`, `AC-09`) | `sb_auth_plugin_factor_chain_selftest`, `sb_auth_plugin_scram_selftest`, `sb_auth_plugin_webauthn_selftest` |
| Secret hygiene (`AC-10`) | `sb_auth_plugin_secrets_hygiene_selftest` |
| Fuzz robustness (`AC-01`, `AC-11`) | `sb_auth_plugin_payload_fuzz_selftest` |

## 6. Residual Risks

1. Current cryptographic primitives are deterministic lightweight checks used for plugin-stage validation; full cryptographic backend hardening remains under `AUTH-PROD-D04`.
2. Provider integrations are policy-driven and fail-closed, but external service SLAs and operational rollback readiness are covered by Phase E.
3. Fuzzing is deterministic corpusless mutation fuzzing; sanitizer-driven deep fuzz campaigns remain a future enhancement item.
