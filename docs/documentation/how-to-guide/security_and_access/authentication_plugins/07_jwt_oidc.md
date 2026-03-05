# JWT/OIDC Plugin

[Prev](./06_certificate_mtls.md) | [Next](./08_webauthn.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

This plugin validates compact JWT/OIDC ID tokens and maps external subjects to ScratchBird principals.

Method IDs:

- `scratchbird.auth.jwt_bearer`
- `scratchbird.auth.oidc_id_token`

## Dispatch Path

Current direct `AuthManager` HBA routing does not select these two method IDs directly.
Use this plugin through plugin-ABI integration paths (contract harness/custom auth orchestration).

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.jwt_oidc": {
  "required": true,
  "allowed_signers": ["sb-security-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.jwt_bearer",
    "scratchbird.auth.oidc_id_token"
  ]
}
```

Set policy-value keys:

- `policy:auth.jwt_oidc.jwt_signing_key_ref`
- `policy:auth.jwt_oidc.oidc_signing_key_ref`
- `auth.jwt_oidc.jwt_expected_issuer`
- `auth.jwt_oidc.jwt_expected_audience`
- `auth.jwt_oidc.jwt_required_alg`
- `auth.jwt_oidc.oidc_expected_issuer`
- `auth.jwt_oidc.oidc_expected_audience`
- `auth.jwt_oidc.oidc_required_alg`

## Step 2 - Token Contract

Input must be compact JWT format (`header.payload.signature`).

Validation includes:

- required algorithm matches policy (`alg`)
- issuer matches policy (`iss`)
- audience matches policy (`aud`)
- expiry is in the future (`exp`)
- signature verifies with configured signing key reference

## Step 3 - External IdP Integration

Linux/Windows-neutral checklist:

1. Choose issuer identifier format (`iss`) and keep it immutable per environment.
2. Ensure token subject (`sub`) is stable and unique.
3. Configure audience exactly for ScratchBird consumer.
4. Export/rotate signing key into secret-provider storage.
5. Map (`iss`,`sub`) -> ScratchBird principal through `resolve_user_by_external_subject`.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_jwt_oidc_selftest --output-on-failure
```

Expect deny on malformed signature and wrong audience.

## Common Errors

- `AUTH_JWT_ALG_MISMATCH`
- `AUTH_JWT_ISSUER_MISMATCH`
- `AUTH_JWT_AUDIENCE_MISMATCH`
- `AUTH_JWT_SIGNATURE_INVALID`
- `AUTH_JWT_SUBJECT_UNKNOWN`

## Rollback

1. Disable call path that invokes JWT/OIDC plugin methods.
2. Route callers to `token_authkey` or SCRAM while IdP/token settings are corrected.
3. Re-enable after issuer/audience/signature tests pass in staging.

## Completion Checklist

- [ ] Both method IDs allowlisted in policy.
- [ ] JWT and OIDC signing-key refs set.
- [ ] Issuer/audience/algorithm policies set.
- [ ] External subject mapping verified.
