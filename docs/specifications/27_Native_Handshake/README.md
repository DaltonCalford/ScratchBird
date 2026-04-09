# 27_Native_Handshake

## Purpose
Canonical native connect and authentication handshake contracts for the current ScratchBird listener and parser IPC client path.

## Status
Authoritative, current implementation baseline.

## Current Code-Backed Authority
- `CONNECT_REQUEST` / `CONNECT_RESPONSE` session establishment.
- `AUTH_CHALLENGE` / `AUTH_RESPONSE` authentication negotiation.
- `FEATURE_AUTH_PLUGIN_REGISTRY` method-slot negotiation in `AUTH_CHALLENGE` payload version `2`.
- MFA continuation carried through auth-response continuation payloads.

## Explicit Boundaries
- `HELLO_REQ` / `HELLO_RSP` transcript language is not current implementation proof.
- Handshake-phase database registry exchange is not current implementation proof.
- Forensic replay connection negotiation is not current implementation proof.
- Fabric-channel multiplex handshake semantics are not current implementation proof.
- Derivative queue, shadow-group, restore-boundary, and failback-boundary
  inspection do not create a separate current handshake family.

## Section Entry Points
- `SPEC_OUTLINE.md`
- `HANDSHAKE_MESSAGE_SCHEMAS.md`
- `AUTH_NEGOTIATION_AND_POLICY.md`
- `HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md`
- `REGISTRY_EXCHANGE_AND_VISIBILITY.md`
- `FORENSIC_REPLAY_CONNECTION_NEGOTIATION.md`
- `TEST_CONTRACT.md`

## Links
- Back to root index: [../README.md](../README.md)

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [AUTH_NEGOTIATION_AND_POLICY.md](AUTH_NEGOTIATION_AND_POLICY.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [FORENSIC_REPLAY_CONNECTION_NEGOTIATION.md](FORENSIC_REPLAY_CONNECTION_NEGOTIATION.md)
- [HANDSHAKE_MESSAGE_SCHEMAS.md](HANDSHAKE_MESSAGE_SCHEMAS.md)
- [HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md](HANDSHAKE_STATE_MACHINE_AND_FAILURE_MATRIX.md)
- [REGISTRY_EXCHANGE_AND_VISIBILITY.md](REGISTRY_EXCHANGE_AND_VISIBILITY.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh` when numbered section files change.

## Hardening promotion note (2026-03-28)
- Current handshake authority is explicitly limited to `CONNECT_*` plus `AUTH_*`.
- Auth plugin registry negotiation is explicitly current through `FEATURE_AUTH_PLUGIN_REGISTRY` and connection-local `method_slot` selection.
- Database-registry exchange, replay negotiation, and fabric handshake profiles remain fail-closed or `target_state_only`.
- Administrative status surfaces for derivative queues and shadow groups are
  post-auth command capabilities, not separate handshake transcript families.

## Audit lookup anchors

Representative section-27 audit anchors are:
- `buildAuthChallenge(`
- `FEATURE_AUTH_PLUGIN_REGISTRY`
- `issue_mfa_challenge_if_required(`
