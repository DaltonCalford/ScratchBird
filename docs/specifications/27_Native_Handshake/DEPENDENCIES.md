# Dependencies - 27_Native_Handshake

## Upstream Dependencies
- `19_Security_Model`
- `26_Native_Wire_Protocol`

## Shared Boundaries
- `24_Catalog_Model_and_Virtual_Overlays`
- `25_Runtime_Modes`
- `29_Listener_and_Server_Orchestration`

## Downstream Dependents
- `28_Parser_Implementations`
- `29_Listener_and_Server_Orchestration`
- `30_Client_Tooling`
- `31_Conformance_Performance_and_Reliability_Gates`

## Cross-Section Contracts
1. Message ids, connect flags, auth challenge payload format, and message framing come from section `26`.
2. Auth policy evaluation, MFA policy, and method/plugin ownership come from section `19`.
3. Database-open and server-session orchestration boundaries are shared with section `29`.
4. Replay policy and forensic scope are shared with sections `19` and `26`, but are not current code-backed handshake proof here.
5. Fabric-channel runtime semantics are owned by cluster/runtime sections rather than this section's current handshake proof.

## External References
- None required for the current `partial` authority set.

## Hardening promotion note (2026-03-28)
- Section `27` owns the current connect/auth negotiation lane only.
- Database-registry exchange, replay negotiation, and fabric runtime semantics remain shared or neighboring-section-owned until separately proven.
