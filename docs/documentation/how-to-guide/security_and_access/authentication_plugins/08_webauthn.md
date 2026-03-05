# WebAuthn Plugin

[Prev](./07_jwt_oidc.md) | [Next](./09_factor_chain.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin for hardware-backed WebAuthn assertion validation (passkeys/security keys).

Method ID:

- `scratchbird.auth.webauthn_assertion`

## Dispatch Path

Current direct `AuthManager` HBA routing does not select this method ID directly.
Use via plugin-ABI integration paths.

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.webauthn": {
  "required": true,
  "allowed_signers": ["sb-security-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.webauthn_assertion"
  ]
}
```

Set policy-value keys:

- `policy:auth.webauthn.signing_key_ref`
- `auth.webauthn.rp_id`
- `auth.webauthn.allowed_origin`
- `auth.webauthn.require_uv=true|false`

## Step 2 - Two-Step Flow

1. Begin payload:
   - `user=<username>;cred=<credential_id>`
2. Plugin returns challenge and exchange handle.
3. Continue payload with signed assertion:
   - `challenge=<hex>;rp=<rp_id>;origin=<https-origin>;cred=<credential_id>;sub=<external-subject>;uv=<true|false>;exp=<unix_ms>;sig=<hex>`

Deny conditions include challenge mismatch, replay, wrong RP/origin, expired assertion, signature failure, and missing UV when required.

## Step 3 - External Authenticator Integration

Beginner path:

1. Register passkeys with your identity front-end.
2. Ensure RP ID and origin in that front-end exactly match ScratchBird policy values.
3. Store signing key material in your secret-provider backend.
4. Map WebAuthn subject (`sub`) to ScratchBird user principal.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_webauthn_selftest --output-on-failure
```

Confirm both success and replay-deny behavior.

## Common Errors

- `AUTH_WEBAUTHN_RP_MISMATCH`
- `AUTH_WEBAUTHN_ORIGIN_MISMATCH`
- `AUTH_WEBAUTHN_UV_REQUIRED`
- `AUTH_WEBAUTHN_SIGNATURE_INVALID`
- `AUTH_WEBAUTHN_REPLAY_DETECTED`

## Rollback

1. Remove WebAuthn method from invoking auth chain.
2. Temporarily fallback to SCRAM or token flow.
3. Restore WebAuthn after RP/origin and signing key alignment is revalidated.

## Completion Checklist

- [ ] RP ID and origin policy keys set.
- [ ] UV requirement explicitly configured.
- [ ] Challenge/continue lifecycle tested.
- [ ] Replay and expiry protections verified.
