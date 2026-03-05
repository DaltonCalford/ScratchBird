# LDAP Plugin

[Prev](./12_proxy_assertion.md) | [Next](./14_kerberos.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to authenticate users against LDAP-compatible identity services and map directory groups to ScratchBird roles.

Method ID:

- `scratchbird.auth.ldap_bind`

## Dispatch Path

Direct HBA/auth token:

- `ldap`

## How the Plugin Makes Decisions

The plugin evaluates the request in this order:

1. Validates payload schema (`user/password/groups/starttls/endpoint` only).
2. Validates plugin config loaded and syntactically correct.
3. Enforces endpoint allowlist (`allowed_ldap_endpoints`) if present.
4. Enforces transport policy:
   - if `require_starttls=true` and `ldap_uri` is not `ldaps://`, request must set `starttls=true`.
5. Validates credentials/provider outcome.
6. Maps user group(s) to ScratchBird role using `group_role_map`.

If no group maps to any role entry, login is denied.

## Step 1 - Policy and Registry Setup

In `/etc/scratchbird/auth_plugins.policy.json` ensure this entry exists:

```json
"scratchbird.auth.ldap": {
  "required": false,
  "allowed_signers": ["sb-enterprise-kid-2026"],
  "min_version": "2.0.0",
  "max_version": "2.x",
  "allowed_method_ids": [
    "scratchbird.auth.ldap_bind"
  ]
}
```

Production hardening:

- Keep `auth.ldap.allow_test_directives=false`.

## Step 2 - HBA Routing

Use TLS transport lane for remote clients:

```conf
hostssl all             all             0.0.0.0/0              ldap
```

Do not use `host` (non-TLS) when LDAP credentials are sent.

## Step 3 - Plugin Configuration Keys

LDAP plugin instance config requires:

- `ldap_uri` (required)
- `bind_dn_template` (required)
- `group_role_map` (required)
- `connect_timeout_ms` (optional, default `3000`)
- `require_starttls` (optional, default `true`)
- `runtime_profile` (`production` or `test`)
- `allowed_ldap_endpoints` (optional CSV/array)

Reference shape:

```json
{
  "ldap_uri": "ldaps://dc01.example.com:636",
  "bind_dn_template": "uid={user},ou=People,dc=example,dc=com",
  "group_role_map": "cn=dba:DBA,cn=readonly:READ_ONLY",
  "connect_timeout_ms": 3000,
  "require_starttls": true,
  "allowed_ldap_endpoints": ["dc01.example.com:636","dc02.example.com:636"],
  "runtime_profile": "production"
}
```

## Step 4 - Client Payload Contract

Expected key/value payload:

- `user=<username>` or `username=<username>`
- `password=<secret>` or `pwd=<secret>`
- `groups=<group1,group2,...>`
- `starttls=<true|false|1|0>`
- `endpoint=<ldap-endpoint>`

Important behavior:

- If `require_starttls=true` and URI is not `ldaps://`, payload must indicate `starttls=true`.
- At least one `groups` value must map through `group_role_map`.
- Unknown keys or duplicate aliases deny request.

## Step 5 - Linux Host Joining a Windows AD Domain (Detailed)

If LDAP backend is Active Directory and ScratchBird runs on Linux:

1. Install base packages (Ubuntu/Debian example):
   ```bash
   sudo apt update
   sudo apt install -y realmd sssd sssd-tools adcli krb5-user samba-common-bin ldap-utils
   ```
2. Install base packages (RHEL/Rocky/Alma example):
   ```bash
   sudo dnf install -y realmd sssd adcli krb5-workstation samba-common-tools openldap-clients oddjob oddjob-mkhomedir
   ```
3. Verify DNS can resolve AD domain and domain controllers:
   ```bash
   getent hosts dc01.example.com
   host -t SRV _ldap._tcp.example.com
   ```
4. Discover domain:
   ```bash
   realm discover EXAMPLE.COM
   ```
5. Join domain:
   ```bash
   sudo realm join EXAMPLE.COM -U Administrator
   ```
6. Confirm join:
   ```bash
   realm list
   ```
7. Validate user/group lookup:
   ```bash
   id user1@example.com
   getent group "Domain Users"
   ```
8. Validate LDAP over TLS from ScratchBird host:
   ```bash
   ldapsearch -H ldaps://dc01.example.com:636 -x -D "svc_ldap@example.com" -W -b "dc=example,dc=com" "(sAMAccountName=user1)"
   ```

## Step 6 - Service Account and Group Mapping Workflow

For reliable operations, define these explicitly:

1. Service account identity used for LDAP bind/search.
2. Exact group DN values expected from directory (for example `cn=dba,ou=groups,dc=example,dc=com`).
3. ScratchBird role names that each group maps to.

Example mapping design:

- `cn=dba,ou=groups,dc=example,dc=com` -> `DBA`
- `cn=readonly,ou=groups,dc=example,dc=com` -> `READ_ONLY`
- `cn=etl,ou=groups,dc=example,dc=com` -> `ETL_RUNNER`

Encode as `group_role_map` comma list:

```text
cn=dba,ou=groups,dc=example,dc=com:DBA,cn=readonly,ou=groups,dc=example,dc=com:READ_ONLY,cn=etl,ou=groups,dc=example,dc=com:ETL_RUNNER
```

Note:

- Current plugin logic uses provided `groups` payload and checks mapping.
- If your environment requires dynamic group lookup per login, run that lookup in an upstream identity gateway and pass resolved groups into plugin payload.

## Step 7 - Windows Host Integration Notes

When ScratchBird runs on Windows:

1. Verify Windows host trusts AD CS CA chain used by LDAPS certs.
2. Verify DNS and firewall to domain controllers.
3. Validate bind account can read user/group attributes.
4. Keep `allowed_ldap_endpoints` aligned to approved DC list.

PowerShell reachability checks:

```powershell
Resolve-DnsName dc01.example.com
Test-NetConnection dc01.example.com -Port 636
```

## Step 8 - Verification Procedure

1. Run plugin selftest:
   ```bash
   ctest --test-dir build -R sb_auth_plugin_ldap_selftest --output-on-failure
   ```
2. Stage environment functional checks:
   - valid user + mapped group -> allow
   - valid user + unmapped group -> deny (`AUTH_LDAP_ROLE_MAPPING_MISSING`)
   - STARTTLS off when required -> deny (`AUTH_LDAP_STARTTLS_REQUIRED`)
   - endpoint outside allowlist -> deny (`AUTH_LDAP_ENDPOINT_NOT_ALLOWED`)
3. Record result evidence (command output, timestamp, operator).

## Common Errors and Fixes

- `AUTH_LDAP_CONFIG_INVALID`
  - Cause: required config key missing or bad type.
  - Fix: validate JSON keys and value types.
- `AUTH_LDAP_BIND_FAILED`
  - Cause: wrong bind credentials or user credential mismatch.
  - Fix: verify bind account and test with `ldapsearch` manually.
- `AUTH_LDAP_TIMEOUT`
  - Cause: directory endpoint unreachable or timeout too small.
  - Fix: test network path and raise `connect_timeout_ms` carefully.
- `AUTH_LDAP_ENDPOINT_NOT_ALLOWED`
  - Cause: endpoint not in allowlist.
  - Fix: align payload endpoint and `allowed_ldap_endpoints`.
- `AUTH_LDAP_TEST_DIRECTIVE_DENIED`
  - Cause: synthetic test directives attempted in production profile.
  - Fix: remove synthetic values and keep test-directive toggle off in production.

## Rollback

1. Change HBA route from `ldap` to SCRAM for impacted users.
2. Keep LDAP policy entry but disable route while investigating.
3. Restore after endpoint/TLS/group-mapping checks are green.

## Completion Checklist

- [ ] `ldap` HBA route defined for intended scope only.
- [ ] Required plugin config keys present.
- [ ] AD/Linux domain-join and lookup verification completed.
- [ ] Bind account permissions validated.
- [ ] Group-to-role mapping validated with real directory groups.
- [ ] Negative tests (TLS/endpoint/group mismatch) passed.
