# SB-NATIVE-V3 Manager Proxy Specification

**Document ID:** SB-NATIVE-V3-MGRPROXY-01  
**Status:** Draft (Implementation-ready)  
**Scope:** Native v3 only; manager routes external connections to localhost native listeners via transparent stream proxying.

## 1. Goals

1. Provide a single, externally reachable “front door” (**Manager**) on the default ScratchBird port.

2. Keep **Native v3 listeners** bound to **localhost only**.

3. Allow the Manager to:
   
   - enumerate databases (policy-controlled),
   
   - orchestrate open/unlock (including cluster key-share retrieval),
   
   - select an appropriate native listener (dialect/port/pool),
   
   - route a remote client connection to that listener using **transparent bidirectional forwarding**.

4. Ensure the Manager is not merely “routing by port”, but is enforcing:
   
   - authorization to access a DB,
   
   - DB readiness,
   
   - anti-confused-deputy binding of the client connection to the selected DB.

## 2. Non-goals

- Defining non-native protocols (MySQL/PostgreSQL/Firebird emulation).

- Defining full cluster security protocol; only key-share retrieval + “DB ready” orchestration hooks.

- Defining encryption algorithms and KMS details beyond interface requirements.

- Replacing the existing engine auth method; this spec only defines how a connection is routed safely.

## 3. Components and Roles

### 3.1 Manager (External-facing Control Plane + L4 Proxy)

- Listens on **default ScratchBird port** (externally reachable).

- Owns database registry and lifecycle coordination:
  
  - “available databases”
  
  - open/closed state
  
  - encryption status
  
  - cluster membership and key-share retrieval orchestration

- Performs client authentication (manager-level) and authorization for DB visibility / selection.

- Selects a **Native v3 Listener** endpoint on localhost.

- Establishes a local TCP/UDS connection to that listener.

- Switches into **proxy mode**: forwards bytes between remote client and local listener.

### 3.2 Native v3 Listener (Localhost-only Data Plane)

- Bound to `127.0.0.1` (or UNIX domain socket) only.

- Owns:
  
  - native v3 handshake (SBWP / native v3 wire)
  
  - parser pool management for this listener
  
  - per-connection handoff to a parser instance

- Must **verify Manager’s DB-binding attestation** before accepting traffic as “associated with DB X”.

### 3.3 Parser Pool (Per Listener)

- Started ahead-of-time or dynamically by listener.

- Receives a remote client socket *indirectly* (through listener) and uses IPC to engine.

- Authentication for the DB session occurs via the parser+engine path (as per your design).

### 3.4 Engine (IPC-only Core)

- Not reachable over external network.

- Authenticates sessions based on credentials provided through the native pipeline.

- Enforces catalog/policy.

---

## 4. High-Level Flow

### 4.1 Connection Phases

**Phase 0: Connect**

1. Remote client connects to `Manager:DEFAULT_PORT`.

**Phase 1: Manager Authentication**  
2. Client performs **Manager Auth** (method defined by Manager policy; typically SCRAM).  
3. Manager establishes an authenticated control-plane session.

**Phase 2: DB Selection and Readiness**  
4. Client requests DB listing (policy-controlled).  
5. Client selects a target DB (by UUID/alias) and requests native v3 connection.  
6. Manager ensures DB is ready:

- If closed and unencrypted: open DB.

- If encrypted: obtain key (prompt user or cluster shares) and open DB.

- If clustered: perform cluster handshake bootstrap or ensure cluster-ready.

**Phase 3: Listener Binding**  
7. Manager chooses a localhost-only Native v3 Listener (port/socket) appropriate for this DB (and profile).  
8. Manager creates a **DB Binding Token (DBBT)** and a **Listener Preface** to bind the proxied stream to DB X.  
9. Manager opens local connection to listener and sends Listener Preface (out-of-band, local-only).

**Phase 4: Proxy Mode**  
10. Manager bridges bytes:  

- remote client ↔ local listener  
11. Listener performs native v3 handshake with the client (over the proxied stream).  
12. Listener allocates/assigns a parser from the pool and completes the connect path.

---

## 5. Trust and Threat Model

### 5.1 Assumptions

- Listener endpoints are not reachable externally (localhost/UDS only).

- Manager and Listener run on the same host and share a **local trust boundary**.

