# Password Compatibility Plugin

[Prev](./01_trust_reject.md) | [Next](./03_scram.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin when you must support legacy password-style clients while migrating toward stronger authentication.

- Method IDs:
  - `scratchbird.auth.password_compat`
  - `scratchbird.auth.md5_legacy`

## Dispatch Path

This plugin is directly selectable through HBA/auth method tokens:

- `password` -> `scratchbird.auth.password_compat`
- `md5` -> `scratchbird.auth.md5_legacy`

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.password_compat": {
  "required": true,
  "allowed_signers": ["sb-release-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.password_compat",
    "scratchbird.auth.md5_legacy"
  ]
}
```

Set policy-value keys:

- `auth.password_compat.mode` (migration mode gate)
- `auth.password_compat.allow_md5_legacy=true|false`
- `auth.password_compat.credential_ref.<username>=<secret-ref>`
- Optional fallback: `auth.password_compat.default_credential_ref=<secret-ref>`

Credential refs are resolved through the host secret-provider interface (do not hardcode clear-text credentials in policy files).

## Step 2 - HBA Routing

Example staged migration rules:

```conf
# Legacy app subnet still using password/MD5
host    appdb           legacy_app      10.40.12.0/24          password

# Force reject MD5 globally after migration cutover
host    all             all             0.0.0.0/0              md5
```

Migration pattern:

1. Start with `password` only.
2. Keep `auth.password_compat.allow_md5_legacy=false` unless you have a hard dependency.
3. Move clients to SCRAM and delete `password`/`md5` HBA entries.

## Step 3 - Request Payload

Client payload is a single password secret blob (no key/value envelope).

Common deny outcomes:

- `AUTH_PASSWORD_CREDENTIAL_REF_MISSING`
- `AUTH_PASSWORD_CREDENTIAL_RESOLVE_FAILED`
- `AUTH_PASSWORD_CREDENTIAL_MISMATCH`
- `AUTH_PASSWORD_MD5_LEGACY_DENIED`

## External Authenticator Integration

None. This plugin validates credentials against ScratchBird-resolved secret references.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_password_compat_selftest --output-on-failure
```

Check logs and ensure MD5 attempts are denied when legacy mode is disabled.

## Common Errors

- `AUTH_PASSWORD_USERNAME_MISSING`: upstream context did not set username.
- `AUTH_PASSWORD_COMPAT_DISABLED`: mode gate blocks password path.
- `AUTH_PASSWORD_MD5_LEGACY_DENIED`: MD5 login attempted when legacy toggle is off.

## Rollback

1. Remove `password`/`md5` HBA routes.
2. Switch callers to SCRAM routes only.
3. Remove `auth.password_compat.credential_ref.*` keys from policy store.

## Completion Checklist

- [ ] Policy allowlist entry present.
- [ ] Per-user credential refs configured.
- [ ] MD5 legacy toggle explicitly set.
- [ ] HBA migration sequence documented and tested.
