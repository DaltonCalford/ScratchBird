# 1️⃣ Architectural Positioning — Making It Optional Was the Right Call

Turning the Manager into an **optional control-plane add-on** instead of a required front door was absolutely correct for ScratchBird’s philosophy.

You now have two valid deployment modes:

### Mode A — Direct Native v3 Listener (No Manager)

- Listener bound publicly (or via firewall)

- Native v3 auth + engine auth

- No DB registry/unlock orchestration layer

- Simpler deployment

- Fewer moving parts

- Suitable for:
  
  - Embedded systems
  
  - Single-DB environments
  
  - Dev/test

### Mode B — Manager + Localhost-only Native Listeners

- Manager handles:
  
  - DB discovery
  
  - Encryption unlock
  
  - Cluster key shares
  
  - Routing

- Listeners bound to localhost only

- Stronger control-plane security

- Suitable for:
  
  - Multi-tenant
  
  - Regulated environments
  
  - Clustered deployments
  
  - Enterprise

That dual-mode approach matches your larger design ethos:

> “Security and complexity are configurable; the DBA may tighten or relax.”

Good.

---

# 2️⃣ Security Evaluation of the Optional Model

Making it optional introduces some **risk divergence** between deployments. That’s fine — but it must be explicit.

## Without Manager (Direct Listener Mode)

Security boundary is:

Remote client → Native v3 Listener → Parser → Engine

Risks:

- Listener must expose DB selection logic (if multi-DB)

- Encryption unlock must occur at listener/engine level

- No centralized control-plane policy

This mode must:

- Reject cross-DB selection unless explicitly supported

- Avoid any path-based DB opens

- Require explicit DB identity binding at handshake time

If that is clearly documented as “single DB per listener” mode, then the risk is bounded and acceptable.

---

## With Manager (Proxy Mode)

Security boundary becomes layered:

Remote client → Manager (authz + DB orchestration)  
                     ↓  
             Localhost Native Listener → Parser → Engine

Advantages:

- DB enumeration is controlled

- Unlock logic isolated

- Cluster key reconstruction isolated

- Listener never sees decryption keys

- External world never sees listener ports

This is strictly stronger — and suitable for hardened deployments.

You’ve effectively created a “mini control-plane/data-plane split” similar to:

- PostgreSQL + pgpool

- MySQL + ProxySQL

- Envoy fronting gRPC services

- Kubernetes API server + kubelet

That’s architecturally sound.

---

# 3️⃣ The Most Important Question: Is There a Security Regression?

Because it’s optional, we must check for regression.

The only potential issue would be:

> If direct-listener mode allows bypassing constraints that manager-mode would enforce.

To prevent that:

### You must clearly define:

- In **direct mode**, how is DB selection done?
  
  - Fixed DB per listener?
  
  - Explicit DB UUID in native handshake?

- In **direct mode**, how are encrypted DBs unlocked?
  
  - Manual pre-unlock?
  
  - CLI only?
  
  - Prohibited?

If direct mode allows runtime unlock via remote handshake without control-plane policy, that would be weaker.

So the spec should clearly state:

> “Manager mode is REQUIRED for runtime encrypted DB unlock and cluster key-share retrieval.”

That keeps direct mode intentionally simpler.

---

# 4️⃣ DBBT + Preface Design — Still Sound as Optional

The DB Binding Token (DBBT) + Listener Preface model is clean and remains valuable even if optional.

Making it optional changes one thing:

- DBBT logic must not be assumed by core native listener code.

- It must be a pluggable binding layer activated when listener is launched in “managed mode”.

That’s good separation.

You effectively have:

Native Listener Mode:  
    no LPREFACE  
    direct native handshake  

Managed Listener Mode:  
    LPREFACE required  
    DBBT validation required

That conditional activation must be explicit in listener configuration.

---

# 5️⃣ Performance and Operational Thoughts

## Proxy overhead

Because you chose Option 1 (L4 proxy), Manager is in the data path.

This is acceptable because:

- Native v3 is likely binary and efficient.

- Forwarding can be zero-copy optimized later.

- Manager can become stateless after proxy begins.

But:

- You should design Manager to enter “raw forwarding mode” immediately after preface success.

- No further parsing in Manager during proxy.

Otherwise you risk accidental performance bottlenecks.

---

# 6️⃣ Cluster and Encryption Handling — Strongest Part of the Design

Making the Manager optional means:

- Direct mode = no automatic cluster unlock orchestration.

- Managed mode = full cluster handshake + key share reconstruction.

That’s actually perfect.

It mirrors your existing philosophy:

> “High security cluster features are available but not mandatory.”

