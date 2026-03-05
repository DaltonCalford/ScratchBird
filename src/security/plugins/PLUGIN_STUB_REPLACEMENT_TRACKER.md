# Auth Plugin Stub Replacement Tracker

Scope boundary:
- Only modify files under `src/security/plugins/**`.
- Do not edit shared auth manager/registry/core code.
- If a plugin requires non-plugin changes, stop and escalate before proceeding.

Execution model:
- Work strictly one plugin at a time.
- For each plugin: implement runtime behavior, build plugin target, record results, then move to the next plugin.

Status legend:
- `TODO`: not started
- `IN_PROGRESS`: currently being implemented
- `DONE`: implemented and plugin target verified
- `BLOCKED`: requires non-plugin change or external dependency

## Ordered plan

| Order | Plugin Directory | Plugin ID | Primary Methods | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| 1 | `trust_reject` | `scratchbird.auth.trust_reject` | `scratchbird.auth.trust`, `scratchbird.auth.reject` | `DONE` | Implemented runtime method dispatch (allow trust / deny reject). |
| 2 | `peer` | `scratchbird.auth.peer` | `scratchbird.auth.peer_uid` | `DONE` | Enforced local/IPC transport and required peer credential presence. |
| 3 | `token_authkey` | `scratchbird.auth.token_authkey` | `scratchbird.auth.authkey_token` | `DONE` | Added payload validation and host external-subject resolution gate. |
| 4 | `password_compat` | `scratchbird.auth.password_compat` | `scratchbird.auth.password_compat`, `scratchbird.auth.md5_legacy` | `DONE` | Added policy-gated compat flow and explicit MD5 legacy policy denial. |
| 5 | `scram` | `scratchbird.auth.scram` | `scratchbird.auth.scram_sha_256`, `scratchbird.auth.scram_sha_512` | `DONE` | Added stateful challenge/continue scaffold with tracked exchanges and fail-closed verifier path. |
| 6 | `certificate_mtls` | `scratchbird.auth.certificate_mtls` | `scratchbird.auth.certificate_x509` | `DONE` | Added certificate-subject validation and host external-subject resolution path. |
| 7 | `jwt_oidc` | `scratchbird.auth.jwt_oidc` | `scratchbird.auth.jwt_bearer`, `scratchbird.auth.oidc_id_token` | `DONE` | Added JWT-format checks and issuer-scoped external-subject resolution. |
| 8 | `oauth_validator` | `scratchbird.auth.oauth_validator` | `scratchbird.auth.oauth_bearer_validated` | `DONE` | Added validated-bearer issuer/subject parsing and resolver-backed identity mapping. |
| 9 | `proxy_assertion` | `scratchbird.auth.proxy_assertion` | `scratchbird.auth.proxy_principal_assertion` | `DONE` | Added local/IPC transport gating and proxy issuer/subject resolution. |
| 10 | `workload_identity` | `scratchbird.auth.workload_identity` | `scratchbird.auth.workload_oidc`, `scratchbird.auth.workload_spiffe` | `DONE` | Added OIDC/SPIFFE subject validation and resolver-backed workload mapping. |
| 11 | `factor_chain` | `scratchbird.auth.factor_chain` | `scratchbird.auth.factor_chain_2fa`, `scratchbird.auth.factor_chain_3fa` | `DONE` | Added tracked multi-step factor progress with explicit continuation semantics. |
| 12 | `webauthn` | `scratchbird.auth.webauthn` | `scratchbird.auth.webauthn_assertion` | `DONE` | Added challenge/assertion exchange state with deterministic fail-closed verification path. |
| 13 | `ldap` | `scratchbird.auth.ldap` | `scratchbird.auth.ldap_bind` | `DONE` | Integrated `loadLdapPluginConfig` with per-instance config state and runtime gates. |
| 14 | `kerberos` | `scratchbird.auth.kerberos` | `scratchbird.auth.kerberos_gssapi` | `DONE` | Integrated `loadKerberosPluginConfig` with per-instance config state and runtime gates. |
| 15 | `ident` | `scratchbird.auth.ident` | `scratchbird.auth.ident_rfc1413` | `DONE` | Integrated `loadIdentPluginConfig` and implemented CIDR/username-match runtime checks. |
| 16 | `radius` | `scratchbird.auth.radius` | `scratchbird.auth.radius_pap` | `DONE` | Integrated `loadRadiusPluginConfig` and implemented timeout/allowlist/reject gating. |
| 17 | `pam` | `scratchbird.auth.pam` | `scratchbird.auth.pam_conversation` | `DONE` | Integrated `loadPamPluginConfig` and implemented timeout/service/deny gating. |

## Verification log

| Plugin | Build target | Result | Date (UTC) | Notes |
| --- | --- | --- | --- | --- |
| `trust_reject` | `sb_auth_plugin_trust_reject` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_trust_reject -j8`. |
| `peer` | `sb_auth_plugin_peer` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_peer -j8`. |
| `token_authkey` | `sb_auth_plugin_token_authkey` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_token_authkey -j8`. |
| `password_compat` | `sb_auth_plugin_password_compat` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_password_compat -j8`. |
| `scram` | `sb_auth_plugin_scram` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_scram -j8`. |
| `certificate_mtls` | `sb_auth_plugin_certificate_mtls` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_certificate_mtls -j8`. |
| `jwt_oidc` | `sb_auth_plugin_jwt_oidc` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_jwt_oidc -j8`. |
| `oauth_validator` | `sb_auth_plugin_oauth_validator` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_oauth_validator -j8`. |
| `proxy_assertion` | `sb_auth_plugin_proxy_assertion` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_proxy_assertion -j8`. |
| `workload_identity` | `sb_auth_plugin_workload_identity` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_workload_identity -j8`. |
| `factor_chain` | `sb_auth_plugin_factor_chain` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_factor_chain -j8`. |
| `webauthn` | `sb_auth_plugin_webauthn` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_webauthn -j8`. |
| `ldap` | `sb_auth_plugin_ldap` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_ldap -j8`. |
| `kerberos` | `sb_auth_plugin_kerberos` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_kerberos -j8`. |
| `ident` | `sb_auth_plugin_ident` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_ident -j8`. |
| `radius` | `sb_auth_plugin_radius` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_radius -j8`. |
| `pam` | `sb_auth_plugin_pam` | `PASS` | `2026-03-04` | Built with `cmake --build build --target sb_auth_plugin_pam -j8`. |
| `all-auth-plugins` | `sb_auth_plugin_*` (17 targets) | `PASS` | `2026-03-04` | Final sweep with a single `cmake --build` invocation listing all plugin targets. |