- OS-level protections apply:
  
  - file permissions for DB files/keys,
  
  - process isolation,
  
  - restricted localhost bind.

### 5.2 Threats addressed

- External client bypassing Manager to hit listener ports (prevented by localhost-only).

- Confused deputy: client requests DB A, but attempts to access DB B via reused listener connection.

- Replay of a previous selection/authorization (mitigated with short-lived token + single-use semantics).

- Manager becoming a “key vault” (mitigated by strict key lifecycle + no persistence; see §11).

### 5.3 Threats not addressed

- Host compromise (root/admin attacker on same machine).

- Side channels from shared hardware.

- Cluster share retrieval security protocol (separate spec).

---

## 6. Manager Control-Plane Protocol (Native v3 Only)

Manager speaks a **Manager Control Protocol (MCP)** over its public port *until it switches into proxy mode*.

### 6.1 MCP Messages

Minimum required message set:

- `MCP_HELLO` (server→client): capabilities, version, auth methods supported.

- `MCP_AUTH_START` / `MCP_AUTH_CONTINUE` / `MCP_AUTH_OK` / `MCP_AUTH_FAIL`

- `MCP_LIST_DATABASES_REQ` / `MCP_LIST_DATABASES_RESP`

- `MCP_DB_INFO_REQ` / `MCP_DB_INFO_RESP` (privileged metadata, optional)

- `MCP_DB_CONNECT_REQ` (client chooses DB + requests native v3 connection)

- `MCP_DB_CONNECT_RESP` (success/failure; if success, manager immediately transitions to proxy mode)

### 6.2 MCP_DB_CONNECT_REQ fields

- `db_selector`: `{ db_uuid | db_alias }`

- `connection_profile`: optional (requested limits/features)

- `client_intent`: “native_v3”

- `client_nonce`: 16–32 bytes random for binding and anti-replay.

### 6.3 MCP_DB_CONNECT_RESP fields (on success)

- `result=OK`

- `handoff_mode=PROXY`

- `proxy_transition`: “IMMEDIATE”

- `server_nonce`: 16–32 bytes

- `dbbt_id`: token identifier (for audit)

- No listener port is revealed externally (because listener is localhost-only).

After sending `MCP_DB_CONNECT_RESP(OK)`, the manager switches to proxy mode for that same TCP connection.

---

## 7. DB Binding Token (DBBT)

DBBT is a **short-lived, single-use** attestation produced by Manager and verified by Listener before it accepts the proxied connection as belonging to DB X.

### 7.1 DBBT Required Properties

- **Integrity protected**: MAC or signature.

- **Bound** to:
  
  - `db_uuid`
  
  - `listener_id` (or listener instance key)
  
  - `expires_at` (tight window)
  
  - `client_nonce` + `server_nonce`
  
  - `manager_session_id` (for auditing)

- **Single-use**: listener must reject reuse.

### 7.2 DBBT Format (normative)

DBBT is a binary blob encoded as:

- `version (u8)`

- `db_uuid (16 bytes)`

- `listener_id (u32)`

- `issued_at (u64 epoch_ms)`

- `expires_at (u64 epoch_ms)`

- `manager_session_id (16 bytes)`

- `client_nonce_len (u16) + client_nonce`

- `server_nonce_len (u16) + server_nonce`

- `flags (u32)` (reserved)

- `mac_len (u16) + mac_bytes`

MAC is computed using a symmetric key **K_mgr_listener** shared between Manager and Listener (local-only secret), using HMAC-SHA-256 or better.

### 7.3 DBBT Validity Rules

Listener MUST reject DBBT if any of:

- expired or not yet valid (clock skew tolerance ≤ 2s recommended)

- db_uuid mismatch with requested binding

- listener_id mismatch

- MAC invalid

- already used (replay)

- manager_session_id not recognized (optional allow-list)

---

## 8. Listener Preface (Local-only)

Before proxying client bytes, Manager opens a local connection to Listener and sends a **Listener Preface** that is not visible to the remote client (because it occurs on the Manager↔Listener local socket before the proxy bridge begins).

### 8.1 Preface Purpose

- Delivers DBBT and binding metadata to Listener.

- Allows Listener to:
  
  - reserve a parser,
  
  - set DB context,
  
  - enforce that the subsequent proxied stream is tied to DB X.

### 8.2 Preface Message

`LPREFACE_V1`:

