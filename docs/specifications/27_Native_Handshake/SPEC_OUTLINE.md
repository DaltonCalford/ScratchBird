# Spec Outline - 27_Native_Handshake

## Purpose
Define the current code-backed connection and authentication handshake for the native listener and parser IPC client path.

## Current Scope
- `CONNECT_REQUEST` and `CONNECT_RESPONSE` connection establishment.
- `AUTH_CHALLENGE` and `AUTH_RESPONSE` method negotiation.
- Auth plugin registry negotiation through `FEATURE_AUTH_PLUGIN_REGISTRY`.
- MFA continuation through auth-response continuation payloads.
- Bounded connect-time preconditions such as database-open and ingress checks.

## Explicitly Bounded or Target-State Scope
- `HELLO_REQ` / `HELLO_RSP` transcript families.
- Handshake-phase database registry exchange and visibility filtering.
- Replay-capable connection negotiation during handshake.
- Fabric-channel multiplex handshake and channel-purpose transcript variants.
- `HANDSHAKE_COMPLETE` and `SESSION_OPEN_*` transcript phases.

## Authoritative Files
- `HANDSHAKE_MESSAGE_SCHEMAS.md`
- `AUTH_NEGOTIATION_AND_POLICY.md`
- `HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md`
- `REGISTRY_EXCHANGE_AND_VISIBILITY.md`
- `FORENSIC_REPLAY_CONNECTION_NEGOTIATION.md`
- `DECISION_RECORD.md`
- `TEST_CONTRACT.md`

## Invariants
1. Current handshake proof is anchored to `CONNECT_*` plus `AUTH_*`, not to a broader future transcript family.
2. Auth-method registry slots are connection-local selectors carried in `AUTH_CHALLENGE` payload version `2`.
3. MFA continuation is real, but it is carried inside the auth round-trip rather than through distinct top-level handshake message ids.
4. Database-registry exchange, replay negotiation, and fabric-channel handshake semantics remain bounded or target-state-only.

## Dependencies
- Upstream: sections `19`, `26`.
- Shared boundaries: sections `24`, `25`, `29`.
- Downstream: sections `28`, `29`, `30`, `31`.

## Completeness Criteria
1. The documented transcript matches the current protocol codec and client/server implementation.
2. Method-registry negotiation and slot selection are described exactly as implemented.
3. MFA continuation is documented as the current bounded continuation path.
4. Unimplemented handshake phases are kept explicit and fail-closed.

## Open Questions
- None for the current `partial` normalization pass.

## Hardening promotion note (2026-03-28)
- Section `27` now carries explicit capability-state vocabulary around transcript authority, auth-registry negotiation, MFA continuation, and connect/session preconditions.
- Broader transcript-family, replay, database-registry, and fabric-handshake narratives remain outside the current code-backed authority set.
