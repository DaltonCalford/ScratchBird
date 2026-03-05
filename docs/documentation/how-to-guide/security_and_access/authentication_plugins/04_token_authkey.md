# Token AuthKey Plugin

[Prev](./03_scram.md) | [Next](./05_peer.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to validate compact signed bearer tokens against policy-controlled issuer/audience/signing key references.

Method ID:

- `scratchbird.auth.authkey_token`

## Dispatch Path

This plugin is directly selectable through HBA/auth method tokens:

- `token`
- `oauth`
- `oidc`

All three tokens currently map to the same auth method ID (`scratchbird.auth.authkey_token`).

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.token_authkey": {
  "required": true,
  "allowed_signers": ["sb-release-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.authkey_token"
  ]
}
```

Set policy-value keys:

- `policy:auth.token_authkey.signing_key_ref` (required)
- `auth.token_authkey.expected_issuer` (default: `scratchbird.auth.authkey`)
- `auth.token_authkey.expected_audience` (default: `scratchbird`)

## Step 2 - HBA Routing

```conf
hostssl all             all             0.0.0.0/0              token
```

Use TLS (`hostssl`) because tokens are bearer credentials.

## Step 3 - Token Payload Contract

Expected key/value payload:

- `iss=<issuer>`
- `aud=<audience>`
- `sub=<subject>`
- `exp=<unix_ms>`
- `sig=<hex_hmac_signature>`

Unknown fields, duplicate keys, missing keys, malformed signatures, or expired values deny access.

## Step 4 - External Authenticator Integration

Typical pattern:

1. External IdP/token-service issues tokens with stable `sub` identifier.
2. ScratchBird validates `iss`, `aud`, `exp`, and `sig`.
3. ScratchBird calls `resolve_user_by_external_subject` to map (`iss`,`sub`) to local principal UUID.

Beginner checklist:

- Keep signing-key rotation in your secret manager.
- Version your token audience values by environment (`scratchbird-prod`, `scratchbird-staging`).
- Keep issuer strings exact and case-consistent.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_token_authkey_selftest --output-on-failure
```

Check deny-paths for:

- `AUTH_TOKEN_ISSUER_MISMATCH`
- `AUTH_TOKEN_AUDIENCE_MISMATCH`
- `AUTH_TOKEN_EXPIRED`
- `AUTH_TOKEN_SIGNATURE_INVALID`

## Common Errors

- `AUTH_TOKEN_SIGNING_KEY_UNAVAILABLE`: secret reference not resolvable.
- `AUTH_TOKEN_SUBJECT_UNKNOWN`: external subject could not be mapped to local user.
- `AUTH_TOKEN_MALFORMED`: token payload not in required key/value schema.

## Rollback

1. Temporarily route affected clients to SCRAM while token pipeline is fixed.
2. Revert only HBA route change; do not delete token policy keys until incident closure.
3. Re-enable token route after issuer/audience/signature checks pass in staging.

## Completion Checklist

- [ ] Issuer/audience policy keys set.
- [ ] Signing-key reference resolvable.
- [ ] External subject mapping table validated.
- [ ] Token expiration and signature failure tests passed.