- `magic = "SBLP"` (4 bytes)

- `version = 1` (u16)

- `listener_id` (u32)

- `dbbt_len (u32)` + `dbbt_bytes`

- `db_selector` (optional alias for logging)

- `requested_profile` (optional)

- `reserved`

Listener replies:

- `LPREFACE_ACK` (success) or `LPREFACE_NACK` (error code)

If `NACK`, Manager MUST NOT enter proxy mode; it MUST terminate the external client connection with an MCP error.

---

## 9. Transition to Proxy Mode

Once the Manager has:

- authenticated the client at MCP level,

- authorized DB selection,

- ensured DB readiness,

- successfully sent `LPREFACE_V1` and received `ACK`,  
  it transitions the external connection to **proxy mode**.

### 9.1 Proxy Mode Definition

- Manager stops interpreting protocol bytes.

- Manager becomes a pure bidirectional byte forwarder between the external client socket and the local listener socket.

### 9.2 Forwarding Requirements

- Bidirectional forwarding MUST preserve byte order and content.

- Manager MUST implement backpressure:
  
  - bounded buffers
  
  - avoid unbounded memory growth

- Manager SHOULD support half-close semantics:
  
  - if client closes write side, forward FIN to listener write side, continue reading until close.

- Manager MUST terminate both sides if:
  
  - DBBT validation fails (listener side closes),
  
  - fatal IO error occurs,
  
  - explicit policy requires immediate termination.

### 9.3 Timeouts

Manager SHOULD enforce:

- MCP auth timeout (pre-proxy)

- DB selection timeout (pre-proxy)

- After proxy: optional idle timeout enforcement (or delegate to listener)

---

## 10. Native v3 Listener Behavior (during proxied connection)

After receiving `LPREFACE_ACK`, Listener MUST:

1. Validate DBBT (see §7.3).

2. Bind the soon-to-arrive client stream to DB UUID.

3. Begin the native v3 handshake **with the client**, which arrives through the proxy.

4. Ensure any parser allocated for the connection is assigned the DB UUID and cannot change it.

### 10.1 Preventing Confused Deputy Inside Listener/Parser

Listener MUST ensure that:

- DB UUID used for this connection is sourced from the validated DBBT, not client-supplied data.

- Any attempt by client to “switch database” at protocol level is rejected unless explicitly supported by policy and re-authorized by manager (default: NOT SUPPORTED).

---

## 11. Database Encryption + Cluster Key Shares (Manager responsibility)

### 11.1 DB States

Manager maintains per-DB state:

- `CLOSED`

- `UNLOCKING`

- `OPENING`

- `OPEN`

- `ERROR`

### 11.2 Unlock policy

- Manager MUST NOT prompt for keys during `LIST_DATABASES`.

- Manager MAY prompt for keys only when:
  
  - client is authenticated,
  
  - client is authorized to request DB details/unlock,
  
  - the DB is selected for connection or an explicit “show details” action.

### 11.3 Key handling requirements

- Decryption keys MUST NOT be persisted to disk by Manager.

- Keys MUST be zeroized on:
  
  - unlock failure,
  
  - DB close,
  
  - manager shutdown

- Keys SHOULD be stored in locked memory if available.

### 11.4 Cluster key shares

If the DB is part of a cluster and uses quorum shares:

- Manager performs the share retrieval and reconstruction.

- Manager MUST audit:
  
  - who requested unlock,
  
  - quorum participants used,
  
  - whether the unlock succeeded.

---

## 12. Authorization Model (Manager-level + Engine-level)

### 12.1 Manager-level authorization

Manager controls:

- which DBs appear in list for a user

- which DBs a user can select

- which DB metadata a user can request

### 12.2 Engine-level authorization

After proxy begins, the native v3 session proceeds normally:

- The parser/engine authenticates user identity for DB session (SCRAM etc.)

- The engine enforces:
  
  - roles, RLS, policies, epochs

- Manager authorization is **necessary but not sufficient**.

This gives you defense-in-depth:

- Manager blocks unauthorized DB selection early.

- Engine remains source of truth for database permissions.

---

## 13. Error Handling

### 13.1 MCP Errors (pre-proxy)

- `AUTH_FAILED`

- `DB_NOT_FOUND`

- `DB_LOCKED` (needs unlock; user not authorized)

- `DB_UNLOCK_REQUIRED` (authorized, but requires action)