I strongly approve of this separation.

---

# 7️⃣ What I Like Most About This Design

This part is subtle but important:

You’ve separated three concepts that most systems mix:

1. **Authentication**

2. **Database selection**

3. **Database unlock/orchestration**

Most RDBMS systems conflate these. You didn’t.

That’s architecturally superior.

---

# 8️⃣ What I Would Tighten

### A. Make the optional nature explicit in configuration

Something like:

mode = direct | managed

- `direct` → native listener binds externally.

- `managed` → native listener binds localhost only and requires LPREFACE.

No auto-detection. Explicit mode.

---

### B. Document direct-mode security envelope

State clearly:

- Direct mode is intended for:
  
  - single DB per listener
  
  - already-unlocked databases

- Encrypted DB unlock must be done via CLI or pre-start phase.

Avoid any “remote unlock in direct mode”.

---

### C. Ensure no implicit DB switching exists anywhere

The listener + parser must never allow:

- `SET DATABASE`

- dynamic rebind

- path-based open

Unless routed back through Manager with new DBBT.

---

# 9️⃣ Big Picture Assessment

### Security posture:

Strong when managed mode enabled.  
Acceptable when direct mode constrained.

### Complexity:

Optional add-on keeps core simpler.  
Good design decision.

### Scalability:

Manager layer makes multi-tenant and cluster deployments realistic.

### Cleanliness:

Separation of control-plane from data-plane is mature architecture.

---

# 🔟 Final Verdict

Making the Manager Proxy an optional add-on was the correct architectural decision.

The current specification:

- Preserves flexibility.

- Avoids forcing enterprise complexity on simple deployments.

- Enables high-security multi-DB/clustered scenarios.

- Keeps native v3 core clean and focused.

From a systems architecture perspective, this is a well-balanced design.



---

Based on the *current* ScratchBird implementation (engine IPC session + auth provider), there are a handful of changes I’d make to materially improve correctness, security, and “multi-security-model” flexibility **without dragging non-native protocols into scope**.

I’m grounding this in the actual code paths you have today:

- Engine IPC auth is handled in `ServerSession::handleAuth()` and it supports PASSWORD / MD5 / SCRAM

- Local auth is implemented in `LocalAuthProvider` and already supports bcrypt verification + MD5 verifier + SCRAM verifier material parsed from a JSON “password_hash” field

- There is still a “bootstrap allowed” pathway when catalog is empty/only SYSTEM (this is *not* the old accept-all dev mode, but it can still be tightened)

Below are the changes I would make, in order of impact.

---

## 1) Make “no catalog” a hard fail everywhere (except explicitly controlled bootstrap)

Right now `ServerSession::authenticate()` has a path where, if the catalog manager is missing, it allows authentication and marks the user as superuser (dev mode) . You already said you’re removing dev mode; do it *at the call site* and *provider factory* too.

**Change**

- In `ServerSession::authenticate()`:
  
  - If `catalog == nullptr`, return `AuthResult::PROVIDER_ERROR` / `Status::INVALID_AUTHORIZATION` and do **not** create a session.

- In `AuthProviderFactory::createDefault(...)`, treat `catalog == nullptr` as fatal (it already throws for LocalAuthProvider ctor, but don’t allow upstream fallback behavior).

**Why**

- Keeps “security model” selection explicit. No silent bypass based on configuration mistakes.

---

## 2) Tighten bootstrap into a first-class state with an explicit bootstrap credential

Your LocalAuthProvider currently has “bootstrap allowed” when no users or only SYSTEM, and then it grants SYSTEM superuser and creates an authkey . That’s a good shape, but it’s too easy to trigger if someone spins up a fresh DB and forgets to lock down the environment.

**Change**

- Introduce a dedicated `AuthProviderType::BOOTSTRAP` or a `bootstrap_mode` policy flag in catalog config.

- Require one extra proof for bootstrap login:
  
  - Option A: a one-time bootstrap token file (0600), or
  
  - Option B: OS peer credentials match DB owner (since IPC has peer creds already captured in ServerSession) .

- On first successful bootstrap, automatically:
  
  - create initial admin user,
  
  - disable bootstrap mode permanently (flip a catalog flag),
  
  - rotate/issue a real authkey.

**Why**

- Removes “accidental superuser” scenarios while keeping your “DBA can shoot self in foot but must be explicit” stance.

---

## 3) Make SCRAM the default and negotiate methods server-first (prevent downgrade)

Today, the engine accepts `AUTH_REQUEST` where the client chooses `auth_method` and sends payload; server branches on that . That allows downgrade if a client chooses weaker methods you still support.

