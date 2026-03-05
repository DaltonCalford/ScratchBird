# OAuth Validator Plugin

[Prev](./10_workload_identity.md) | [Next](./12_proxy_assertion.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to validate OAuth-style bearer tokens with issuer, scope, revocation, and signature checks.

Method ID:

- `scratchbird.auth.oauth_bearer_validated`

## Dispatch Path

Current direct `AuthManager` HBA routing does not select this method ID directly.
Use via plugin-ABI integration paths.

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.oauth_validator": {
  "required": true,
  "allowed_signers": ["sb-security-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.oauth_bearer_validated"
  ]
}
```

Set policy-value keys:

- `policy:auth.oauth_validator.signing_key_ref`
- `auth.oauth_validator.expected_issuer`
- `auth.oauth_validator.required_scope`
- `auth.oauth_validator.cache_ttl_ms`
- `auth.oauth_validator.revoked_cache_ttl_ms`

## Step 2 - Token Contract

Expected key/value payload:

- `iss=<issuer>`
- `sub=<subject>`
- `aud=<audience>`
- `scope=<space_or_comma_scopes>`
- `jti=<token_id>`
- `exp=<unix_ms>`
- `active=<true|false>`
- `sig=<hex>`

Revoked/inactive tokens deny even when signature is valid.

## Step 3 - External OAuth Integration

1. Align issuer and audience between OAuth server and ScratchBird policy.
2. Standardize scope naming (`db.read`, `db.admin`, etc.) and set required scope.
3. Ensure token IDs (`jti`) are unique for revocation cache correctness.
4. Keep signing-key reference synchronized with OAuth key rotation.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_oauth_validator_selftest --output-on-failure
```

Validate:

- active token allow
- revoked token deny
- revoked-cache TTL behavior

## Common Errors

- `AUTH_OAUTH_VALIDATOR_ISSUER_MISMATCH`
- `AUTH_OAUTH_VALIDATOR_SCOPE_MISMATCH`
- `AUTH_OAUTH_VALIDATOR_TOKEN_REVOKED`
- `AUTH_OAUTH_VALIDATOR_SIGNATURE_INVALID`

## Rollback

1. Disable OAuth validator invocation path.
2. Route users to SCRAM or token_authkey fallback until OAuth revocation/signing pipeline is fixed.
3. Re-enable only after revocation cache tests pass.

## Completion Checklist

- [ ] Required scope configured.
- [ ] Revocation cache TTLs configured.
- [ ] Signing-key ref configured and tested.
- [ ] Revoked-token denial confirmed.