- `DB_OPEN_FAILED`

- `CLUSTER_KEY_QUORUM_FAILED`

- `LISTENER_UNAVAILABLE`

- `LISTENER_PREFACE_REJECTED`

- `INTERNAL_ERROR`

### 13.2 Listener Preface Errors

`LPREFACE_NACK` error codes:

- `INVALID_FORMAT`

- `INVALID_DBBT`

- `DB_NOT_READY`

- `DB_NOT_OPEN`

- `POLICY_DENIED`

- `RESOURCE_EXHAUSTED` (no parsers)

- `INTERNAL_ERROR`

### 13.3 Proxy Mode Errors

In proxy mode, Manager cannot send MCP errors. It must:

- close the connection and log an audit event

- optionally send a best-effort “disconnect notice” if the native protocol supports it (not required)

---

## 14. Observability and Audit Requirements

Manager MUST log:

- Manager session creation/auth success/failure

- DB listing requests

- DB selection attempts (success/failure)

- DB unlock/open attempts (including cluster quorum results)

- Listener chosen + listener_id

- DBBT issuance (dbbt_id, expiry, db_uuid)

- Preface ack/nack

- Proxy session start/stop, byte counts, durations, termination reason

Listener SHOULD log:

- Preface received and validation outcome (db_uuid, dbbt_id)

- Parser allocation success/failure

- Connection lifecycle events (start, end, reason)

Engine logs remain as-is (session creation, authkey issuance, etc.).

---

## 15. Security Hardening Checklist (Normative)

1. Listener MUST be bound to localhost/UDS only.

2. Listener MUST require valid DBBT before binding a connection to a DB.

3. DBBT MUST be:
   
   - short-lived (recommended ≤ 30 seconds)
   
   - single-use (replay-protected)
   
   - MACed with a secret not accessible to remote clients

4. Manager MUST enforce DB selection authorization.

5. Manager MUST NOT leak listener endpoints externally.

6. Manager MUST NOT persist decryption keys.

7. All unlock/open operations MUST be audited.

8. No “development allow-all” fallback.

---

## 16. Implementation Notes (Non-normative but practical)

- Start with a single Manager process and one localhost-only Native v3 listener.

- The listener can remain largely unchanged except for:
  
  - adding a localhost-only “preface channel” handshake before it begins reading native protocol bytes,
  
  - binding DB UUID from DBBT into the parser allocation context.

- Manager proxy forwarding can begin as a simple event-loop copy and later be upgraded to zero-copy splice where supported.

---

## 17. Minimal Message Summary

### MCP (public, pre-proxy)

- HELLO

- AUTH (multi-step)

- LIST_DATABASES

- DB_CONNECT (native_v3)

- DB_CONNECT_OK (then switch to proxy)

### Local Preface (manager↔listener only)

- LPREFACE_V1 + DBBT

- LPREFACE_ACK/NACK

### Native v3 (proxied, listener↔client)

- Existing native v3 protocol unchanged.



---

## Companion Spec Bundle for SB-NATIVE-V3 Manager Proxy

**Bundle ID:** SB-NATIVE-V3-MGRPROXY-COMPANION-01  
**Status:** Draft (Implementation-ready)  
**Applies to:** Native v3 only (Manager public port + localhost-only native listeners + parser pools + IPC-only engine)

This companion bundle contains three specs:

1. **DBBT Key Management Spec** (shared secret, rotation, storage, validation)

2. **Manager DB Lifecycle State Machine Spec** (open/unlock/cluster-ready orchestration)

3. **Listener↔Parser Pool Binding Contract Spec** (non-bypassable DB binding into engine sessions)

---

# 1) SB-NATIVE-V3 DBBT Key Management Specification

**Doc ID:** SB-NATIVE-V3-DBBT-KEYMGMT-01

## 1.1 Purpose

Define how Manager and each localhost-only Native v3 Listener share and rotate secrets used to MAC/sign the **DB Binding Token (DBBT)**, and how DBBT replay prevention works.

## 1.2 Entities

- **Manager**: issues DBBTs and maintains key ring.

- **Listener**: validates DBBTs and enforces single-use.

- **Key Ring**: local-only set of active + previous keys for verification.

## 1.3 Keying Model

### 1.3.1 Per-listener key vs global key

**MUST** use **per-listener** shared secret keys:

