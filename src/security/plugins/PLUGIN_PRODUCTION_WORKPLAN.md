# Auth Plugins Full Production Workplan

Date baseline: 2026-03-04
Scope: move all auth plugins from scaffold/beta behavior to full production authentication status.

## 1. Production Definition
A plugin is `Production Ready` only when all criteria are met:
1. Real verifier/provider path implemented for all advertised methods.
2. No placeholder terminal errors (`*_PENDING`, `*_NOT_IMPLEMENTED`) in normal execution paths.
3. Strict config schema validation and clear startup-time failure reporting.
4. Strong security controls: replay protection, timeout controls, input bounds, secret-handling hygiene.
5. Audit events and metrics emitted for allow, deny, challenge, timeout, and policy-denied outcomes.
6. Unit tests, integration tests, and negative-path tests passing in CI.
7. Performance and resilience budgets met under load and fault injection.
8. Operational runbook complete (alerts, rollback, diagnostics).

## 2. Program Phases

### Phase A: Platform Hardening (prerequisite)
1. Replace permissive flat config parsing with strict per-plugin JSON schema parsing.
2. Add shared plugin utility for structured audit/metrics emission.
3. Add shared challenge-state store with TTL, nonce replay cache, and cleanup guarantees.
4. Add secret-provider integration contract for keytab/shared-secret/token signing material.
5. Add plugin contract tests runnable for any plugin target.

Exit gate:
- All plugins compile against hardened utility layer.
- Contract test harness runs in CI.

### Phase B: Complete Non-Challenge Plugins
Plugins:
- `trust_reject`, `peer`, `password_compat`, `token_authkey`, `proxy_assertion`, `certificate_mtls`, `jwt_oidc`, `oauth_validator`, `workload_identity`, `ident`, `radius`, `pam`, `ldap`, `kerberos`

Key work:
1. Replace fail-closed placeholder terminal branches with provider-backed verification logic.
2. Enforce policy integration at runtime (method allowlist, issuer allowlist, endpoint allowlist, signer/issuer pinning).
3. Add authoritative principal resolution flow and deterministic conflict handling.
4. Add per-plugin configuration self-check at startup.

Exit gate:
- No `*_PROVIDER_PENDING` style failures in nominal supported deployments.
- Integration matrix green for all non-challenge methods.

### Phase C: Complete Challenge/MFA Plugins
Plugins:
- `scram`, `webauthn`, `factor_chain`

Key work:
1. Implement full verifier backends for final proof/assertion validation.
2. Add challenge integrity protections: per-exchange binding, nonce freshness windows, replay rejection.
3. Add continuation flow invariants and cancellation safety under concurrent sessions.
4. Add high-load soak tests for challenge lifecycle and cleanup.

Exit gate:
- Full begin/continue/abort lifecycle verified under concurrency and replay attack tests.

### Phase D: Security and Compliance Validation
1. Threat-model review and abuse-case testing for all plugin classes.
2. Fuzzing coverage for payload parsers and config parsers.
3. Secrets handling validation (no sensitive data in logs/health payloads).
4. Cryptography and dependency review.

Exit gate:
- Security sign-off from platform/security owners.

### Phase E: Rollout and Operations
1. Progressive enablement: canary, staged rollout, production ramp.
2. Operational dashboards: auth latency, deny-rate, challenge completion, provider error rates.
3. Rollback switches per plugin and per method.
4. Runbook and on-call playbooks finalized.

Exit gate:
- Stable production operation through rollout window and post-rollout SLO period.

### Phase F: Enterprise Provider Driver Cutover (Continuation)
Plugins:
- `ident`, `radius`, `pam`, `ldap`, `kerberos`

Key work:
1. Replace synthetic test-directive execution paths as the primary runtime behavior with provider-driver-backed execution paths.
2. Keep synthetic directives available only for explicit test profiles and deny them by default in production policy profiles.
3. Add provider-driver adapters for enterprise plugins with deterministic error mapping to plugin ABI codes.
4. Add enterprise integration matrix coverage for provider outage, timeout, retry/failover, and principal/group mapping outcomes.