**Change**

- Move to “server chooses method”:
  
  - On `CONNECT_RESPONSE`, include `allowed_auth_methods` + `required_auth_method` (policy-driven).
  
  - Client must comply or fail.

- Set policy default:
  
  - `SCRAM_SHA_256` required
  
  - `SCRAM_SHA_512` optional for hardened mode
  
  - `MD5` disabled by default
  
  - `PASSWORD` either removed or allowed only in explicitly “trusted local” mode

You already have message IDs like `AUTH_CHALLENGE` in the wire protocol header, which is a natural place for server-first negotiation[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

[include/scratchbird/protocol/wi…](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

.

**Why**

- Prevents a class of protocol downgrade mistakes and makes “security models” truly policy-driven.

---

## 4) Fix the PASSWORD method so it never ships plaintext across the wire (even IPC)

Right now `ProtocolCodec::buildAuthRequest(session_id, username, password)` literally puts password bytes into the payload (plaintext)[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

[src/protocol/wire_protocol](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

. Even if you’re “IPC-only,” plaintext passwords are a liability (crash dumps, tracing, memory inspection).

**Change**

- If you keep PASSWORD at all:
  
  - redefine it to mean “password-based auth” but **implemented as SCRAM** (i.e., remove the plaintext mode).

- Or delete PASSWORD entirely from the protocol enum and treat SCRAM as “the password method”.

**Why**

- It removes a whole set of operational security risks with basically no downside.

---

## 5) Replace per-process lockout tracking with catalog-backed tracking (or shared service)

`LoginAttemptTracker` is instantiated per LocalAuthProvider instance in-process . In a multi-parser architecture, you can end up with:

- per-parser counters (inconsistent),

- lockout bypass by hitting a different parser,

- lockout resets on restart.

**Change**

- Persist login failures/lockouts to catalog tables (or a shared in-memory manager service).

- Track by:
  
  - username
  
  - peer identity (uid/pid if IPC)
  
  - remote addr if later network

- Keep fast in-memory cache, but source of truth should be shared/persistent.

**Why**

- Makes brute-force protection meaningful in your actual multi-process model.

---

## 6) Harden SCRAM implementation details (you’re close)

Your SCRAM implementation already:

- rejects channel binding `p=` (fine for now)

- uses dummy SCRAM record to reduce user enumeration timing differences

- uses constant-time compare via `CRYPTO_memcmp`

**Changes**

- Enforce minimum SCRAM iterations by policy (and reject weak records).

- Add automatic “rehash/upgrade” on login:
  
  - if stored iterations < policy → mark user for credential upgrade flow.

- Rate limit SCRAM handshake steps separately (start vs finish) to prevent resource abuse.

**Why**

- Turns SCRAM into an evolving security posture rather than static config.

---

## 7) Turn AuthKeys into real session tokens (short-lived, scoped, revocable)

You already create an authkey per successful auth and bind it to the session context . Right now they’re “UNLIMITED” and appear to be more of an audit/session linkage.

**Change**

- Extend authkeys to support:
  
  - expiry
  
  - scopes (LOGIN_SESSION, REATTACH, SERVICE)
  
  - binding (optional: peer uid/pid; later cert fingerprint)
  
  - revocation

- Add an `AuthMethod::TOKEN` in the engine protocol so trusted clients can reconnect without redoing password auth.

**Why**

- This is how you support different operational models (pooling, service accounts, dormant reattach) cleanly and securely.

---

## 8) Make “managed listener mode” explicit and non-bypassable (if/when you implement it)

Right now I only see the spec doc committed, not code changes tied to Manager Proxy[](https://github.com/DaltonCalford/ScratchBird/blob/da2ad453af04f2ab75d5fddc5fb5895e1e4d5625/docs/specifications/SB-NATIVE-V3%20Manager%20Proxy%20Specification.md)

[docs/specifications/SB-NATIVE-V…](https://github.com/DaltonCalford/ScratchBird/blob/da2ad453af04f2ab75d5fddc5fb5895e1e4d5625/docs/specifications/SB-NATIVE-V3%20Manager%20Proxy%20Specification.md)

. When you implement it, the key improvement is:

**Change**

- Add a listener config flag:
  
  - `mode = direct | managed`

- In managed mode, listener MUST require a local-only preface (LPREFACE) before it processes any client bytes, and MUST bind DB UUID from that preface.

- In direct mode, listener MUST be fixed to a single DB UUID at startup (no client “choose DB”).

**Why**

- Prevents “optional add-on” from accidentally creating a third, ambiguous mode with security gaps.

----

# Δ0 — Ground Rules (what changes *mean*)

**Target behavior:**

- No silent “dev mode” auth success.

- Bootstrap is explicit, controlled, audited, and one-way (auto-disables).

- SCRAM is the default, and *server chooses auth method* (no downgrade).

- MD5 and plaintext “PASSWORD” are either removed or policy-gated.

- Lockout/rate-limit is consistent across parser pools (catalog-backed).

- AuthKey becomes a real session credential (scoped, expiring, revocable).

- Optional manager-proxy mode remains optional (but if enabled, it’s non-bypassable).

---

# Δ1 — Remove “no catalog => allow-all superuser”

### Files

- `src/server/server_session.cpp`

### Changes

1. **Delete/disable the “no catalog manager – allow any authentication (development mode)” branch** in `ServerSession::authenticate()` that currently generates a user_id/authkey and sets `is_superuser=true` when `catalog == nullptr` .

2. Replace with:
   
   - `error_msg_out = "Authentication provider unavailable";`
   
   - return `AuthResult::PROVIDER_ERROR`
   
   - propagate to `handleAuth()` as an auth failure.

### Notes

- You still keep “bootstrap allowed” *inside the LocalAuthProvider* (Δ2), but **only when catalog exists** and catalog policy says bootstrap is enabled.

---

# Δ2 — Make bootstrap an explicit catalog state + explicit bootstrap credential

### Files

- `src/core/auth_provider.cpp` (LocalAuthProvider bootstrap path)

- `include/scratchbird/core/auth_provider.h` (optional additions)

- `src/core/catalog_manager.*` (schema + APIs; not opened here, but this is where it belongs)

### Changes

## 2.1 Add catalog flags/state

Add catalog config table fields (or a dedicated config object):

- `bootstrap_enabled` (bool)

- `bootstrap_token_hash` (string / bytes)

- `bootstrap_expires_at` (timestamp)

- `bootstrap_consumed_at` (timestamp nullable)

## 2.2 Add a bootstrap auth method

In `wire_protocol.h` add:

- `AuthMethod::BOOTSTRAP` (or `TOKEN` could serve, but better explicit).

## 2.3 Bootstrap proof requirement

Modify LocalAuthProvider bootstrap branch (currently triggers when only SYSTEM user or empty catalog) :

- Only allow bootstrap if:
  
  - `bootstrap_enabled == true`
  
  - AND bootstrap not expired/consumed
  
  - AND provided credential matches `bootstrap_token_hash`
  
  - OPTIONAL: also require IPC peer uid == DB owner (strong for IPC deployments)

**Credential format**

- simplest: treat payload as a random token string (never store plaintext; store Argon2/bcrypt hash).

## 2.4 One-way disable

On first bootstrap success:

- create initial admin principal (or require bootstrap to immediately run “init user” procedure)

- set `bootstrap_enabled=false`

- set `bootstrap_consumed_at=now`

- wipe token hash field

## 2.5 Audit

Log:

- “bootstrap attempted” (success/fail)

- requesting username, peer uid/pid if available, and reason

---

# Δ3 — Server-first auth method negotiation (no downgrade)

### Files

- `include/scratchbird/protocol/wire_protocol.h`[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)
  
  [include/scratchbird/protocol/wi…](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

- `src/protocol/wire_protocol.cpp`[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)
  
  [src/protocol/wire_protocol](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

- `src/server/server_session.cpp`

- `src/core/catalog_manager.*` (policy storage + fetch)

### Changes

## 3.1 Protocol additions

Add to the engine wire protocol:

- `CONNECT_RESPONSE` includes:
  
  - `auth_policy_id` (optional)
  
  - `required_auth_method`
  
  - `allowed_auth_methods_mask`
  
  - `server_nonce` (for binding)

You already have `AUTH_CHALLENGE` defined in the enum list[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

[include/scratchbird/protocol/wi…](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

—use it for negotiation/challenges.

## 3.2 Server drives auth

In `ServerSession::handleConnect()`:

- after successful CONNECT, send:
  
  - allowed methods
  
  - required method (policy-driven)
  
  - any initial SCRAM “server-first needed?” (or prompt client to send client-first)

## 3.3 Client choice enforcement

In `ServerSession::handleAuth()`:

- reject `AUTH_REQUEST` if client selected a method not allowed by policy.

- reject downgrade (if server required SCRAM and client sends MD5/PASSWORD).

## 3.4 Policy location

Add catalog API:

- `getAuthPolicyForEndpoint(endpoint_id)` or `getAuthPolicyForDb(db_uuid)`

- return allowed methods + required method + min SCRAM iterations + etc.

---

# Δ4 — Remove or redefine plaintext PASSWORD auth

### Files

- `include/scratchbird/protocol/wire_protocol.h`[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)
  
  [include/scratchbird/protocol/wi…](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

- `src/protocol/wire_protocol.cpp`[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)
  
  [src/protocol/wire_protocol](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

- `src/server/server_session.cpp`

- `src/core/auth_provider.cpp`

### Changes

**Preferred**: Remove plaintext password transport entirely.

## Option A (best): Delete PASSWORD from wire protocol

- Remove `AuthMethod::PASSWORD`

- Replace “password auth” with SCRAM only.

This also means removing `ProtocolCodec::buildAuthRequest(session_id, username, password)` which currently sends raw password bytes[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

[src/protocol/wire_protocol](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

.

## Option B: Keep PASSWORD symbol but re-map to SCRAM

- Treat `PASSWORD` as “password-based auth” but implemented as SCRAM steps.

- Never accept a payload that is plaintext password.

Either way: in `ServerSession::handleAuth()`, delete the branch that interprets payload as plaintext password and calls `authenticate(username, password, ...)` .

---

# Δ5 — Persist lockout/rate-limit in catalog (not per-process)

### Files

- `src/core/auth_provider.cpp` (remove per-instance LoginAttemptTracker ownership)

- `include/scratchbird/core/auth_provider.h` (optional new admin methods)

- `src/core/catalog_manager.*` (new tables + APIs)

### Changes

## 5.1 New catalog tables

Add tables (names are suggestions):

- `sys_auth_failures`:
  
  - `username`
  
  - `peer_uid` (nullable)
  
  - `peer_pid` (nullable)
  
  - `fail_count`
  
  - `first_fail_at`
  
  - `last_fail_at`
  
  - `locked_until`

- `sys_auth_rate_limits` (optional if you unify with above)

## 5.2 Catalog APIs

- `recordAuthFailure(username, peer_identity, reason)`

- `recordAuthSuccess(username, peer_identity)`

- `getLockoutState(username, peer_identity)`

## 5.3 Provider changes

In `LocalAuthProvider`:

- remove `new LoginAttemptTracker(policy)` per instance

- use catalog-backed lockout queries and updates instead

- keep an in-memory cache optionally, but it must be best-effort only.

## 5.4 Peer identity binding

For IPC sessions, incorporate peer creds already gathered in `ServerSession` (you already call `connection->getPeerCredentials()`) .

---

# Δ6 — SCRAM hardening: enforce iteration policy + upgrade path

### Files

- `src/core/auth_provider.cpp` (SCRAM parsing and begin/finish)

- `src/security/scram_auth.*` (if you want shared helpers; you already include it)

- `src/core/catalog_manager.*` (store policy and credential metadata)

### Changes

## 6.1 Minimum iterations

Add policy:

- `scram_min_iterations_sha256`

- `scram_min_iterations_sha512`

In `beginScramAuth()`:

- if stored record iterations < min, either:
  
  - deny auth, OR
  
  - allow auth but mark “needs upgrade” for later  
    Prefer: allow + upgrade flag (less disruptive).

## 6.2 Upgrade on login

If password KDF is available (bcrypt) and you can derive SCRAM verifiers from password during password change, you can:

- set a “rehash required” flag in catalog

- require password reset for upgrade  
  OR

- if you still had plaintext password (you shouldn’t), you could upgrade automatically (but we’re removing plaintext).

So realistically:

- upgrade SCRAM record on next password change or admin maintenance job.

## 6.3 Rate limit SCRAM handshake steps

Add separate rate limiting for:

- SCRAM begin

- SCRAM finish  
  This prevents an attacker from spamming half-handshakes.

---

# Δ7 — Upgrade AuthKey into real session tokens (TOKEN auth)

### Files

- `include/scratchbird/protocol/wire_protocol.h` (new auth method/status payload)[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)
  
  [include/scratchbird/protocol/wi…](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

- `src/server/server_session.cpp` (handleAuth branch)

- `src/core/auth_provider.cpp` / catalog manager (issue/validate/revoke authkeys)

### Changes

## 7.1 AuthKey schema

Extend catalog authkey record (you already create them) :

- `authkey_id` (uuid)

- `issuer`

- `scope` enum: `LOGIN_SESSION`, `REATTACH`, `SERVICE`

- `expires_at`

- `revoked_at`

- `last_used_at`

- `bind_peer_uid` (optional)

- `bind_peer_pid` (optional)

- `usage_count`, `max_uses`

## 7.2 Protocol method

Add `AuthMethod::TOKEN`

- Payload: `{authkey_id, nonce, proof}`

- proof = HMAC(authkey_secret, nonce || session_id || …)
  
  - where `authkey_secret` is stored hashed/encrypted; or treat authkey as random bearer token + binding.

## 7.3 Engine auth handling

In `ServerSession::handleAuth()`:

- add branch for TOKEN:
  
  - validate authkey in catalog
  
  - check scope/expiry/revocation/binding
  
  - create session + authkey_id (or reuse)
  
  - respond OK

This enables connection pooling / reconnect without passwords.

---

# Δ8 — Managed listener mode: explicit, non-bypassable (optional add-on)

*(This is about implementation scaffolding you’ll need once you implement the Manager Proxy design. Right now I only see the spec doc in repo, not the code.)*[](https://github.com/DaltonCalford/ScratchBird/blob/da2ad453af04f2ab75d5fddc5fb5895e1e4d5625/docs/specifications/SB-NATIVE-V3%20Manager%20Proxy%20Specification.md)

[docs/specifications/SB-NATIVE-V…](https://github.com/DaltonCalford/ScratchBird/blob/da2ad453af04f2ab75d5fddc5fb5895e1e4d5625/docs/specifications/SB-NATIVE-V3%20Manager%20Proxy%20Specification.md)

### Files

- Native listener implementation (SBWP/native listener area, not the emulation adapters)

- Config parsing layer

### Changes

## 8.1 Add explicit listener mode

Config:

- `listener.mode = direct | managed`

Behavior:

- `direct`: listener binds externally; fixed DB UUID configured at start (no runtime selection)

- `managed`: listener binds localhost only; requires LPREFACE before any client bytes.

## 8.2 Implement LPREFACE gate

Listener must:

- read local-only preface before starting native handshake

- validate DBBT (HMAC key)

- bind db_uuid into connection context

- then proceed with native protocol

## 8.3 Enforce DB binding at parser attach

Parser must attach to engine by `db_uuid` handle, never by path.

---

# Δ9 — Tests and tooling deltas (you’ll want these)

### Files

- `tests/unit/test_scram_auth.cpp` exists already[](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/tests/unit/test_scram_auth.cpp)
  
  [tests/unit/test_scram_auth](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/tests/unit/test_scram_auth.cpp)

- Add new tests under `tests/unit/` and `tests/integration/`

### Add tests for:

1. **No-catalog auth fails** (regression test for removed dev mode)

2. **Bootstrap requires explicit token** and auto-disables after first use

3. **Server-first negotiation**: client attempts MD5 when SCRAM required → fail

4. **No plaintext password**: buildAuthRequest(password) removed or disabled

5. **Lockout persistence**: failures across two “parser sessions” share lockout state

6. **SCRAM min iterations** enforcement

7. **AuthKey token auth**: mint token, authenticate with token, revoke token

---

# Δ10 — Quick “edit map” (where to touch what)

Here’s the “most direct” mapping for edits:

### `src/server/server_session.cpp`

- Remove dev/no-catalog success path

- Add server-first auth negotiation in connect response

- Enforce allowed/required methods

- Remove plaintext PASSWORD branch

- Add TOKEN branch (validate authkey)

### `include/scratchbird/protocol/wire_protocol.h`[

include/scratchbird/protocol/wi…

](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/include/scratchbird/protocol/wire_protocol.h)

- Add fields to connect response for auth policy

- Add `AuthMethod::TOKEN` (+ maybe BOOTSTRAP)

- Possibly remove `AuthMethod::PASSWORD` and `AuthMethod::MD5` or gate them

### `src/protocol/wire_protocol.cpp`[

src/protocol/wire_protocol

](https://github.com/DaltonCalford/ScratchBird/blob/3003f0c9abd2b77ad242c44146e48fb6e08755d2/src/protocol/wire_protocol.cpp)

- Remove `buildAuthRequest(session_id, username, password)` plaintext builder

- Add negotiation message build/parse helpers

- Add TOKEN payload build/parse

### `src/core/auth_provider.cpp`

- Replace in-process lockout tracker with catalog-backed tracking

- Tighten bootstrap: require token + catalog flag

- SCRAM iteration enforcement + upgrade flags

- Support token auth issuance/validation (may live better in catalog manager)

### `src/core/catalog_manager.*`

- Add:
  
  - auth policy storage (allowed methods, required method, iteration mins)
  
  - auth failure/lockout tables
  
  - authkey expansions (scope, expiry, revocation, binding)
  
  - bootstrap config flags and token hash

---

## One important call: MD5

You currently support MD5 in provider + protocol (including constant-time compare) . My delta assumes:

- **MD5 stays implemented** but is **disabled by default** via auth policy.

- That lets you keep it for compatibility/testing without weakening default posture.

---

If you want, I can format this delta into a single checklist with unique IDs (SB-AUTH-XXX) so you can drop it directly into your existing task list system.

Sources

format this delta into a single checklist with unique IDs (SB-AUTH-XXX) so you can drop it directly into your existing task list system.

Below is the **consolidated implementation checklist** formatted for direct inclusion in your task system.

All items use unique IDs in the format:

> **SB-AUTH-XXX**

They are grouped by logical area and sequenced in recommended implementation order.

---

# 🔐 ScratchBird Authentication Hardening – Implementation Checklist

---

## 🔴 Phase 1 — Remove Implicit / Unsafe Paths

### SB-AUTH-001 — Remove No-Catalog Superuser Fallback

**Component:** `src/server/server_session.cpp`  
**Description:**  
Remove any branch that authenticates successfully when `catalog == nullptr`.

**Required Changes:**

- In `ServerSession::authenticate()`:
  
  - Replace fallback with `AuthResult::PROVIDER_ERROR`
  
  - Do not generate user_id/authkey

- Ensure `AuthProviderFactory::createDefault()` never returns a permissive fallback

**Acceptance Criteria:**

- Engine refuses all auth if catalog unavailable

- Unit test confirms no implicit superuser

---

### SB-AUTH-002 — Remove Plaintext PASSWORD Transport

**Component:**

- `include/scratchbird/protocol/wire_protocol.h`

- `src/protocol/wire_protocol.cpp`

- `src/server/server_session.cpp`

**Description:**  
Remove plaintext password transmission.

**Required Changes:**

- Remove `AuthMethod::PASSWORD` OR

- Remap it internally to SCRAM

- Delete `buildAuthRequest(session_id, username, password)` that sends raw password bytes

**Acceptance Criteria:**

- No plaintext password appears in wire protocol

- All password-based auth flows use SCRAM

---

### SB-AUTH-003 — Disable MD5 by Default

**Component:** `LocalAuthProvider`, auth policy

**Description:**  
MD5 remains implemented but disabled unless explicitly enabled by policy.

**Required Changes:**

- Add policy flag `allow_md5_auth`

- Reject MD5 in `handleAuth()` unless allowed

**Acceptance Criteria:**

- MD5 login fails under default configuration

- MD5 works only when policy explicitly enables it

---

## 🟡 Phase 2 — Bootstrap Hardening

### SB-AUTH-010 — Add Explicit Bootstrap Mode Flag

**Component:** Catalog schema + LocalAuthProvider

**Description:**  
Replace implicit “only SYSTEM user” bootstrap detection with explicit flag.

**Schema Additions:**

- `bootstrap_enabled`

- `bootstrap_token_hash`

- `bootstrap_expires_at`

- `bootstrap_consumed_at`

---

### SB-AUTH-011 — Require Bootstrap Token

**Component:** LocalAuthProvider

**Description:**  
Bootstrap requires valid one-time token.

**Required Changes:**

- Add `AuthMethod::BOOTSTRAP`

- Validate provided token against stored hash

- Reject if expired or already consumed

---

### SB-AUTH-012 — Auto-Disable Bootstrap After First Use

**Component:** LocalAuthProvider + Catalog

**Description:**  
After successful bootstrap:

- Create initial admin

- Set `bootstrap_enabled=false`

- Wipe token

**Acceptance Criteria:**

- Bootstrap cannot be reused

- Fully audited

---

## 🟢 Phase 3 — Server-Driven Auth Negotiation

### SB-AUTH-020 — Add Server-First Auth Negotiation

**Component:** `wire_protocol.h`, `server_session.cpp`

**Description:**  
Server chooses auth method.

**Protocol Additions:**

- `allowed_auth_methods_mask`

- `required_auth_method`

- `auth_policy_id`

- `server_nonce`

---

### SB-AUTH-021 — Enforce Required Auth Method

**Component:** `ServerSession::handleAuth()`

**Description:**  
Reject client-selected methods not allowed by policy.

**Acceptance Criteria:**

- Downgrade attempts fail

- Required SCRAM enforced

---

### SB-AUTH-022 — Add Auth Policy Retrieval

**Component:** Catalog

**Description:**  
Add API:

- `getAuthPolicyForDb(db_uuid)`

- returns:
  
  - allowed methods
  
  - required method
  
  - min SCRAM iterations
  
  - lockout thresholds

---

## 🔵 Phase 4 — Lockout & Rate Limiting (Cluster-Safe)

### SB-AUTH-030 — Move Lockout Tracking to Catalog

**Component:** Catalog + LocalAuthProvider

**Description:**  
Replace per-process `LoginAttemptTracker` with shared catalog-backed tracking.

**Schema Additions:**

- `sys_auth_failures`

- `locked_until`

- `fail_count`

---

### SB-AUTH-031 — Bind Lockout to Peer Identity

**Component:** ServerSession + Catalog

**Description:**  
Record:

- username

- peer_uid

- peer_pid

- optional remote addr (future)

---

### SB-AUTH-032 — Rate Limit SCRAM Begin/Finish Separately

**Component:** LocalAuthProvider

**Description:**  
Add rate limits for:

- `beginScramAuth`

- `finishScramAuth`

---

## 🟣 Phase 5 — SCRAM Policy Hardening

### SB-AUTH-040 — Enforce Minimum SCRAM Iterations

**Component:** LocalAuthProvider

**Description:**  
Reject or flag credentials below policy minimum.

---

### SB-AUTH-041 — Mark Credentials for Upgrade

**Component:** Catalog + LocalAuthProvider

**Description:**  
If SCRAM iterations below policy:

- Mark user as `credential_upgrade_required`

- Require password reset

---

### SB-AUTH-042 — Add Iteration Policy to Catalog

**Component:** Catalog

**Add Fields:**

- `scram_min_iterations_sha256`

- `scram_min_iterations_sha512`

---

## 🟠 Phase 6 — AuthKey Upgrade (Session Tokens)

### SB-AUTH-050 — Extend AuthKey Schema

**Component:** Catalog

**Add Fields:**

- `scope`

- `expires_at`

- `revoked_at`

- `last_used_at`

- `bind_peer_uid`

- `bind_peer_pid`

- `max_uses`

- `usage_count`

---

### SB-AUTH-051 — Add TOKEN Auth Method

**Component:** Wire protocol + ServerSession

**Description:**  
Add `AuthMethod::TOKEN`.

**Payload:**

- `authkey_id`

- `nonce`

- `proof`

---

### SB-AUTH-052 — Implement Token Validation Flow

**Component:** ServerSession

**Description:**  
Validate:

- existence

- expiry

- revocation

- scope

- peer binding

- usage limits

---

### SB-AUTH-053 — Add Token Revocation API

**Component:** Catalog

**Description:**  
Admin can revoke:

- specific authkey

- all keys for user

- all keys by scope

---

## 🟤 Phase 7 — Managed Listener Mode (Optional Add-On)

### SB-AUTH-060 — Add Explicit Listener Mode Flag

**Component:** Listener config

**Modes:**

- `direct`

- `managed`

---

### SB-AUTH-061 — Implement LPREFACE Gate in Managed Mode

**Component:** Native v3 listener

**Description:**  
Before reading client bytes:

- Require valid LPREFACE

- Validate DBBT

- Bind DB UUID

---

### SB-AUTH-062 — Enforce Fixed DB UUID in Direct Mode

**Component:** Native v3 listener

**Description:**  
In direct mode:

- Listener bound to single DB UUID

- Reject DB switching attempts

---

### SB-AUTH-063 — Parser Binding Contract Enforcement

**Component:** Listener + Parser

**Description:**  
Parser attach must:

- Use db_uuid handle only

- Never open by path

- Never accept client DB change

---

## ⚫ Phase 8 — Audit & Observability

### SB-AUTH-070 — Audit Bootstrap Attempts

### SB-AUTH-071 — Audit Auth Policy Decisions

### SB-AUTH-072 — Audit DBBT Issuance (when manager mode enabled)

### SB-AUTH-073 — Audit Token Auth Usage

---

## 🧪 Phase 9 — Test Coverage

### SB-AUTH-080 — Test No-Catalog Hard Fail

### SB-AUTH-081 — Test Bootstrap Token Single-Use

### SB-AUTH-082 — Test Auth Downgrade Rejection

### SB-AUTH-083 — Test Lockout Across Parser Pool

### SB-AUTH-084 — Test SCRAM Min Iterations

### SB-AUTH-085 — Test Token Auth Flow

### SB-AUTH-086 — Test Token Revocation

### SB-AUTH-087 — Test Managed Listener LPREFACE Enforcement

---

# 🧩 Suggested Implementation Order

1. SB-AUTH-001, 002, 003

2. SB-AUTH-010 → 012

3. SB-AUTH-020 → 022

4. SB-AUTH-030 → 032

5. SB-AUTH-040 → 042

6. SB-AUTH-050 → 053

7. SB-AUTH-060 → 063

8. SB-AUTH-070 → 087
