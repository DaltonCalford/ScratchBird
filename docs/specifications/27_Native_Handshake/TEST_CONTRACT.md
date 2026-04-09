# Test Contract - 27_Native_Handshake

## Current Proof Contract
The current section `27` proof set must stay aligned with the implemented connect/auth negotiation path.

### Current required proof lanes
1. `CONNECT_REQUEST` / `CONNECT_RESPONSE` success and rejection behavior.
2. Auth negotiation returns `AUTH_CHALLENGE` and enforces allowed or required method selection.
3. Registry-capable auth negotiation returns stable `AuthMethodRegistryEntry` rows and validates slot selection.
4. MFA continuation through `AUTH_RESPONSE` continuation status and follow-up `AUTH_REQUEST` is validated.
5. Invalid negotiation payloads, slot mismatches, transport denials, and auth failures reject deterministically.
6. Connect handling rejects when required database-open or binding preconditions fail.
7. Administrative derivative or shadow status surfaces are reachable only after
   successful authentication and remain ordinary post-auth command capability,
   not a separate handshake family.

## Target-State-Only Test Areas
The following remain future-work or neighboring-section scope rather than current section `27` proof:
- handshake-time database registry exchange suites
- replay-capable handshake negotiation suites
- fabric-channel multiplex handshake suites
- full canonical `HS-*` failure-matrix conformance suites
- `HELLO_REQ` / `HELLO_RSP` transcript conformance suites
- `SESSION_OPEN_*` handshake-phase suites
- dedicated derivative-status or shadow-status handshake negotiation suites

## Gate Criteria
Current stage advancement for this section depends on keeping the test contract aligned with the code-backed connect/auth negotiation lane and explicitly excluding target-state-only handshake phases.

## Hardening promotion note (2026-03-28)
- Gate authority is now aligned to current connect/auth negotiation, registry-capable auth-method selection, MFA continuation, and connect preconditions.
- Replay, database-registry, and fabric handshake suites remain future-state only.
