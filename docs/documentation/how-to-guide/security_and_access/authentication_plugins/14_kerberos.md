# Kerberos Plugin

[Prev](./13_ldap.md) | [Next](./15_ident.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin for Kerberos/GSSAPI-style authentication with replay-window protection and service-principal validation.

Method ID:

- `scratchbird.auth.kerberos_gssapi`

## Dispatch Path

Direct HBA/auth tokens:

- `gss`
- `gssapi`
- `kerberos`

## How the Plugin Makes Decisions

The plugin evaluates in this order:

1. Validates ticket payload schema.
2. Validates configured `service_principal` and `keytab_path`.
3. Validates KDC endpoint allowlist if configured.
4. Resolves keytab secret reference.
5. Enforces replay window against `ts` and `nonce`.
6. Verifies ticket signature.
7. Resolves external subject to ScratchBird principal.

## Step 1 - Policy and Registry Setup

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.kerberos": {
  "required": false,
  "allowed_signers": ["sb-enterprise-kid-2026"],
  "min_version": "2.0.0",
  "max_version": "2.x",
  "allowed_method_ids": [
    "scratchbird.auth.kerberos_gssapi"
  ]
}
```

## Step 2 - HBA Routing

```conf
hostssl all             all             0.0.0.0/0              gss
```

## Step 3 - Plugin Configuration Keys

Required keys:

- `service_principal`
- `keytab_path`

Optional:

- `allow_delegation`
- `max_replay_window_ms`
- `runtime_profile` (`production` or `test`)
- `allowed_kdc_endpoints`

Example:

```json
{
  "service_principal": "postgres/db01.example.com@EXAMPLE.COM",
  "keytab_path": "policy:auth.kerberos.keytab_ref",
  "allow_delegation": false,
  "max_replay_window_ms": 30000,
  "allowed_kdc_endpoints": ["dc01.example.com:88","dc02.example.com:88"],
  "runtime_profile": "production"
}
```

## Step 4 - Ticket Payload Contract

Expected key/value payload:

- `princ=<service_principal>` (alias `principal`)
- `sub=<subject>` (alias `subject`)
- `ts=<unix_ms>`
- `nonce=<nonce>`
- `kdc=<kdc-endpoint>`
- `sig=<hex>`

Unknown fields or duplicate aliases deny the request.

## Step 5 - Linux Kerberos and AD Setup

1. Install required packages (Ubuntu/Debian):
   ```bash
   sudo apt update
   sudo apt install -y krb5-user realmd sssd adcli samba-common-bin
   ```
2. Install required packages (RHEL/Rocky/Alma):
   ```bash
   sudo dnf install -y krb5-workstation realmd sssd adcli samba-common-tools oddjob oddjob-mkhomedir
   ```
3. Configure `/etc/krb5.conf` for your realm/KDC.
4. Validate Kerberos clock sync (critical for replay/time window):
   ```bash
   timedatectl status
   ```
5. Acquire ticket for service account:
   ```bash
   kinit svc_scratchbird@EXAMPLE.COM
   klist
   ```
6. Validate keytab locally:
   ```bash
   klist -k /path/to/scratchbird.keytab
   ```
7. Store keytab ref in policy store:
   - `auth.kerberos.keytab_ref=<secret-or-path-reference>`

## Step 6 - Windows AD Admin Workflow (SPN + Keytab)

From domain admin workstation:

1. Create or identify service account (`EXAMPLE\svc_scratchbird`).
2. Register SPN:
   ```powershell
   setspn -S postgres/db01.example.com EXAMPLE\svc_scratchbird
   ```
3. Validate SPN registration:
   ```powershell
   setspn -L EXAMPLE\svc_scratchbird
   ```
4. Generate keytab:
   ```powershell
   ktpass /princ postgres/db01.example.com@EXAMPLE.COM /mapuser EXAMPLE\svc_scratchbird /pass * /ptype KRB5_NT_PRINCIPAL /out scratchbird.keytab
   ```
5. Transfer keytab securely to ScratchBird host.
6. Rotate keytab on defined schedule and update secret reference.

## Step 7 - Network and Endpoint Control

If `allowed_kdc_endpoints` is configured, make sure payload `kdc` value exactly matches one entry.

Connectivity checks:

Linux:

```bash
nc -zv dc01.example.com 88
```

Windows PowerShell:

```powershell
Test-NetConnection dc01.example.com -Port 88
```

## Step 8 - Verification

1. Run plugin selftest:
   ```bash
   ctest --test-dir build -R sb_auth_plugin_kerberos_selftest --output-on-failure
   ```
2. Stage checks:
   - valid ticket -> allow
   - replayed nonce -> deny (`AUTH_KERBEROS_REPLAY_DETECTED`)
   - timestamp outside window -> deny (`AUTH_KERBEROS_TICKET_EXPIRED`)
   - wrong signature -> deny (`AUTH_KERBEROS_TICKET_INVALID`)
   - unapproved `kdc` endpoint -> deny (`AUTH_KERBEROS_KDC_ENDPOINT_NOT_ALLOWED`)

## Common Errors and Fixes

- `AUTH_KERBEROS_CONFIG_INVALID`
  - Cause: missing `service_principal` or `keytab_path`.
  - Fix: correct plugin config JSON.
- `AUTH_KERBEROS_KEYTAB_UNAVAILABLE`
  - Cause: secret ref/path cannot be resolved/read.
  - Fix: verify secret provider, file path, and permissions.
- `AUTH_KERBEROS_TICKET_EXPIRED`
  - Cause: replay window mismatch or host time skew.
  - Fix: synchronize time and review `max_replay_window_ms`.
- `AUTH_KERBEROS_SUBJECT_UNKNOWN`
  - Cause: subject could not be mapped to local principal.
  - Fix: add/update `resolve_user_by_external_subject` mapping.

## Rollback

1. Route impacted traffic from `gss` to SCRAM temporarily.
2. Keep policy and keytab references for rapid restore.
3. Restore Kerberos route after SPN/keytab/time checks pass.

## Completion Checklist

- [ ] SPN registered and validated.
- [ ] Keytab generated, secured, and referenced.
- [ ] Replay window and clock sync validated.
- [ ] KDC allowlist and connectivity validated.
- [ ] Subject mapping validated.
- [ ] Positive and replay/expiry negative tests passed.
