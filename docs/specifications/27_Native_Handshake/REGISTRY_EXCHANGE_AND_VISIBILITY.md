# Registry Exchange and Visibility

## Purpose
Bound handshake-time registry language to what is currently proven in code.

## Current Code-Backed Registry Lane
The only current handshake registry proof in this section is auth-method registry negotiation.

Current authority anchors:
- server builds `AuthMethodRegistryEntry` rows during auth negotiation
- auth registry entries are carried in `AUTH_CHALLENGE` payload version `2`
- client selects a registry slot during the follow-up auth request

Current fields:
- `method_slot`
- `method_id`
- `legacy_wire_code`

## Not Current Code-Backed Registry Lane
Handshake-time database registry exchange is not current implementation proof.

Not proven here:
- `REGISTRY_REQ`
- `REGISTRY_RSP`
- handshake-time database listing payloads
- visibility filtering for database registry exchange
- deterministic ordering and hidden-entry leakage guarantees for a handshake database registry
- emulation filter negotiation in the handshake path

## Boundary Rule
- Auth-method registry negotiation is current and authoritative.
- Database-registry exchange remains target-state-only and must not be promoted from terminology or neighboring catalog surfaces alone.

## Hardening promotion note (2026-03-28)
- The only current handshake registry lane is auth-method registry negotiation in `AUTH_CHALLENGE` payload version `2`.
- Database-registry exchange remains `target_state_only`.
