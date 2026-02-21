# Security
Last modified: 2026-02-21

[Back to Admin Index](index.md) | [Back to Documentation Index](../index.md)

---

## 0.1.0 Authentication/Security Baseline

ScratchBird 0.1.0 enforces an engine-owned, policy-driven authentication model:

1. Bootstrap is explicit and one-time (no silent dev fallback).
2. Server drives auth method negotiation (`AUTH_CHALLENGE`) before credentials are accepted.
3. SCRAM-SHA-256 is the default auth method.
4. Legacy `PASSWORD`/`MD5` are policy-gated and transport-gated.
5. `TOKEN` and `PEER` auth are first-class methods.
6. MFA challenge/step-up is enforced via `AuthStatus::CONTINUE` and policy.

## Service Identity

ScratchBird server processes are expected to run as:

- user: `scratchbird`
- group: `scratchbird`

Config keys:

```ini
[server]
run_as_user = scratchbird
run_as_group = scratchbird
```

Startup fail-fast is enabled if the configured user/group does not exist.

## Bootstrap Security

Bootstrap phase (`UNINITIALIZED`) is detected from catalog bootstrap state + user list.

Bootstrap proof requirements:

1. Bootstrap token file (default: `/var/lib/scratchbird/bootstrap.token`, override: `SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE`).
2. File must be regular file with mode `0600`.
3. Presented token must match exactly (timing-safe compare).
4. Token is consumed/revoked after success (zeroed + removed).

Bootstrap peer-UID gate (local IPC hardening):

- `SCRATCHBIRD_BOOTSTRAP_REQUIRE_OWNER_UID` (default enabled)
- `SCRATCHBIRD_BOOTSTRAP_OWNER_UID` (optional explicit owner uid)

## Policy-Driven Auth Negotiation

Handshake contract:

1. Client sends `AUTH_REQUEST` with empty payload to trigger negotiation.
2. Server returns `AUTH_CHALLENGE` with:
   - allowed auth methods
   - optional required auth method
   - allowed transport mask
   - nonce
3. Client must choose a method allowed by policy.

Deterministic policy-deny codes include:

- `AUTH_POLICY_NEGOTIATION_REQUIRED`
- `AUTH_POLICY_METHOD_DENIED`
- `AUTH_POLICY_REQUIRED_METHOD`
- `AUTH_POLICY_TRANSPORT_DENIED`
- `AUTH_POLICY_USER_MISMATCH`
- `AUTH_POLICY_PEER_REQUIRED`
- `AUTH_POLICY_PEER_TRANSPORT_UNSUPPORTED`

## Auth Methods in 0.1.0

| Method | Status | Notes |
|---|---|---|
| `SCRAM_SHA_256` | Default | Recommended baseline |
| `SCRAM_SHA_512` | Supported | High-security policy profile |
| `TOKEN` | Supported | AuthKey proof payload + policy gating |
| `PEER` | Supported | Local Unix socket peer UID/GID mapping |
| `PASSWORD` | Legacy | Deprecated warning + policy fallback + trusted local IPC only |
| `MD5` | Legacy | Deprecated warning + policy fallback + trusted local IPC only |

Legacy usage emits warnings and increments telemetry counter:

- `scratchbird_auth_legacy_method_total`

## MFA and Step-Up

If policy requires MFA:

1. Server returns `AuthStatus::CONTINUE` with MFA challenge payload.
2. Client responds with `SBMFA1|<challenge_id>|<code>`.
3. Server validates TOTP or policy-allowed recovery/break-glass code.

MFA policy controls include:

- `allow_recovery_codes`
- `allow_break_glass`
- `max_challenge_attempts`
- `step_up_ttl_ms`

Step-up enforcement applies to privileged SQL, including:

- `SET ROLE` / `RESET ROLE`
- `GRANT` / `REVOKE`
- `CREATE/ALTER/DROP USER|ROLE|GROUP|POLICY|TOKEN`
- `CREATE/ALTER/DROP AUTH POLICY`
- `CREATE/ALTER/DROP CONNECTION RULE`

If step-up is missing/expired, privileged commands fail with SQLSTATE `28000`.

## Token and Peer Controls

### AuthKey Token Auth

Token payload contract:

- `authkey_id(16)` + `proof_len(u16)` + `proof` + `binding_len(u16)` + `binding`

Token auth validates:

1. AuthKey state/scope
2. binding policy
3. HMAC proof
4. usage consumption (`consumeAuthKey`)

### Peer Identity Auth

Peer auth requires:

1. local Unix socket peer credentials
2. principal-account mapping in catalog
3. source scope mapped by `PEER_UID` or `PEER_GID`

## Audit Coverage

Audit/security events include:

- bootstrap lifecycle (`attempt`, `failure`, `success`, `revoke`)
- login success/failure
- MFA method details (`totp`, `recovery_code`, `break_glass`)
- step-up denial for privileged commands

## Operational Checklist (0.1.0)

1. Ensure install flow created `scratchbird:scratchbird` and owned runtime dirs.
2. Ensure bootstrap token exists before first bootstrap login.
3. Ensure default auth policy requires SCRAM (`SCRAM_SHA_256` or stronger).
4. Keep legacy fallback disabled unless explicitly required for trusted local IPC.
5. Configure peer mapping only when local process identity auth is intended.
6. Configure MFA policies for privileged operator/admin accounts.
