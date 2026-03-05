# Workload Identity Plugin

[Prev](./09_factor_chain.md) | [Next](./11_oauth_validator.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to authenticate non-human workloads using OIDC workload tokens or SPIFFE assertions.

Method IDs:

- `scratchbird.auth.workload_oidc`
- `scratchbird.auth.workload_spiffe`

## Dispatch Path

Current direct `AuthManager` HBA routing does not select these method IDs directly.
Use via plugin-ABI integration paths.

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.workload_identity": {
  "required": true,
  "allowed_signers": ["sb-security-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.workload_oidc",
    "scratchbird.auth.workload_spiffe"
  ]
}
```

Set policy-value keys:

- `policy:auth.workload_identity.oidc_signing_key_ref`
- `policy:auth.workload_identity.spiffe_signing_key_ref`
- `auth.workload_identity.oidc_expected_issuer`
- `auth.workload_identity.oidc_expected_audience`
- `auth.workload_identity.oidc_trust_bundle`
- `auth.workload_identity.oidc_required_alg`
- `auth.workload_identity.oidc_subject_prefix`
- `auth.workload_identity.spiffe_trust_bundle`
- `auth.workload_identity.spiffe_subject_prefix`

## Step 2 - Payload Contracts

- OIDC workload method expects compact JWT-like token containing issuer/audience/subject/bundle/signature fields.
- SPIFFE method expects key/value assertion:
  - `spiffe=<spiffe://...>;bundle=<bundle-id>;exp=<unix_ms>;sig=<hex>`

## Step 3 - External Integration (Linux + Windows control planes)

1. Choose one trust anchor model per environment.
2. For OIDC workloads:
   - issue workload tokens from your workload IdP.
   - pin issuer/audience and trust-bundle identifiers in policy.
3. For SPIFFE workloads:
   - issue SPIFFE IDs with stable prefixes.
   - ensure bundle identifiers match policy exactly.
4. Keep workload subject mapping in `resolve_user_by_external_subject`.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_workload_identity_selftest --output-on-failure
```

Validate both OIDC and SPIFFE positive/negative cases.

## Common Errors

- `AUTH_WORKLOAD_ISSUER_MISMATCH`
- `AUTH_WORKLOAD_AUDIENCE_MISMATCH`
- `AUTH_WORKLOAD_TRUST_BUNDLE_INVALID`
- `AUTH_WORKLOAD_SUBJECT_POLICY_MISMATCH`
- `AUTH_WORKLOAD_SIGNATURE_INVALID`

## Rollback

1. Disable workload-identity call path and fall back to service-account SCRAM/token route.
2. Rotate workload trust bundle/signing keys if compromise is suspected.
3. Re-enable after full staging validation.

## Completion Checklist

- [ ] OIDC and/or SPIFFE policy keys set.
- [ ] Trust-bundle values pinned.
- [ ] Subject prefix rules validated.
- [ ] External subject mapping tested.
