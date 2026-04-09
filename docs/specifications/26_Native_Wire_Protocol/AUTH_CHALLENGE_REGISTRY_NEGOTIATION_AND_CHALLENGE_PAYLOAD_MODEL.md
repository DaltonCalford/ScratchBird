# Auth Challenge Registry Negotiation and Challenge Payload Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the native wire protocol contract for:
- `AUTH_CHALLENGE`
- negotiated auth method registry entries
- required-method and allowed-transport publication
- downgrade compatibility between canonical auth plugin method identifiers and legacy wire codes

## Canonical role

The native auth challenge is the wire-level publication point for:
- server-chosen challenge session identity
- server-published allowed auth methods
- optional required auth method
- allowed transport mask
- challenge nonce
- optional canonical auth method registry

It is not the authority for plugin admission policy. That remains in section `19`.

## Current payload layout

The current `AUTH_CHALLENGE` payload layout is:

1. `session_id`
  - `16` bytes
2. `username`
  - null-terminated string
  - bounded to `64`
3. `payload_version`
  - `uint8`
  - current allowed values:
    - `1`
    - `2`
4. `method_count`
  - `uint8`
  - current valid range:
    - `1..16`
5. `allowed_methods`
  - `method_count` entries
  - each `uint8`
6. `has_required_method`
  - `uint8`
7. `required_method`
  - `uint8`
8. `allowed_transport_mask`
  - `uint8`
9. `reserved`
  - `uint8`
10. `nonce_len`
  - `uint16`
11. `challenge_nonce`
  - `nonce_len` bytes
12. optional registry block, version `2` only:
  - `registry_count`
    - `uint16`
  - repeated registry entries

## Known auth methods for wire negotiation

Current known wire-level auth methods are:
- `PASSWORD`
- `MD5`
- `SCRAM_SHA_256`
- `SCRAM_SHA_512`
- `TOKEN`
- `PEER`

An unknown auth method value is a protocol violation.

## Version rules

- version `1`
  - challenge contains no registry block
- version `2`
  - challenge may include an auth method registry block

Any version other than `1` or `2` is a protocol violation.

## Registry entry model

Each `AuthMethodRegistryEntry` contains:
- `method_slot`
  - `uint16`
  - stable selector for the negotiated flow
- `method_id`
  - canonical auth plugin method identifier
  - `scratchbird.auth.*`
- `has_legacy_wire_code`
  - boolean conveyed through registry flags
- `legacy_wire_code`
  - `uint32`
  - valid only when the legacy-wire flag is present

Current binary layout for each version-2 registry entry is:
1. `method_slot`
  - `uint16`
2. `registry_flags`
  - `uint16`
3. `legacy_wire_code`
  - `uint32`
4. `method_id_len`
  - `uint16`
5. `method_id`
  - `method_id_len` bytes

Current flag meaning:
- bit `0`
  - legacy wire code is present

## Downgrade compatibility rule

Canonical method identity is the `method_id`, not the legacy wire code.

The legacy wire code exists only for downgrade compatibility.
The server and client must prefer the canonical method identifier whenever the registry block is available.

## Fail-closed parser rules

The current parser must reject on:
- truncated session ID
- invalid username field
- truncated header
- invalid payload version
- zero method count
- method count greater than `16`
- unknown allowed-method value
- truncated policy block
- invalid required method when `has_required_method` is true
- truncated nonce length or nonce bytes
- truncated registry count or registry entry fields
- truncated `method_id`

These are protocol violations, not soft warnings.

## Round-trip requirement

Current tested runtime already requires `AUTH_CHALLENGE` round-trip preservation for:
- session ID
- username
- allowed methods
- required-method state
- transport mask
- nonce
- registry entry slot
- registry entry method identifier
- registry legacy-wire compatibility fields

That round-trip contract is canonical.

## Allowed-transport rule

The challenge publishes an `allowed_transport_mask`.
Clients and adapters must not select a method whose required transport posture is outside the allowed mask.

The transport mask is part of the auth challenge policy block, not an informational hint.

## Required-method rule

When `has_required_method` is true:
- the client must treat `required_method` as mandatory policy
- a different selected method is non-conforming

This wire-level policy aligns with the higher-level client-pinning and method-pinning rules in section `19`.

## Canonical split with section 19

Section `26` owns:
- binary payload layout
- parser and serializer rules
- wire downgrade compatibility
- protocol-violation semantics

Section `19` owns:
- plugin admission policy
- signer trust
- built-in registry identity
- auth-type to method binding
- direct-login refusal and pinning semantics
