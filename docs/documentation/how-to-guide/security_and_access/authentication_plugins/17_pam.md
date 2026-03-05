# PAM Plugin

[Prev](./16_radius.md) | [Next](./README.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to delegate authentication decisions to the host PAM stack.

Method ID:

- `scratchbird.auth.pam_conversation`

## Dispatch Path

Direct HBA/auth token:

- `pam`

## How the Plugin Makes Decisions

1. Validates payload schema (`service`, `module`, `password`, `prompt`).
2. Validates configured `service_name` and `allowed_modules`.
3. Enforces timeout and policy restrictions.
4. Enforces prompt mode constraints.
5. Resolves local username/principal on success.

## Step 1 - Policy and Registry Setup

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.pam": {
  "required": false,
  "allowed_signers": ["sb-enterprise-kid-2026"],
  "min_version": "2.0.0",
  "max_version": "2.x",
  "allowed_method_ids": [
    "scratchbird.auth.pam_conversation"
  ]
}
```

Production hardening:

- Keep `auth.pam.allow_test_directives=false`.

## Step 2 - HBA Routing

```conf
hostssl all             all             0.0.0.0/0              pam
```

## Step 3 - Plugin Configuration Keys

Required:

- `service_name`
- `allowed_modules`

Optional:

- `conversation_timeout_ms` (default `2000`)
- `runtime_profile` (`production` or `test`)

Example:

```json
{
  "service_name": "login",
  "allowed_modules": ["pam_unix.so","pam_sss.so"],
  "conversation_timeout_ms": 2000,
  "runtime_profile": "production"
}
```

## Step 4 - Payload Contract

Expected payload fields:

- `service=<pam-service>`
- `module=<pam-module>`
- `password=<secret>` (alias `pwd`)
- `prompt=<hidden|secret>`

Denied when:

- service/module not allowed
- prompt mode is insecure/unknown
- timeout condition triggered
- synthetic test directives used outside test profile

## Step 5 - Linux PAM Baseline Setup

1. Choose PAM service name to expose to ScratchBird (for example `scratchbird-login`).
2. Create `/etc/pam.d/scratchbird-login` with approved modules only.
3. Keep module list minimal and explicit.

Example `/etc/pam.d/scratchbird-login`:

```text
auth    required    pam_unix.so
account required    pam_unix.so
```

If AD-backed users are needed through SSSD:

```text
auth    required    pam_sss.so
account required    pam_sss.so
```

Set plugin config:

- `service_name=scratchbird-login`
- `allowed_modules=["pam_unix.so","pam_sss.so"]` (only what you actually use)

## Step 6 - Linux + AD (SSSD) Integration

1. Join domain:
   ```bash
   realm discover EXAMPLE.COM
   sudo realm join EXAMPLE.COM -U Administrator
   ```
2. Confirm SSSD is active:
   ```bash
   systemctl status sssd
   ```
3. Confirm user lookup:
   ```bash
   id user1@example.com
   ```
4. Validate PAM path with controlled login test before enabling production route.

## Step 7 - Windows Notes

PAM is Linux/Unix-native.

For Windows-hosted ScratchBird, prefer:

- `kerberos`
- `ldap`
- token-based methods

## Step 8 - Verification

1. Run plugin selftest:
   ```bash
   ctest --test-dir build -R sb_auth_plugin_pam_selftest --output-on-failure
   ```
2. Stage checks:
   - allowed service/module + valid password -> allow
   - disallowed service -> deny (`AUTH_PAM_SERVICE_NOT_ALLOWED`)
   - disallowed module -> deny (`AUTH_PAM_MODULE_NOT_ALLOWED`)
   - insecure prompt -> deny (`AUTH_PAM_INSECURE_PROMPT`)
   - timeout path -> deny (`AUTH_PAM_CONVERSATION_TIMEOUT`)

## Common Errors and Fixes

- `AUTH_PAM_CONFIG_INVALID`
  - Cause: missing required config keys.
  - Fix: correct config JSON.
- `AUTH_PAM_SERVICE_NOT_ALLOWED`
  - Cause: request service differs from configured service.
  - Fix: align request payload and configured service.
- `AUTH_PAM_MODULE_NOT_ALLOWED`
  - Cause: requested module not in allowlist.
  - Fix: restrict payload or update allowlist after review.
- `AUTH_PAM_INSECURE_PROMPT`
  - Cause: prompt value outside `hidden|secret`.
  - Fix: set prompt mode to approved values.
- `AUTH_PAM_TEST_DIRECTIVE_DENIED`
  - Cause: synthetic directives attempted in production.
  - Fix: remove directives and keep hardening key disabled.

## Rollback

1. Route affected HBA entries from `pam` to SCRAM.
2. Keep PAM service files for post-incident review.
3. Restore `pam` path only after module/service allowlist tests pass.

## Completion Checklist

- [ ] PAM service file created and reviewed.
- [ ] Allowed module set minimized.
- [ ] Timeout value set.
- [ ] Linux/SSSD integration tested if AD-backed users are used.
- [ ] Production test-directive hardening confirmed.
- [ ] Positive and negative auth checks passed.