Exit gate:
- Enterprise plugin success/failure paths are driven by provider-driver responses for nominal deployments.
- Synthetic directive paths are test-profile only and blocked under production profiles.
- Enterprise provider integration matrix is green and documented.

## 3. Plugin-by-Plugin Completion Targets

| Plugin | Current State | Required Production Additions |
| --- | --- | --- |
| trust_reject | Functional baseline | Add policy/audit instrumentation and conformance tests. |
| peer | Functional baseline | Harden peer-credential trust model and OS-specific edge cases. |
| password_compat | Policy-gated baseline | Implement real credential verification backend and migration controls. |
| token_authkey | Input and subject mapping baseline | Implement token signature, expiry, issuer/audience validation. |
| certificate_mtls | Subject mapping baseline | Implement full certificate chain, SAN/CN policy, revocation checks. |
| jwt_oidc | JWT-shape baseline | Implement JOSE verification, claims policy, key rotation handling. |
| oauth_validator | Issuer/subject parse baseline | Implement token introspection or local validation with cache strategy. |
| proxy_assertion | Local/IPC gated baseline | Add trust boundary attestation and signed assertion verification. |
| workload_identity | OIDC/SPIFFE shape baseline | Implement full SPIFFE/OIDC trust bundle validation and rotation. |
| ident | Config + CIDR logic baseline | Add robust IDENT exchange implementation and spoofing mitigations. |
| radius | Config + policy baseline | Integrate real RADIUS client, retries, failover, message authenticators. |
| pam | Config + policy baseline | Integrate PAM conversation backend with secure prompt handling. |
| ldap | Config + policy baseline | Integrate LDAP bind/search, TLS policy enforcement, group mapping. |
| kerberos | Config + policy baseline | Integrate GSSAPI ticket verification, replay cache, keytab lifecycle. |
| scram | Challenge scaffold | Implement full SCRAM verifier with salted password store integration. |
| webauthn | Challenge scaffold | Implement authenticator data/signature verification and RP policy checks. |
| factor_chain | Chain-progress scaffold | Implement real factor orchestration and chain policy engine. |

## 4. Test and Quality Gates
1. Unit tests: method-specific success/failure cases per plugin.
2. Integration tests: end-to-end login flows per protocol and transport mode.
3. Security tests: replay, downgrade, malformed payload, brute-force rate limits.
4. Performance tests: auth latency p95/p99 and throughput targets.
5. Reliability tests: provider outage behavior, timeout handling, fallback policy.

Mandatory release gate:
- All plugin targets build.
- Full auth plugin test suite green.
- No high/critical security findings open.

## 5. Delivery Sequence and Resourcing
1. Sprint 1: Phase A + complete non-challenge low-complexity plugins (`trust_reject`, `peer`, `password_compat`, `token_authkey`, `proxy_assertion`).
2. Sprint 2: token/cert/workload (`certificate_mtls`, `jwt_oidc`, `oauth_validator`, `workload_identity`).
3. Sprint 3: enterprise providers (`ident`, `radius`, `pam`, `ldap`, `kerberos`).
4. Sprint 4: challenge plugins (`scram`, `webauthn`, `factor_chain`).
5. Sprint 5: security hardening, canary, production rollout.

## 6. Immediate Next Actions
1. Keep `auth_plugin_enterprise_matrix_gate.sh` in pre-release validation to preserve Phase F no-fail-open coverage.
2. Track any future enterprise plugin behavior expansion against the provider-driver decision helpers in `ident`, `radius`, `pam`, `ldap`, and `kerberos`.
3. Treat `AUTH_PLUGIN_CONTINUATION_READINESS_REVIEW_20260304.md` and `AUTH_PLUGIN_CONTINUATION_SIGNOFF_20260304.json` as the deployment checkpoint for Phase F closure.
