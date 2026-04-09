# Forensic Replay Connection Negotiation

## Status
`target_state_only`

## Purpose
Reserve the future handshake-time replay negotiation surface without overstating current implementation.

## Current Boundary
The current code-backed handshake in this section does not prove:
- replay-purpose declaration during handshake
- replay-capable handshake admission or denial
- replay capability response fields in the connect/auth transcript
- replay-specific handshake state transitions

## Shared Ownership
- replay policy ownership remains shared with section `19`
- replay wire/session profile ownership remains shared with section `26`
- any later replay binding or replay-session context outside the initial handshake does not upgrade this section to current handshake proof

## Promotion Gate
This file remains `target_state_only` until explicit code-backed handshake transport, state-machine, and failure-path evidence exists for replay negotiation.

## Hardening promotion note (2026-03-28)
- Replay-capable handshake negotiation remains `target_state_only`.
- No current connect/auth transcript evidence upgrades this file beyond shared-boundary planning status.
