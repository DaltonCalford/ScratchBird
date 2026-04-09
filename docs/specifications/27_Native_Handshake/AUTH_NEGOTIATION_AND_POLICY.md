# Authentication Negotiation and Policy

## Purpose
Define the current code-backed authentication negotiation and policy enforcement path used by the native listener/client and parser IPC client path.

## Current Code-Backed Method Set
The current protocol enum and live client/server implementation prove these negotiated methods:
- `PASSWORD`
- `MD5`
- `SCRAM_SHA_256`
- `SCRAM_SHA_512`
- `TOKEN`
- `PEER`

These methods are transported as protocol enum values and may also be exposed through plugin-style method ids during registry-capable negotiation.

## Current Negotiation Model
1. Client connects with `CONNECT_REQUEST` and receives `CONNECT_RESPONSE`.
2. Client sends an initial `AUTH_REQUEST` to start negotiation.
3. Server responds with `AUTH_CHALLENGE` containing:
   - allowed methods
   - optional required method
   - allowed transport mask
   - challenge nonce
   - optional auth-method registry entries
4. Client selects a permitted method.
5. Client sends a follow-up `AUTH_REQUEST` with the selected method and method payload.
6. Server returns `AUTH_RESPONSE` with `OK`, `FAILURE`, or `CONTINUE`.

## Auth Plugin Registry Negotiation
Current code-backed registry negotiation is real and bounded.

Gate:
- `FEATURE_AUTH_PLUGIN_REGISTRY`

Current authority fields:
- `method_slot`
- `method_id`
- `legacy_wire_code`

Current behavior:
1. Server builds registry entries from allowed protocol methods and allowed plugin-style method ids.
2. Each registry entry is connection-local for the life of the negotiation.
3. Client resolves the selected protocol method to a negotiated `method_slot`.
4. Client prepends the slot-selection payload marker and selected slot to the auth payload.
5. Server validates slot, legacy compatibility mapping, and method selection before continuing auth.

Current boundary:
- Registry-capable negotiation is proven only for methods that still map back to the current protocol auth-method enum.
- Broad plugin-only handshake parity is not proven here.

## Current Policy-Backed Inputs
The live server auth negotiation path proves these policy-backed inputs or outputs:
- `allowed_auth_method_mask`
- `allowed_auth_method_ids`
- `has_required_auth_method`
- `required_auth_method`
- `allowed_transport_mask`
- `peer_mode`
- `allow_password_fallback`
- `mfa_required`
- `mfa_policy_id`
- `client_pinning_required_methods`
- `client_pinning_forbidden_methods`
- `client_pinning_require_channel_binding`

Current boundary:
- These are current server policy surfaces.
- Rich client-declared method-id and policy-hint transport across a generalized handshake transcript is not fully proven here.

## MFA Continuation
MFA is current code-backed behavior, but its transport is bounded.

Current proof:
- Server may return `AUTH_RESPONSE` with `AuthStatus::CONTINUE`.
- Client parses the MFA challenge payload and resubmits another `AUTH_REQUEST`.
- Server tracks pending MFA state and verifies continuation responses.
- Session MFA verification state is maintained after successful completion.

Current boundary:
- MFA is proven as a continuation in the auth round-trip.
- The broader spec language about a dedicated top-level `MFA_CHALLENGE` / `MFA_RESPONSE` transcript family is not current wire authority.

## Connect-Time Precondition
A bounded pre-auth precondition is current proof:
- server connect handling rejects when the target database is not open.

Current boundary:
- This is a generic connect-time database-open precondition.
- The richer parser-specific `no_open_database` handshake policy language and exact canonical `HS-AUTH-*` failure mapping are not fully proven here.

## Post-auth administrative boundary

Authentication establishes the authenticated session only.

Required rule:
- derivative queue inspection
- shadow-group inspection
- restore-boundary inspection
- failback-boundary inspection

are authorized after authentication as ordinary privileged command surfaces.

Negative requirements:
- authentication success does not imply authorization for derivative mutation or
  shadow route mutation
- these surfaces must not be advertised as replay-capable handshake negotiation
  proof

## Explicit Non-Guarantees
- Full caller-supplied auth policy-hint transport is not guaranteed.
- Canonical `scratchbird.auth.*` method-id negotiation for methods without current enum mapping is not guaranteed.
- SCRAM channel binding is not guaranteed and is currently bounded.
- Full canonical `HS-AUTH-*` deterministic failure-code coverage is not guaranteed.
- Fabric-channel mTLS policy narratives are not guaranteed as current handshake proof in this section.

## Hardening promotion note (2026-03-28)
- Current authority is split between enum-backed auth negotiation, registry-capable method-slot selection, and MFA continuation.
- Plugin-only parity beyond the current enum-backed method surface remains fail-closed.
