# SCRAM Plugin

[Prev](./02_password_compat.md) | [Next](./04_token_authkey.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin for modern password authentication with challenge-response semantics.

- `scratchbird.auth.scram_sha_256`
- `scratchbird.auth.scram_sha_512`

## Dispatch Path

This plugin is directly selectable through HBA/auth method tokens:

- `scram-sha-256`
- `scram-sha-512`

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.scram": {
  "required": true,
  "allowed_signers": ["sb-release-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.scram_sha_256",
    "scratchbird.auth.scram_sha_512"
  ]
}
```

Set policy-value keys for credential material references:

- `auth.scram.credential_ref.<username>=<secret-ref>`
- Optional fallback: `auth.scram.default_credential_ref=<secret-ref>`

Expected secret shape includes fields required by SCRAM verification (for example `salt`, `secret`, `iter`).

## Step 2 - HBA Routing

```conf
host    all             all             0.0.0.0/0              scram-sha-256
```

Use `scram-sha-512` only if your client fleet fully supports it.

## Step 3 - SCRAM Flow (Client/Server)

1. Client first payload (begin):
   - `user=<username>;cnonce=<client-nonce>`
2. Server challenge (continue required):
   - includes server nonce and salt metadata.
3. Client proof payload (continue):
   - `proof=<proof-hex>;nonce=<combined-nonce>`
4. Success returns resolved principal and assurance level.

Deny examples:

- `AUTH_SCRAM_CLIENT_FIRST_MALFORMED`
- `AUTH_SCRAM_PROOF_INVALID`
- `AUTH_SCRAM_REPLAY_DETECTED`
- `AUTH_SCRAM_CREDENTIAL_UNAVAILABLE`

## External Authenticator Integration

None. SCRAM credentials are resolved through ScratchBird secret references.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_scram_selftest --output-on-failure
```

Validate both positive and negative flows (valid proof, invalid proof, replay, exchange expiry).

## Common Errors

- `AUTH_SCRAM_UNKNOWN_EXCHANGE`: client continued with stale/invalid exchange ID.
- `AUTH_SCRAM_EXCHANGE_EXPIRED`: continuation arrived after exchange TTL.
- `AUTH_SCRAM_USER_UNKNOWN`: resolver could not map username.

## Rollback

1. Keep password_compat as temporary fallback only if business-critical.
2. Revert HBA entry from SCRAM only if migration blocker is confirmed.
3. Document reason and cutover date before restoring weaker method.

## Completion Checklist

- [ ] SCRAM policy allowlist entry present.
- [ ] User credential refs populated.
- [ ] HBA routes set to SCRAM.
- [ ] Replay/expiry failure modes tested.
