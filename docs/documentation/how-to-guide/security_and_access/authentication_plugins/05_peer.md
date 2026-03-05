# Peer UID Plugin

[Prev](./04_token_authkey.md) | [Next](./06_certificate_mtls.md) | [Topic README](./README.md) | [Security and Access README](../README.md) | [How-To Guide README](../../README.md) | [Documentation Workspace README](../../../README.md)

## What This Plugin Is For

Use this plugin to authenticate local IPC/Unix-socket clients using OS peer credentials (UID/GID/PID).

Method ID:

- `scratchbird.auth.peer_uid`

## Dispatch Path

Direct HBA/auth token:

- `peer`

## Step 1 - Policy Configuration

In `/etc/scratchbird/auth_plugins.policy.json`:

```json
"scratchbird.auth.peer": {
  "required": true,
  "allowed_signers": ["sb-release-kid-2026"],
  "min_version": "1.0.0",
  "max_version": "1.x",
  "allowed_method_ids": [
    "scratchbird.auth.peer_uid"
  ]
}
```

Set policy-value keys:

- `auth.peer.accept_ipc=true|false`
- `auth.peer.allow_uid_zero=true|false`

Recommended production defaults:

- `auth.peer.accept_ipc=true`
- `auth.peer.allow_uid_zero=false`

## Step 2 - HBA Routing

```conf
# Local socket clients only
local   all             all                                     peer
```

Do not use `peer` on remote TCP entries.

## Step 3 - Runtime Expectations

- Transport must be local IPC/Unix socket.
- Payload must be empty.
- Plugin denies if peer PID is missing.
- Optional username-to-principal resolution occurs via `resolve_user_by_name`.

Key deny codes:

- `AUTH_PEER_REMOTE_TRANSPORT`
- `AUTH_PEER_IPC_DISABLED`
- `AUTH_PEER_UID_ZERO_DENIED`
- `AUTH_PEER_PID_MISSING`

## External Authenticator Integration

None. This is host-kernel credential validation, not remote identity federation.

## Verification

```bash
ctest --test-dir build -R sb_auth_plugin_peer_selftest --output-on-failure
```

Also validate by testing:

- Local socket connection succeeds for allowed UID.
- Remote TCP connection with `peer` rule fails.

## Common Errors

- `AUTH_PEER_PAYLOAD_NOT_EMPTY`: caller sent unexpected payload.
- `AUTH_PEER_UID_ZERO_DENIED`: root login attempted while policy forbids UID 0.

## Rollback

1. Replace `peer` HBA rule with SCRAM for impacted path.
2. Keep `peer` only for controlled local automation if needed.
3. Re-verify local admin scripts after rollback.

## Completion Checklist

- [ ] `auth.peer.accept_ipc` policy key set.
- [ ] UID 0 decision documented.
- [ ] Local-only routing validated.
- [ ] Remote misuse test confirmed denial.
