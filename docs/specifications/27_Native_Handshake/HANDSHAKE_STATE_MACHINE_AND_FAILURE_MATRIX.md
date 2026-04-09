# Handshake State Machine and Failure Matrix

## Purpose
Define the current code-backed handshake state progression and bounded failure handling.

## Current Code-Backed State Shape
1. `CONNECT_PENDING`
2. `CONNECTED_UNAUTHENTICATED`
3. `AUTH_NEGOTIATION_READY`
4. `AUTH_PRIMARY`
5. `AUTH_MFA_CONTINUE` (optional)
6. `AUTHENTICATED`
7. `ABORTED`

## Current Transitions
- `CONNECT_PENDING` -> `CONNECTED_UNAUTHENTICATED` on valid `CONNECT_REQUEST` and successful `CONNECT_RESPONSE`.
- `CONNECTED_UNAUTHENTICATED` -> `AUTH_NEGOTIATION_READY` after the server issues `AUTH_CHALLENGE`.
- `AUTH_NEGOTIATION_READY` -> `AUTH_PRIMARY` on selected-method `AUTH_REQUEST`.
- `AUTH_PRIMARY` -> `AUTH_MFA_CONTINUE` when server returns `AUTH_RESPONSE` with `CONTINUE`.
- `AUTH_PRIMARY` -> `AUTHENTICATED` on auth success without MFA continuation.
- `AUTH_MFA_CONTINUE` -> `AUTHENTICATED` on successful continuation response.
- any active state -> `ABORTED` on protocol violation, policy rejection, or auth failure.

## Current Failure Authority
Current code-backed failure lanes include:
- invalid connect payload
- missing or mismatched bound database uuid where required
- ingress-policy rejection
- database not open
- auth negotiation missing or user mismatch
- transport denied for selected negotiation policy
- invalid auth method or registry slot selection
- credential failure
- MFA payload failure or MFA retry exhaustion

## Current Boundary
- The current implementation does not prove the full canonical `HS_INIT` / `HS_HELLO_EXCHANGED` / `HS_REPLAY_POLICY` / `HS_REGISTRY_EXCHANGE` state family.
- The current implementation does not prove the full canonical `HS-PROT-*`, `HS-AUTH-*`, `HS-FORENSIC-*`, and `HS-REG-*` matrix as a complete stable emitted wire-code set.
- Replay-policy, handshake-phase database registry exchange, and post-handshake session-open phases are not current state-machine proof.

## Timeout Boundary
The detailed per-phase timeout grid in older prose is not current code-backed authority in this section.

## Parser and Fabric Boundary
- Generic connect/auth handling is current proof.
- Parser-id claim enforcement, replay-specific transitions, and fabric-channel multiplex state semantics are not current code-backed handshake-state proof here.

## Administrative capability transition boundary

After `AUTHENTICATED`, the session may proceed to ordinary command execution and
post-auth administrative status requests.

Current rule:
- derivative queue, shadow-group, restore-boundary, and failback-boundary
  inspection are post-auth command capabilities
- denial of those operations is an authorization or command-scope failure after
  handshake, not a separate handshake-state transition
- handshake success alone does not authorize those operations

## Hardening promotion note (2026-03-28)
- Current state authority is bounded to connect, auth negotiation, optional MFA continuation, and authenticated transition.
- The broader `HS-*` canonical matrix remains fail-closed outside the currently shipped denial and continuation paths.
