# Handshake Message Schemas

## Purpose
Document the current code-backed native handshake transcript and payload shapes.

## Current Transcript Authority
The live transcript is:
1. `CONNECT_REQUEST`
2. `CONNECT_RESPONSE`
3. `AUTH_REQUEST` used as negotiation starter
4. `AUTH_CHALLENGE`
5. one or more `AUTH_REQUEST` / `AUTH_RESPONSE` exchanges
6. optional MFA continuation carried in `AUTH_RESPONSE` continuation data and a follow-up `AUTH_REQUEST`

## Current Message Families
### `CONNECT_REQUEST` / `CONNECT_RESPONSE`
Current proof includes:
- connection establishment
- session id assignment
- connect flags
- bounded database binding and dormant-reattach adjunct data

Current boundary:
- This is the current connection-establishment family.
- It is not the same as the broader future `HELLO_REQ` / `HELLO_RSP` family used in older prose.

### `AUTH_CHALLENGE`
Current code-backed fields include:
- `session_id`
- `username`
- payload version (`1` or `2`)
- `allowed_methods[]`
- `has_required_method`
- `required_method`
- `allowed_transport_mask`
- reserved byte
- `challenge_nonce`
- optional auth-method registry list when payload version is `2`

### `AUTH_METHOD_REGISTRY_ENTRY`
Current registry-entry fields:
- `method_slot:u16`
- `method_id:string`
- `has_legacy_wire_code:bool`
- `legacy_wire_code:u32`

Rules:
1. `method_slot` is connection-local.
2. `method_id` is the canonical registry identifier carried for the current bounded registry path.
3. `legacy_wire_code` keeps compatibility with the currently shipped auth-method enum.

### Auth Registry Selection Payload
Current client selection payload is bounded and code-backed.

Current structure:
- magic prefix `SBAPR1`
- selected `method_slot:u16`
- auth-method payload bytes

### MFA Continuation Payload
Current MFA continuation is code-backed but bounded.

Current proof:
- challenge and response payloads are carried as auth-response/auth-request continuation data
- the client and server both recognize the current MFA payload marker and challenge id flow

Current boundary:
- MFA continuation is not a distinct top-level message family in the current wire authority.

## Not Current Message Authority
The following message families are not current code-backed handshake proof here:
- `HELLO_REQ`
- `HELLO_RSP`
- `REGISTRY_REQ`
- `REGISTRY_RSP`
- `HANDSHAKE_COMPLETE`
- `SESSION_OPEN_REQ`
- `SESSION_OPEN_RSP`
- `HANDSHAKE_ABORT`

## Connection Identity Boundary
Current proof includes session establishment and authenticated connection progression.

Current boundary:
- the richer canonical connection/session/transaction identity stack, replay capability attachment, and fabric-session multiplex identity model are not fully proven as current handshake-message authority in this section.
- derivative queue, shadow-group, restore-boundary, and failback-boundary
  status surfaces are not negotiated as distinct handshake payload families
  under current authority

## Administrative capability boundary

Current rule:
- derivative queue inspection
- shadow-group inspection
- restore-boundary inspection
- failback-boundary inspection

are post-auth command or status capabilities. They do not require a dedicated
`CONNECT_RESPONSE` extension family or a new top-level handshake message under
current authority.

Negative requirements:
- do not invent a dedicated `ADMIN_CAPABILITIES` handshake family here
- do not advertise derivative or shadow status as replay-capable handshake proof

## Hardening promotion note (2026-03-28)
- `CONNECT_*` and `AUTH_*` remain the canonical shipped message families for current proof.
- `HELLO_*`, `REGISTRY_*`, `HANDSHAKE_COMPLETE`, and `SESSION_OPEN_*` remain target-state transcript families only.
