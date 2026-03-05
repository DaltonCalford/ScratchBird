# RADIUS Plugin

[Prev](./15_ident.md) | [Next](./17_pam.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to authenticate credentials against one or more RADIUS servers.

Method ID:

- `scratchbird.auth.radius_pap`

## Dispatch Path

Direct HBA/auth token:

- `radius`

## How the Plugin Makes Decisions

1. Validates payload schema (`password`, `auth` plus optional `simulate` in test profile).
2. Validates plugin config (`radius_servers`, `shared_secret_ref`, timeout).
3. Resolves shared secret reference.
4. Recomputes request authenticator and compares in constant time.
5. Applies endpoint allowlist.
6. Applies timeout/reject conditions.

## Step 1 - Policy and Registry Setup

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.radius": {
  "required": false,
  "allowed_signers": ["sb-enterprise-kid-2026"],
  "min_version": "2.0.0",
  "max_version": "2.x",
  "allowed_method_ids": [
    "scratchbird.auth.radius_pap"
  ]
}
```

Production hardening:

- Keep `auth.radius.allow_test_directives=false`.

## Step 2 - HBA Routing

```conf
hostssl all             all             0.0.0.0/0              radius
```

## Step 3 - Plugin Configuration Keys

Required:

- `radius_servers`
- `shared_secret_ref`

Optional:

- `request_timeout_ms` (default `2000`)
- `runtime_profile` (`production` or `test`)
- `allowed_radius_endpoints`

Example:

```json
{
  "radius_servers": ["radius-primary.example.com","radius-secondary.example.com"],
  "shared_secret_ref": "auth.radius.shared_secret_ref",
  "request_timeout_ms": 2000,
  "allowed_radius_endpoints": ["radius-primary.example.com","radius-secondary.example.com"],
  "runtime_profile": "production"
}
```

Policy store key for secret reference:

- `auth.radius.shared_secret_ref=<secret-ref-or-managed-secret-material>`

## Step 4 - Payload Contract

Expected key/value payload:

- `password=<secret>` (alias `pwd`)
- `auth=<hmac>` (aliases `authenticator`, `mac`)
- optional `simulate=<directives>` (test profile only)

`simulate` directives are denied unless:

1. `runtime_profile=test`
2. `auth.radius.allow_test_directives=true`

## Step 5 - Linux FreeRADIUS Integration

1. Define ScratchBird host as RADIUS client in FreeRADIUS config.
2. Set client shared secret and auth policy.
3. Open firewall UDP ports:
   - authentication: `1812/udp`
   - accounting (if used): `1813/udp`
4. Validate server responds:
   ```bash
   nc -zvu radius-primary.example.com 1812
   ```
5. Keep `radius_servers` and `allowed_radius_endpoints` synchronized with approved list.

## Step 6 - Windows NPS Integration

1. Add ScratchBird host as RADIUS client in NPS.
2. Configure shared secret and network policy conditions.
3. If NPS uses AD for credential checks, validate AD reachability and policy precedence.
4. Validate endpoint connectivity from ScratchBird host:
   ```powershell
   Test-NetConnection radius-primary.example.com -Port 1812 -UdpPort 1812
   ```

## Step 7 - Authenticator Construction Guidance

The plugin recomputes expected authenticator using username, password, and shared secret.

Operational guidance:

- Generate authenticator on the trusted caller side that submits plugin payload.
- Never log clear-text password or shared secret.
- Ensure caller and ScratchBird agree on exact input canonicalization.

## Step 8 - Verification

1. Run selftest:
   ```bash
   ctest --test-dir build -R sb_auth_plugin_radius_selftest --output-on-failure
   ```
2. Stage checks:
   - valid secret/authenticator -> allow
   - bad authenticator -> deny (`AUTH_RADIUS_SHARED_SECRET_INVALID`)
   - rejected credentials -> deny (`AUTH_RADIUS_REJECTED`)
   - out-of-policy endpoint -> deny (`AUTH_PLUGIN_POLICY_DENIED`)

## Common Errors and Fixes

- `AUTH_RADIUS_CONFIG_INVALID`
  - Cause: missing required config keys.
  - Fix: validate JSON keys and value types.
- `AUTH_RADIUS_TIMEOUT`
  - Cause: server unreachable or timeout too low.
  - Fix: validate network path and tune `request_timeout_ms`.
- `AUTH_RADIUS_SHARED_SECRET_INVALID`
  - Cause: secret mismatch or bad authenticator.
  - Fix: rotate/verify shared secret and caller-side auth generation.
- `AUTH_PLUGIN_POLICY_DENIED`
  - Cause: endpoint not in allowlist.
  - Fix: align configured endpoint list with approved servers.
- `AUTH_RADIUS_TEST_DIRECTIVE_DENIED`
  - Cause: synthetic directives in production path.
  - Fix: remove synthetic directives and keep hardening key disabled in production.

## Rollback

1. Move impacted HBA path from `radius` to SCRAM.
2. Keep RADIUS config objects in place to reduce restore time.
3. Re-enable only after secret, endpoint, and timeout checks pass.

## Completion Checklist

- [ ] Primary/secondary endpoints configured and reachable.
- [ ] Shared secret reference validated and rotation process defined.
- [ ] Endpoint allowlist enforced.
- [ ] Production test-directive hardening confirmed.
- [ ] Positive and negative auth-path checks passed.
