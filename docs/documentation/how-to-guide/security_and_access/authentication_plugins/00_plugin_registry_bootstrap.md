# Plugin Registry Bootstrap (All Plugins)

[Prev](./README.md) | [Next](./01_trust_reject.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## Goal

Set up ScratchBird so authentication plugins are loaded through the signed policy/truststore registry path, with predictable fail-closed behavior.

This is the required foundation before enabling any individual plugin guide in this folder.

## Audience and Assumptions

This document is written for operators who are new to:

- ScratchBird internals.
- Linux or Windows service hosting.
- Plugin signing/admission policy concepts.

## Core Concepts (Plain Language)

- Trust store:
  - File listing signer keys ScratchBird trusts for plugin manifests.
  - Default path: `/etc/scratchbird/auth_plugin_truststore.jwks.json`.
- Plugin policy:
  - File listing which plugin IDs and method IDs are allowed.
  - Default path: `/etc/scratchbird/auth_plugins.policy.json`.
- Plugin root:
  - Directory containing externally packaged plugin modules and signed manifests.
  - Required only for external module admission path.
- Built-in vs external plugin loading:
  - Built-in plugin IDs are admitted from policy allowlist.
  - External plugins additionally require `manifest.json`, `manifest.jws`, digest match, and ABI compatibility.

## Step 1 - Build ScratchBird and Plugin Modules

From repo root:

```bash
cmake -S . -B build
cmake --build build
```

Plugin module build output root defaults to:

- `build/auth_plugins/`

Expected module layout pattern:

- `build/auth_plugins/<plugin_id>/libscratchbird_auth_<plugin_dir>.so`

Example:

- `build/auth_plugins/scratchbird.auth.ldap/libscratchbird_auth_ldap.so`

## Step 2 - Create Runtime Directories and Permissions

Linux example:

```bash
sudo mkdir -p /etc/scratchbird
sudo mkdir -p /opt/scratchbird/auth_plugins
sudo chown root:root /etc/scratchbird
sudo chmod 0755 /etc/scratchbird
sudo chown -R root:root /opt/scratchbird/auth_plugins
sudo chmod -R 0755 /opt/scratchbird/auth_plugins
```

If your service runs as non-root, ensure read access to these files/directories.

## Step 3 - Install Trust Store and Policy Baselines

Copy examples and then edit:

```bash
sudo cp etc/auth/auth_plugin_truststore.jwks.json.example /etc/scratchbird/auth_plugin_truststore.jwks.json
sudo cp etc/auth/auth_plugins.policy.json.example /etc/scratchbird/auth_plugins.policy.json
sudo chmod 0644 /etc/scratchbird/auth_plugin_truststore.jwks.json /etc/scratchbird/auth_plugins.policy.json
```

## Step 4 - Configure AuthManager Plugin Registry Flags

Configure your service bootstrap to set `AuthManagerConfig` fields:

```cpp
scratchbird::security::AuthManagerConfig cfg;
cfg.auth_plugin_registry_enabled = true;
cfg.allow_legacy_auth_fallback = false;
cfg.auth_plugin_truststore_path = "/etc/scratchbird/auth_plugin_truststore.jwks.json";
cfg.auth_plugin_policy_path = "/etc/scratchbird/auth_plugins.policy.json";
cfg.auth_plugin_root = "/opt/scratchbird/auth_plugins";
```

Behavior expectations:

- `auth_plugin_registry_enabled=true` enables policy-controlled method availability checks.
- `allow_legacy_auth_fallback=false` enforces fail-closed when registry method is unavailable.

## Step 5 - External Plugin Packaging Layout (If Used)

For each external plugin ID under `auth_plugin_root`, create:

- `manifest.json`
- `manifest.jws`
- module binary at manifest `module_path`

Directory example:

```text
/opt/scratchbird/auth_plugins/
  scratchbird.auth.ldap/
    manifest.json
    manifest.jws
    libscratchbird_auth_ldap.so
```

Manifest requirements (simplified):

- `plugin_id` must match directory/policy entry.
- `supported_method_ids` must match policy allowlist.
- `module_sha256` must exactly match module file digest.
- `signing.kid` must be trusted by truststore and permitted by policy for that plugin.

## Step 6 - Verify Module Digest Before Service Start

Linux:

```bash
sha256sum /opt/scratchbird/auth_plugins/scratchbird.auth.ldap/libscratchbird_auth_ldap.so
```

Windows PowerShell:

```powershell
Get-FileHash C:\opt\scratchbird\auth_plugins\scratchbird.auth.ldap\libscratchbird_auth_ldap.so -Algorithm SHA256
```

Make sure digest equals `module_sha256` in `manifest.json`.

## Step 7 - Register HBA/Auth Routing

Set HBA method tokens for your target plugin(s), for example:

```conf
hostssl all all 0.0.0.0/0 scram-sha-256
hostssl appdb app_user 10.20.0.0/16 ldap
```

## Step 8 - Startup and Admission Verification

After service restart, verify:

1. Service starts successfully.
2. Required plugin set is complete.
3. No admission rejections for enabled plugin set.
4. Authentication method routes return expected allow/deny results.

If startup fails with required plugin errors, check:

- plugin ID typo in policy.
- missing or unreadable manifest files.
- signer not trusted or signer not allowed by policy.
- digest mismatch.
- method ID mismatch between manifest and policy.

## Step 9 - Baseline Selftests

Run a minimal baseline before per-plugin testing:

```bash
ctest --test-dir build -R 'sb_auth_plugin_contract_harness|sb_auth_plugin_runbook_selftest|sb_auth_plugin_hardening_h1_selftest' --output-on-failure
```

## Hardening Checklist (Required Before Production)

- [ ] `allow_legacy_auth_fallback=false` in production profiles.
- [ ] Policy file owned by root/admin and write-protected.
- [ ] Trust store update process documented and approval-gated.
- [ ] External plugin manifests/signatures come only from release signing pipeline.
- [ ] HBA routes do not leave `trust` open on remote paths.
- [ ] Provider plugins keep test-directive policy keys disabled in production.

## Rollback Strategy

If plugin admission failures block startup:

1. Restore last known-good policy and truststore files.
2. Restore last known-good plugin root package set.
3. Restart service.
4. Re-run baseline selftests.
5. Perform root-cause analysis before reattempting rollout.
