# Proxy Assertion Plugin

[Prev](./11_oauth_validator.md) | [Next](./13_ldap.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin when a trusted authentication proxy terminates user auth and forwards a signed principal assertion to ScratchBird.

Method ID:

- `scratchbird.auth.proxy_principal_assertion`

## Dispatch Path

Current direct `AuthManager` HBA routing does not select this method ID directly.
Use via plugin-ABI integration paths.

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.proxy_assertion": {
  "required": true,
  "allowed_signers": ["sb-security-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.proxy_principal_assertion"
  ]
}
```

Set policy-value keys:

- `policy:auth.proxy_assertion.signing_key_ref`
- `auth.proxy_assertion.allow_inet=true|false`
- `auth.proxy_assertion.trusted_proxy_uid=<uid>`
- `auth.proxy_assertion.expected_issuer=<issuer>`
- `auth.proxy_assertion.required_audience=<aud>`
- `auth.proxy_assertion.expected_proxy_id=<proxy-id>`

## Step 2 - Assertion Contract

Expected payload:

- `iss=<issuer>`
- `sub=<subject>`
- `aud=<audience>`
- `proxy=<proxy_id>`
- `exp=<unix_ms>`
- `sig=<hex>`

The plugin denies if proxy UID is untrusted, issuer/audience/proxy mismatch, assertion expired, or signature invalid.

## Step 3 - Proxy Integration (Linux + Windows)

1. Run proxy under dedicated service account (stable UID on Linux).
2. Pin proxy identity in ScratchBird policy (`trusted_proxy_uid`, `expected_proxy_id`).
3. Sign assertions with managed key material and rotate periodically.
4. Set `SCRATCHBIRD_AUTH_NO_LOGIN_DIRECT=1` to enforce proxy-only login policy.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_proxy_assertion_selftest --output-on-failure
```

Also validate direct-login blocking behavior in `AuthManager` by setting `SCRATCHBIRD_AUTH_NO_LOGIN_DIRECT`.

## Common Errors

- `AUTH_PROXY_ASSERTION_UNTRUSTED_PROXY`
- `AUTH_PROXY_ASSERTION_ISSUER_MISMATCH`
- `AUTH_PROXY_ASSERTION_AUDIENCE_MISMATCH`
- `AUTH_PROXY_ASSERTION_PROXY_ID_MISMATCH`
- `AUTH_PROXY_ASSERTION_SIGNATURE_INVALID`

## Rollback

1. Temporarily clear proxy-only login guard only during controlled incident response.
2. Rotate proxy signing key and re-pin policy values.
3. Re-enable strict proxy-only mode after validation.

## Completion Checklist

- [ ] Trusted proxy UID configured.
- [ ] Issuer/audience/proxy-id pinned.
- [ ] Proxy signing key reference configured.
- [ ] Direct-login guard behavior validated.
