# Trust / Reject Plugin

[Prev](./00_plugin_registry_bootstrap.md) | [Next](./02_password_compat.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to control "allow immediately" (`trust`) and "deny immediately" (`reject`) behavior from one audited policy surface.

- `scratchbird.auth.trust` allows login with no credential challenge.
- `scratchbird.auth.reject` denies login unconditionally.

## Dispatch Path

This plugin is directly selectable through HBA/auth method tokens:

- `trust` -> `scratchbird.auth.trust`
- `reject` -> `scratchbird.auth.reject`

## Step 1 - Policy Configuration

Add/verify this entry in `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.trust_reject": {
  "required": true,
  "allowed_signers": ["sb-release-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.trust",
    "scratchbird.auth.reject"
  ]
}
```

Set trust behavior switch in your policy-value store:

- `auth.trust_reject.trust_enabled = true|false`
- If `false`, any `trust` attempt is denied with `AUTH_TRUST_REJECT_TRUST_DISABLED`.

## Step 2 - HBA Routing

Example HBA rules:

```conf
# Local admin sockets only
local   all             scratch_admin                            trust

# Deny all direct remote logins to sensitive DB
host    finance_prod    all             0.0.0.0/0               reject
```

Safety recommendation:

- Restrict `trust` to local-only bootstrap windows.
- Keep a separate break-glass admin path before changing production rules.

## Step 3 - Request Payload Expectations

- `trust` accepts empty payload or optional metadata payload: `reason=<text>;request_id=<id>`.
- Unknown keys or malformed payload return `AUTH_TRUST_REJECT_PAYLOAD_INVALID`.
- `reject` always denies.

## External Authenticator Integration

None. This plugin is internal control logic only.

## Verification

1. Validate plugin selftest:
   ```bash
   ctest --test-dir build -R sb_auth_plugin_trust_reject_selftest --output-on-failure
   ```
2. Confirm policy key behavior by toggling `auth.trust_reject.trust_enabled`.
3. Confirm expected deny codes in logs:
   - `AUTH_TRUST_REJECT_FORCED_DENY`
   - `AUTH_TRUST_REJECT_TRUST_DISABLED`

## Common Errors

- `AUTH_TRUST_REJECT_METHOD_UNKNOWN`: HBA method token does not map to trust/reject correctly.
- `AUTH_TRUST_REJECT_PAYLOAD_INVALID`: malformed metadata payload.
- Startup `required plugin set missing`: policy marks plugin required but registry admission failed.

## Rollback

1. Replace `trust` HBA rules with `scram-sha-256` or your standard credential method.
2. Set `auth.trust_reject.trust_enabled=false`.
3. Reload/restart service and validate that trust logins no longer succeed.

## Completion Checklist

- [ ] Policy allowlist entry present.
- [ ] `auth.trust_reject.trust_enabled` explicitly set.
- [ ] HBA rules validated in staging.
- [ ] Selftest and log validation completed.