- `K_mgr_listener[listener_id]`  
  Rationale: limits blast radius; supports independent listener restarts.

### 1.3.2 MAC algorithm

- **MUST** use HMAC-SHA-256 or stronger.

- **MUST** MAC the entire DBBT payload excluding the MAC field itself.

## 1.4 Key Ring Structure

Each listener_id has a key ring:

- `active_key_id` (u32)

- `keys[]`: list of `(key_id, key_bytes, not_before, not_after, status)`

- statuses: `ACTIVE`, `VERIFY_ONLY`, `RETIRED`

**Rules:**

- Manager signs DBBTs **only** with `ACTIVE`.

- Listener verifies DBBTs against:
  
  - `ACTIVE` + `VERIFY_ONLY`

- `RETIRED` keys are not accepted for DBBT verification.

## 1.5 Key Generation

- **MUST** generate keys from cryptographic RNG.

- Key length: **32 bytes minimum** (256-bit).

- Keys **MUST NOT** be derived from user passwords or DB encryption keys.

## 1.6 Local Secret Storage

### 1.6.1 Storage location

- Manager stores the key ring in an **OS-protected local secret store**. Acceptable options:
  
  1. a root-owned file with 0600 permissions
  
  2. OS keychain/KMS (where available)
  
  3. encrypted-at-rest file (encrypted with a host-secret)

### 1.6.2 Process access

- Listeners **MUST NOT** read the key ring from disk directly.

- Preferred: Manager delivers keys to listeners via a **local bootstrap IPC** channel at listener startup (see §1.7).

## 1.7 Listener Bootstrap / Key Provisioning

When a listener starts, it establishes a local-only bootstrap connection to the manager and requests:

- its `listener_id`

- key ring entries for verification (ACTIVE + VERIFY_ONLY)

- replay window parameters

**Bootstrap channel requirements:**

- local-only (UDS or localhost)

- mutual authentication is local-trust; at minimum:
  
  - OS uid check (listener must run as trusted service user)
  
  - optional fixed “listener bootstrap token” on disk readable only by listener

## 1.8 Rotation Policy

### 1.8.1 Rotation trigger

Keys rotate on:

- schedule (e.g., daily/weekly)

- manager restart (optional)

- security event (compromise suspected)

### 1.8.2 Rotation mechanics

- New key is created as `ACTIVE`.

- Previous `ACTIVE` becomes `VERIFY_ONLY` for a grace period.

- Grace period must be ≥ max DBBT expiry + clock skew buffer (recommend: 2 minutes).

- After grace, previous key becomes `RETIRED`.

## 1.9 DBBT Replay Prevention

### 1.9.1 Token identity

Each DBBT includes a `dbbt_id` (u128 recommended) or equivalent uniqueness:

- You can use `(manager_session_id, issued_at, nonce)` to derive uniqueness, but explicit `dbbt_id` is cleaner.

### 1.9.2 Listener replay cache

Listener **MUST** maintain an in-memory replay cache keyed by `dbbt_id`:

- Value: `(expires_at, db_uuid, manager_session_id)`

- On accept: insert immediately; reject any future use until expiry passes.

- Eviction: time-based.

### 1.9.3 Persistence

Replay cache **SHOULD NOT** be persisted (keeps it simple); DBBT TTL is short enough.

## 1.10 Time Semantics

- DBBT expiry **MUST** be short-lived (recommended 10–30 seconds).

- Listener clock skew tolerance: ≤ 2 seconds recommended.

- If host clock is unreliable, allow a slightly larger skew but tighten TTL.

## 1.11 Failure Behavior

- Invalid MAC / expired / replayed DBBT ⇒ listener sends `LPREFACE_NACK(INVALID_DBBT)` and closes local connection attempt.

- Manager must treat that as a hard failure and close external client connection before proxy mode.

---

# 2) SB-NATIVE-V3 Manager DB Lifecycle State Machine Specification

**Doc ID:** SB-NATIVE-V3-MGR-DBLIFECYCLE-01

## 2.1 Purpose

Define how the manager decides what DBs are “available”, orchestrates open/unlock (including cluster key-shares), and presents a stable “DB readiness” contract to listeners and parsers.

## 2.2 Database Registry Model

Manager maintains a registry entry per DB:

**Registry fields (normative minimum):**

- `db_uuid` (stable ID)

