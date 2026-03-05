# Auth Plugin Crypto and Dependency Review

Date: 2026-03-04  
Ticket: `AUTH-PROD-D04`  
Acceptance targets: `AT-D04-01`, `AT-D04-02`

## Scope
- `src/security/plugins` auth plugin modules and plugin-local selftests.
- Crypto primitives used for token/signature/proof validation paths.
- Build/link dependency posture for crypto libraries.

## Inventory Summary

### 1. Signature and proof verification primitives
- Shared helper: `auth_plugin_crypto.h`
  - `hmacSha256Hex(key, message, out_hex)` implemented with OpenSSL `EVP_MAC` HMAC-SHA256 on OpenSSL 3, with legacy `HMAC_*` fallback for pre-3.0.
- Plugins using `hmacSha256Hex` in production auth paths:
  - `token_authkey`
  - `jwt_oidc`
  - `oauth_validator`
  - `proxy_assertion`
  - `radius`
  - `scram`
  - `kerberos`
  - `workload_identity`
  - `webauthn`
  - `factor_chain`

### 2. Randomness primitives
- Challenge plugins use host API secure RNG (`secure_random`) for nonce/challenge generation:
  - `scram`
  - `webauthn`
  - `factor_chain`

### 3. Non-crypto auth plugins
- `trust_reject`, `peer`, `password_compat`, `certificate_mtls`, `ident`, `pam`, `ldap` do not add custom hashing/signing primitives in plugin code paths.

## Weak/legacy primitive review
- `fnv1a64` scan in `src/security/plugins` source: no matches.
- SHA-1 usage scan: no matches in plugin source.
- MD5 usage:
  - Present only as migration method id in `password_compat` (`scratchbird.auth.md5_legacy`).
  - Default behavior is deny unless policy explicitly enables (`auth.password_compat.allow_md5_legacy`).
  - Classified as controlled compatibility exception, not an active default primitive.

## Dependency posture
- OpenSSL is configured in core build:
  - `src/CMakeLists.txt` includes `find_package(OpenSSL)` and links `OpenSSL::Crypto`.
- Auth plugin modules and crypto-dependent selftests/harness are explicitly linked to `OpenSSL::Crypto` in:
  - `src/security/plugins/CMakeLists.txt`
- Local environment validation:
  - `openssl version` -> `OpenSSL 3.0.13 30 Jan 2024`

## Validation evidence
- Reconfigure/build passed for auth plugin modules and crypto-dependent selftests/harness.
- Full auth plugin test sweep passed:
  - `ctest --test-dir build -R 'sb_auth_plugin_.*(selftest|contract_harness)' --output-on-failure`
  - Result: 22 passed, 0 failed.

## Findings
- Critical findings: none open.
- High findings: none open.
- Medium findings:
  - `password_compat` includes policy-gated md5 legacy migration path; remains disabled by default and tracked as compatibility-only control.
- Low findings:
  - Pre-OpenSSL-3 fallback uses legacy `HMAC_*` API path for backward compatibility; OpenSSL-3 path uses `EVP_MAC`.

## Conclusion
`AT-D04-01` satisfied with documented crypto inventory and primitive review.  
`AT-D04-02` satisfied with no open critical crypto/dependency findings in current plugin scope.
