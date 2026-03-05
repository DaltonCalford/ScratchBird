# Authentication Plugins

[Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## Purpose

This sub-guide is the operator playbook for ScratchBird authentication plugins.
Each child document is plugin-specific and covers:

- What the plugin does and what it does not do.
- Exactly which ScratchBird knobs/policy keys must be set.
- How to connect to outside identity systems when the plugin depends on them.
- Verification, rollback, and common operational failures.

## Read This First (Beginner Baseline)

Before enabling any single plugin, set up the shared plugin-registry baseline:

1. Build plugin artifacts (for selftests and module packaging):
   ```bash
   cmake -S . -B build
   cmake --build build
   ```
2. Copy or create a trust store at `/etc/scratchbird/auth_plugin_truststore.jwks.json`.
   - Example source: `etc/auth/auth_plugin_truststore.jwks.json.example`.
3. Copy or create a plugin policy at `/etc/scratchbird/auth_plugins.policy.json`.
   - Example source: `etc/auth/auth_plugins.policy.json.example`.
4. Make sure your service bootstrap sets `AuthManagerConfig` with plugin registry enabled.
   - Required fields are in `include/scratchbird/security/auth_manager.h`:
     - `auth_plugin_registry_enabled`
     - `allow_legacy_auth_fallback`
     - `auth_plugin_truststore_path`
     - `auth_plugin_policy_path`
     - `auth_plugin_root` (for externally packaged signed plugin modules)
   - Minimal bootstrap example:
     ```cpp
     scratchbird::security::AuthManagerConfig cfg;
     cfg.auth_plugin_registry_enabled = true;
     cfg.allow_legacy_auth_fallback = false;
     cfg.auth_plugin_truststore_path = "/etc/scratchbird/auth_plugin_truststore.jwks.json";
     cfg.auth_plugin_policy_path = "/etc/scratchbird/auth_plugins.policy.json";
     cfg.auth_plugin_root = "/opt/scratchbird/auth_plugins";
     ```
5. For HBA-routed methods, update your HBA rules to use the method token (`ldap`, `gss`, `radius`, etc.).

For externally packaged signed modules (`auth_plugin_root` path), each plugin directory must contain:

- `manifest.json`
- `manifest.jws`
- module file referenced by manifest `module_path`

## Important Runtime Scope

Current `AuthManager` dispatch maps HBA/AuthType methods to this method-ID set:

- `scratchbird.auth.trust`
- `scratchbird.auth.reject`
- `scratchbird.auth.password_compat`
- `scratchbird.auth.md5_legacy`
- `scratchbird.auth.scram_sha_256`
- `scratchbird.auth.scram_sha_512`
- `scratchbird.auth.certificate_x509`
- `scratchbird.auth.ldap_bind`
- `scratchbird.auth.kerberos_gssapi`
- `scratchbird.auth.peer_uid`
- `scratchbird.auth.ident_rfc1413`
- `scratchbird.auth.radius_pap`
- `scratchbird.auth.pam_conversation`
- `scratchbird.auth.authkey_token`

The remaining plugins in this suite are fully documented because they are part of the enterprise plugin set and test/contract flows, but they are currently consumed through plugin ABI integration paths (not direct HBA method tokens).

## Plugin Guides

- [Plugin Registry Bootstrap (All Plugins)](00_plugin_registry_bootstrap.md)
- [Trust / Reject Plugin](01_trust_reject.md)
- [Password Compatibility Plugin](02_password_compat.md)
- [SCRAM Plugin](03_scram.md)
- [Token AuthKey Plugin](04_token_authkey.md)
- [Peer UID Plugin](05_peer.md)
- [Certificate mTLS Plugin](06_certificate_mtls.md)
- [JWT/OIDC Plugin](07_jwt_oidc.md)
- [WebAuthn Plugin](08_webauthn.md)
- [Factor Chain Plugin](09_factor_chain.md)
- [Workload Identity Plugin](10_workload_identity.md)
- [OAuth Validator Plugin](11_oauth_validator.md)
- [Proxy Assertion Plugin](12_proxy_assertion.md)
- [LDAP Plugin](13_ldap.md)
- [Kerberos Plugin](14_kerberos.md)
- [IDENT Plugin](15_ident.md)
- [RADIUS Plugin](16_radius.md)
- [PAM Plugin](17_pam.md)

## Recommended Validation Order

1. Validate policy/truststore load and required plugin admission.
2. Validate one local method (`peer` or `trust_reject`) in a non-production environment.
3. Validate credential-based methods (`password_compat`, `scram`, `token_authkey`).
4. Validate external-provider methods (`ldap`, `kerberos`, `radius`, `pam`, `ident`).
5. Validate advanced assertion plugins (`jwt_oidc`, `oauth_validator`, `proxy_assertion`, `workload_identity`, `webauthn`, `factor_chain`).
