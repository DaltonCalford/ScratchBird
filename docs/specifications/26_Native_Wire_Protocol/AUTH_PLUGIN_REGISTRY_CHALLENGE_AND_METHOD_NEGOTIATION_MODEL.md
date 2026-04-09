# Auth Plugin Registry Challenge and Method Negotiation Model

## Purpose

Define the protocol-visible method registry and its relationship to authentication challenge negotiation.

## Challenge Payload Contract

The authentication challenge may carry:

- session identifier
- username
- allowed methods
- required method flag
- required method
- transport mask
- nonce
- optional registry entries

## Registry Entry Fields

Each registry entry carries:

- method slot
- method identifier
- legacy-wire-code presence flag
- legacy-wire-code value

## Purpose of Registry Entries

Registry entries let the client correlate the runtime plugin-registry method identity with the public challenge surface. This avoids ambiguous method negotiation when multiple plugin-backed methods exist.

## Negotiation Rules

1. The server advertises allowed methods and any required method.
2. The server may include registry entries for plugin-backed methods.
3. The client shall choose only from the advertised allowed methods.
4. The client shall obey a required method when one is present.
5. Unknown registry entries do not widen client authority; they only describe server-supported methods.

## Relationship to Auth Policy

Method negotiation is constrained by the resolved authentication policy. The protocol challenge is the runtime projection of that policy decision, not an independent policy source.

## Current Proof and Rebuild Boundary

Current code proves:

- challenge round-trip with registry entries
- method identifiers carried in the challenge
- legacy wire-code compatibility projection

This specification reconstructs the canonical public method-registry negotiation contract for plugin-backed authentication.