- `db_aliases[]` (human names)

- `storage_locator` (path / URI / volume)

- `encryption_state`: `PLAINTEXT | ENCRYPTED | UNKNOWN`

- `cluster_state`: `STANDALONE | CLUSTERED`

- `cluster_id` (if clustered)

- `default_listener_profile` (native v3 profile id)

- `policy_tag` (for selection authorization and disclosure)

- `open_state` (see §2.3)

- `last_error` (structured reason)

## 2.3 DB State Machine

### 2.3.1 States

- `CLOSED`

- `OPENING`

- `OPEN`

- `UNLOCK_REQUIRED` (encrypted and no key available)

- `UNLOCKING`

- `CLUSTER_HANDSHAKE`

- `ERROR`

- `CLOSING`

### 2.3.2 Invariants

- `OPEN` implies:
  
  - DB file is opened by engine/server
  
  - Catalog is accessible
  
  - Manager can route connections to a listener for it

- `UNLOCK_REQUIRED` implies:
  
  - DB is known but cannot be opened until unlock material is acquired

- `CLUSTER_HANDSHAKE` implies:
  
  - DB may be opened locally but cluster security/bootstrap isn’t satisfied yet (policy can decide if read-only is allowed here)

### 2.3.3 Transitions

**CLOSED → OPENING**

- Trigger: authorized client requests connect OR authorized admin requests open.

- Action: start open attempt (non-encrypted) or detect encryption.

**CLOSED → UNLOCK_REQUIRED**

- Trigger: DB marked ENCRYPTED and no cached key material is available.

**UNLOCK_REQUIRED → UNLOCKING**

- Trigger: authorized client explicitly requests connect/details and policy allows unlock flow.

- Action: begin unlock workflow.

**UNLOCKING → OPENING**

- Trigger: unlock material obtained and validated.

- Action: open DB with derived key.

**OPENING → CLUSTER_HANDSHAKE**

- Trigger: DB is clustered and requires handshake before “ready”.

**OPENING → OPEN**

- Trigger: open success for standalone OR cluster does not require additional handshake.

**CLUSTER_HANDSHAKE → OPEN**

- Trigger: cluster handshake succeeded.

**ANY → ERROR**

- Trigger: unrecoverable failure (open fail, quorum fail, key invalid).

- Action: record error, publish status.

**OPEN → CLOSING**

- Trigger: explicit admin close, shutdown, inactivity policy, or cluster requires close.

**CLOSING → CLOSED**

- Trigger: engine confirms close.

## 2.4 Concurrency and Locking

Manager **MUST** enforce a per-db lifecycle lock:

- only one open/unlock/handshake in flight per db_uuid.

- other requests:
  
  - wait (bounded) or
  
  - return `DB_BUSY` with retry guidance.

## 2.5 Unlock Workflow (Encrypted DB)

### 2.5.1 Inputs

Unlock can be sourced from:

- user prompt (interactive)

- cluster key shares (quorum)

- external secret provider (future)

### 2.5.2 Policy gates

Before entering `UNLOCKING`, manager must check:

- user authenticated

- user authorized for “unlock” action on that db_uuid

- step-up requirements satisfied (optional)

### 2.5.3 Key acquisition steps (recommended)

1. Determine required unlock method from registry/policy:
   
   - `PROMPT_LOCAL`
   
   - `CLUSTER_QUORUM`

2. Acquire material:
   
   - Prompt: collect passphrase/key; validate length/format locally.
   
   - Quorum: request shares, validate attestations.

3. Reconstruct DB key:
   
   - key derivation happens **in-memory** only

4. Attempt open:
   
   - If open fails due to key error, go to `UNLOCK_REQUIRED` with `KEY_INVALID` and rate-limit retries.

### 2.5.4 Key lifetime rules

- Manager must **not persist decrypted keys**.

- If DB is open, the key effectively lives inside the engine/db handle; manager should not keep a copy.

- If DB is closed, key is discarded immediately.

## 2.6 Cluster Key Share Retrieval Hook

This spec only defines the manager interface; the cluster protocol can be separate.

**Manager must provide:**

- `request_key_shares(cluster_id, db_uuid, requester_identity, nonce) → shares[]`

- `validate_share_attestations(shares[]) → ok/fail`

- `reconstruct_key(shares[]) → db_key`

**Audit requirements:**

