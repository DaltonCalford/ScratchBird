# IDENT Plugin

[Prev](./14_kerberos.md) | [Next](./16_radius.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin for RFC1413/IDENT-style username verification on trusted network segments.

Method ID:

- `scratchbird.auth.ident_rfc1413`

## Dispatch Path

Direct HBA/auth token:

- `ident`

## Critical Security Warning

IDENT is a legacy protocol and should only be used on tightly controlled internal networks.

Do not expose IDENT-authenticated paths to internet-facing traffic.

## Step 1 - Policy and Registry Setup

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.ident": {
  "required": false,
  "allowed_signers": ["sb-enterprise-kid-2026"],
  "min_version": "2.0.0",
  "max_version": "2.x",
  "allowed_method_ids": [
    "scratchbird.auth.ident_rfc1413"
  ]
}
```

## Step 2 - HBA Routing

Scope IDENT to approved private CIDRs only:

```conf
host    all             all             10.20.0.0/16           ident
```

## Step 3 - Plugin Configuration Keys

Required:

- `trusted_cidrs`

Optional:

- `ident_timeout_ms` (default `1000`)
- `require_username_match` (default `true`)
- `runtime_profile` (`production` or `test`)

Example:

```json
{
  "trusted_cidrs": ["10.20.0.0/16","127.0.0.1/32"],
  "ident_timeout_ms": 1000,
  "require_username_match": true,
  "runtime_profile": "production"
}
```

## Step 4 - Payload Contract

Expected key/value payload:

- `ident_user=<user>` (aliases: `user`, `ident`)
- optional `client_addr=<ipv4>`

Rules:

- if `client_addr` is present, it must equal actual connection remote address.
- unknown keys deny request.
- duplicate aliases deny request.

## Step 5 - Network and Host Prerequisites

1. Confirm trusted network CIDR boundaries and firewall ACLs.
2. Confirm source hosts that will send IDENT requests are managed/trusted.
3. Confirm routing/NAT behavior does not alter source addresses unexpectedly.
4. Keep `require_username_match=true` unless you have explicit mapping logic and approval.

Linux connectivity checks:

```bash
ip route
ss -tuna | grep -E ':113\b'
```

## Step 6 - Windows Notes

IDENT is uncommon in Windows-first environments.

Preferred alternatives:

- `kerberos`
- `ldap`
- token-based methods (`token`, `jwt_oidc`, `oauth_validator` depending your integration path)

If IDENT must be used in mixed environments, enforce strict network segmentation and short deprecation timeline.

## Step 7 - Verification

```bash
ctest --test-dir build -R sb_auth_plugin_ident_selftest --output-on-failure
```

Stage checks:

- trusted CIDR + matched user -> allow
- untrusted CIDR -> deny (`AUTH_IDENT_UNTRUSTED_TRANSPORT`)
- payload `client_addr` mismatch -> deny (`AUTH_IDENT_ADDRESS_MISMATCH`)
- username mismatch with strict policy -> deny (`AUTH_CREDENTIAL_INVALID`)

## Common Errors and Fixes

- `AUTH_IDENT_QUERY_FAILED`
  - Cause: malformed payload or query failure.
  - Fix: validate payload keys and ensure network path is stable.
- `AUTH_IDENT_UNTRUSTED_TRANSPORT`
  - Cause: client outside configured CIDRs.
  - Fix: correct CIDR config or client source path.
- `AUTH_IDENT_ADDRESS_MISMATCH`
  - Cause: payload client address does not match transport source.
  - Fix: remove spoofed/mismatched `client_addr` and inspect NAT/proxy layers.
- `AUTH_CREDENTIAL_INVALID`
  - Cause: username mismatch under strict mode.
  - Fix: keep username aligned or review policy decision.

## Rollback

1. Replace `ident` HBA entries with SCRAM/Kerberos.
2. Restrict IDENT to temporary emergency/internal-only lane if still required.
3. Set target date to remove IDENT from production.

## Completion Checklist

- [ ] IDENT limited to trusted private CIDRs.
- [ ] Username-match policy explicitly approved.
- [ ] Address-mismatch and untrusted-CIDR tests passed.
- [ ] Decommission plan documented.
