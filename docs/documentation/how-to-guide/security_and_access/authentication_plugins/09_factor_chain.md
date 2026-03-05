# Factor Chain Plugin

[Prev](./08_webauthn.md) | [Next](./10_workload_identity.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to enforce ordered multi-factor authentication (2FA/3FA) with signed factor submissions.

Method IDs:

- `scratchbird.auth.factor_chain_2fa`
- `scratchbird.auth.factor_chain_3fa`

## Dispatch Path

Current direct `AuthManager` HBA routing does not select these method IDs directly.
Use via plugin-ABI integration paths.

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.factor_chain": {
  "required": true,
  "allowed_signers": ["sb-security-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.factor_chain_2fa",
    "scratchbird.auth.factor_chain_3fa"
  ]
}
```

Set policy-value keys:

- `policy:auth.factor_chain.signing_key_ref`
- `auth.factor_chain.2fa.sequence` (for example `password,otp`)
- `auth.factor_chain.3fa.sequence` (for example `password,otp,webauthn`)
- `auth.factor_chain.require_distinct_factors=true|false`

## Step 2 - Exchange Flow

1. Begin payload:
   - `user=<username>;sub=<external-subject>`
2. Plugin returns first challenge with nonce.
3. Client submits factor proof:
   - `factor=<name>;status=ok;sub=<subject>;nonce=<nonce>;proof=<hex>`
4. Repeat until configured factor sequence is complete.

Denies occur on out-of-order factor, nonce mismatch, duplicate/replayed factor step, invalid proof, or subject mismatch.

## Step 3 - External Integration

Typical pattern:

- Upstream identity service orchestrates factor UX.
- ScratchBird validates signed factor submissions and ordering.
- ScratchBird maps external subject to internal principal.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_factor_chain_selftest --output-on-failure
```

Check both positive 2FA path and factor-order denial path.

## Common Errors

- `AUTH_FACTOR_CHAIN_POLICY_INVALID`
- `AUTH_FACTOR_CHAIN_FACTOR_ORDER`
- `AUTH_FACTOR_CHAIN_NONCE_MISMATCH`
- `AUTH_FACTOR_CHAIN_PROOF_INVALID`
- `AUTH_FACTOR_CHAIN_REPLAY_DETECTED`

## Rollback

1. Remove factor-chain invocation from auth pipeline.
2. Revert to single-factor method temporarily.
3. Re-enable only after sequence and key material are corrected.

## Completion Checklist

- [ ] 2FA/3FA sequences configured.
- [ ] Distinct-factor policy decided.
- [ ] Signing key reference set.
- [ ] Replay/order failure tests passed.