- log who requested

- which members contributed (identifiers only)

- whether quorum satisfied

- whether reconstruction succeeded

## 2.7 “DB Ready” Contract for Listeners

Before manager sends `LPREFACE_ACK` / transitions to proxy mode:

- DB must be in state `OPEN` (or a policy-allowed degraded open mode).

- Manager must provide listener enough metadata to bind:
  
  - `db_uuid`
  
  - `db_profile_id` (optional)
  
  - `policy_epoch` snapshot (optional)

If the DB later transitions from OPEN→CLOSING while active sessions exist, policy decides:

- graceful drain (recommended)

- hard disconnect (only for emergency)

---

# 3) SB-NATIVE-V3 Listener↔Parser Pool Binding Contract Specification

**Doc ID:** SB-NATIVE-V3-LISTENER-PARSER-BINDING-01

## 3.1 Purpose

Ensure that once a manager has selected `db_uuid` and issued DBBT, the listener+parser+engine path cannot be coerced into:

- attaching to a different database,

- or “switching” database context mid-session without manager authorization.

## 3.2 Binding Inputs

Listener receives binding data via `LPREFACE_V1`:

- `db_uuid` (from DBBT)

- `dbbt_id`

- `manager_session_id`

- optional `profile_id`, `policy_epoch`

Listener MUST treat these as authoritative.

## 3.3 Parser Allocation Contract

When listener allocates a parser from its pool, it MUST provide a **Connection Binding Context (CBC)**:

CBC minimum fields:

- `db_uuid`

- `dbbt_id`

- `listener_id`

- `accepted_at`

- `client_addr` (as seen by manager; optional)

- `client_auth_context` (optional; manager auth identity for audit correlation)

CBC MUST be immutable for the lifetime of the connection.

## 3.4 Parser→Engine Attachment Contract

Parser, via server IPC, MUST attach to engine using `db_uuid` from CBC, not from client-supplied fields.

### 3.4.1 Engine attach API requirements

Engine IPC interface must accept:

- `db_uuid`

- `requested_dialect_tag = "scratchbird"` (native v3)

- `session_binding = { dbbt_id, manager_session_id, listener_id }` (for audit correlation)

Engine must:

- confirm db_uuid is currently OPEN

- bind the session to that db_uuid internally

- reject if db_uuid is unknown/closed/unavailable

## 3.5 Session Context Hard Requirements

Once authenticated, engine creates session context:

- `session_id`

- `authkey_id`

- `user_id`

- `policy_epoch_*` snapshot

The engine SHOULD also store:

- `dbbt_id`

- `manager_session_id`

- `listener_id`

This supports:

- auditing “who routed this”

- diagnosing reattach/dormant sessions without ambiguity

## 3.6 Database Switching Semantics

By default, **database switching is NOT SUPPORTED** for native v3.

If you later add “switch database”:

- it MUST be mediated by manager:
  
  - client requests switch to manager (control-plane)
  
  - manager authorizes and issues a new DBBT
  
  - listener rebinds connection only after receiving a fresh `LPREFACE` rebind event

- engine must treat it as “new session” or a well-defined rebinding boundary (strongly recommended: new session)

## 3.7 Failure Modes

### 3.7.1 Parser pool exhaustion

Listener returns `LPREFACE_NACK(RESOURCE_EXHAUSTED)` OR accepts preface but rejects connection early (prefer NACK before proxy).

### 3.7.2 DB closure during active sessions

Listener behavior is policy-driven:

- graceful: allow existing sessions, deny new ones

- drain: notify, then close  
  Engine should enforce consistency regardless.

## 3.8 Observability Contract

Listener MUST emit events:

- preface accepted/rejected (db_uuid, dbbt_id)

- parser allocated (parser_id, db_uuid)

- connection ended (reason)

Engine MUST log session create:

- db_uuid

- user_id

- authkey_id

- (recommended) dbbt_id + manager_session_id

---

## Practical Defaults (Recommended)

If you want a “default secure posture” for native v3:

- DBBT TTL: **15 seconds**

- Replay cache: in-memory, max 1–5 minutes worth of tokens

- Rotation: daily; grace: 2 minutes

- DB open: only on authorized connect; no background opening

- No plaintext password auth in engine; **SCRAM-SHA-256 default**

- Manager auth and engine auth are separate layers (defense-in-depth)
