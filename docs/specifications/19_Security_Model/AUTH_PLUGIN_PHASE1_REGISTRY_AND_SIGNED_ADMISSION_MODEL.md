# Auth Plugin Phase 1 Registry and Signed Admission Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current ScratchBird auth-plugin runtime that is already visible in code and tests:
- built-in phase-1 auth plugin registry
- canonical method identifiers
- signed admission and trusted-signer policy model
- auth-type to plugin-method binding
- selected manager-level pinning and direct-login refusal rules

## Canonical runtime role

The auth plugin manager is the admission and registry authority for plugin-backed authentication methods.

It is responsible for:
- loading trusted-signer policy
- admitting or rejecting plugins
- exposing canonical plugin method identifiers
- resolving legacy auth-type selections into canonical plugin methods
- preserving downgrade compatibility through optional legacy wire codes

## Current built-in phase-1 plugin registry

The current built-in phase-1 plugin registry contains these plugin identifiers:

- `scratchbird.auth.trust_reject`
- `scratchbird.auth.password_compat`
- `scratchbird.auth.scram`
- `scratchbird.auth.token_authkey`
- `scratchbird.auth.peer`
- `scratchbird.auth.certificate_mtls`
- `scratchbird.auth.jwt_oidc`
- `scratchbird.auth.webauthn`
- `scratchbird.auth.factor_chain`
- `scratchbird.auth.workload_identity`
- `scratchbird.auth.oauth_validator`
- `scratchbird.auth.proxy_assertion`
- `scratchbird.auth.ldap`
- `scratchbird.auth.kerberos`
- `scratchbird.auth.ident`
- `scratchbird.auth.radius`
- `scratchbird.auth.pam`

Current code-backed admission tests require this built-in phase-1 set to exist and initialize cleanly.

## Canonical method identifiers by built-in plugin

The current built-in method registry includes at minimum:

- `scratchbird.auth.trust_reject`
  - `scratchbird.auth.trust`
  - `scratchbird.auth.reject`
- `scratchbird.auth.password_compat`
  - `scratchbird.auth.password_compat`
  - `scratchbird.auth.md5_legacy`
- `scratchbird.auth.scram`
  - `scratchbird.auth.scram_sha_256`
  - `scratchbird.auth.scram_sha_512`
- `scratchbird.auth.token_authkey`
  - `scratchbird.auth.authkey_token`
- `scratchbird.auth.peer`
  - `scratchbird.auth.peer_uid`
- `scratchbird.auth.certificate_mtls`
  - `scratchbird.auth.certificate_x509`
- `scratchbird.auth.jwt_oidc`
  - `scratchbird.auth.jwt_bearer`
  - `scratchbird.auth.oidc_id_token`
- `scratchbird.auth.webauthn`
  - `scratchbird.auth.webauthn_assertion`
- `scratchbird.auth.factor_chain`
  - `scratchbird.auth.factor_chain_2fa`
  - `scratchbird.auth.factor_chain_3fa`
- `scratchbird.auth.workload_identity`
  - `scratchbird.auth.workload_oidc`
  - `scratchbird.auth.workload_spiffe`
- `scratchbird.auth.oauth_validator`
  - `scratchbird.auth.oauth_bearer_validated`
- `scratchbird.auth.proxy_assertion`
  - `scratchbird.auth.proxy_principal_assertion`
- `scratchbird.auth.ldap`
  - `scratchbird.auth.ldap_bind`
- `scratchbird.auth.kerberos`
  - `scratchbird.auth.kerberos_gssapi`
- `scratchbird.auth.ident`
  - `scratchbird.auth.ident_rfc1413`
- `scratchbird.auth.radius`
  - `scratchbird.auth.radius_pap`
- `scratchbird.auth.pam`
  - `scratchbird.auth.pam_conversation`

These method identifiers are canonical security-contract identifiers. They must not be replaced with dialect-specific aliases in the runtime registry.

## Signed admission and trust policy model

Current code-backed plugin admission includes:
- a default trust-store path
- a default auth-plugin policy path
- canonical JSON handling for manifest and policy material
- signer trust validation
- plugin module digest validation
- required symbol binding for the plugin ABI

Current built-in defaults recovered from code:
- trust store path:
  - `/etc/scratchbird/auth_plugin_truststore.jwks.json`
- policy path:
  - `/etc/scratchbird/auth_plugins.policy.json`
- required exported API symbol:
  - `sb_auth_plugin_get_api_v1`

The signed-admission model is fail-closed:
- an untrusted signer is rejection authority
- missing required plugins are rejection authority
- policy disallow lists or method disallow lists are rejection authority
- malformed manifest or signature envelope is rejection authority

Deterministic signer rejection is already part of the tested runtime and is canonical behavior.

## Auth-type to plugin-method binding

The current runtime binds legacy or coarse auth-type selections into canonical plugin methods.

Current code-backed required bindings include:
- `TOKEN` -> `scratchbird.auth.authkey_token`
- `LDAP` -> `scratchbird.auth.ldap_bind`
- `KERBEROS` -> `scratchbird.auth.kerberos_gssapi`
- `IDENT` -> `scratchbird.auth.ident_rfc1413`
- `RADIUS` -> `scratchbird.auth.radius_pap`
- `PAM` -> `scratchbird.auth.pam_conversation`
- `PEER` -> `scratchbird.auth.peer_uid`

The runtime may also expose older wire-level auth method enums for downgrade compatibility, but canonical policy and registry identity remain `scratchbird.auth.*` method identifiers.

## Plugin connection context model

The current plugin-connection context assembled for plugin-backed auth flows includes at minimum:
- username
- client address
- server address
- transport class
- connection flags
- peer UID
- peer GID
- optional peer PID when surfaced through session properties

Transport class is normalized from connection metadata into:
- local transport
- IPC transport
- inet transport

Plugin-backed methods must evaluate their policy against this normalized connection context, not against ad hoc per-adapter assumptions.

## Direct-login refusal and pinning rules

Current tested runtime behavior already proves two important admission controls:

- required-method pinning:
  - if the selected auth path is not in the required set, authentication must fail with an `AUTH_CLIENT_PINNING_VIOLATION` failure path
- direct-login refusal:
  - when direct login is disabled and proxy assertion is not verified, authentication must fail with an `AUTH_NO_LOGIN_DIRECT` failure path

These controls are canonical admission rules and must remain fail-closed.

## Current-versus-required split

Current code-backed authority is strongest for:
- built-in plugin registry identity
- method identifier registry
- signed admission structure
- deterministic plugin rejection
- auth-type to canonical method binding

Required reconstructed canon extends this into a full commercial-grade model for:
- clustered plugin rollout coordination
- shared policy propagation across managed deployments
- richer enterprise provider negotiation and MFA chaining policy publication

## Non-authority

This file does not claim that every possible external provider is already production-complete.
It defines the current runtime registry and the canonical admission model implementation must satisfy.
