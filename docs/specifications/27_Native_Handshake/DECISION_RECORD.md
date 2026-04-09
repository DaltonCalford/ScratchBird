# Decision Record - 27_Native_Handshake

## Scope
- Connection establishment.
- Authentication negotiation.
- Auth plugin registry negotiation.
- MFA continuation.

## Decisions
1. **Current transcript authority is `CONNECT_*` plus `AUTH_*`**
   - The live listener and client do not implement the broader `HELLO_REQ` / `HELLO_RSP` transcript as current proof.

2. **Auth plugin registry negotiation is current proof**
   - `FEATURE_AUTH_PLUGIN_REGISTRY`, `AuthMethodRegistryEntry`, and connection-local `method_slot` selection are treated as the current code-backed registry lane.

3. **MFA is a bounded auth continuation, not a distinct top-level handshake family**
   - MFA challenge and response continue through `AUTH_RESPONSE` continuation data and a follow-up `AUTH_REQUEST` payload.

4. **Policy-backed auth selection is current proof**
   - The server enforces allowed methods, required method, transport mask, peer mode, and MFA policy through the auth negotiation path.

5. **Database-registry exchange is not current proof**
   - The code-backed registry lane is auth-method registry negotiation only; handshake-time database listing remains target-state-only.

6. **Replay and fabric handshake narratives are not current proof**
   - Replay-capable connection negotiation and fabric-channel multiplex handshake semantics remain bounded to neighboring sections or future work.

7. **Connect-time database-open gating is real but bounded**
   - A generic database-open precondition exists during connect handling, but the richer parser-specific open-database gate language is not fully proven here.

## Rejected Alternatives
1. Treating `HELLO_REQ` / `HELLO_RSP` as current shipped authority.
2. Treating handshake-phase database registry exchange as implemented.
3. Treating replay or fabric-channel handshake semantics as current implementation proof.

## Invariants Confirmed
- Authentication remains security-gated before authenticated session use.
- Registry-capable auth negotiation remains subordinate to the current protocol codec.
- Current handshake proof remains narrower than the target-state spec language it replaces.

## Open Items
- None for the current normalization pass.

## Hardening promotion note (2026-03-28)
- `CONNECT_REQUEST` / `CONNECT_RESPONSE` plus `AUTH_CHALLENGE` / `AUTH_RESPONSE` are the only current transcript authority.
- Auth-registry slot negotiation is current proof.
- Replay and fabric handshake semantics require separate future promotion gates.
