# Section 26 Test Contract

Section `26` is implementation-ready only if maintained evidence covers the
current native and IPC transport behaviors it claims.

## Required certification lanes

### Header conformance

- native header field sizes and offsets match the 12-byte `SBDB` contract
- IPC header field sizes and offsets match the 40-byte `SBIP` contract
- native and IPC magic or version mismatches fail closed
- oversized payloads fail closed

### Message catalogs

- native message ids round-trip for current active families
- IPC message ids round-trip for current active families
- declared payload schemas match the current header-defined payload structures

### Result and error transport

- native result frames follow the documented sequence
- IPC result frames follow the documented sequence
- native `QUERY_ERROR` and `PROTOCOL_ERROR` remain distinct
- IPC `ERROR_RESPONSE` and `NOTICE` remain distinct
- manager `STATUS_RESPONSE` rows expose deterministic heartbeat, readiness,
  drift, and queue keys for the bounded control surface
- derivative queue-state, shadow-group status, restore-boundary status, and
  failback-boundary status surfaces use the documented existing result or status
  paths with stable field names or fixed row contracts

### Copy and streaming

- declared copy or stream message families are encoded and decoded according to
  current header contracts
- `STREAM_CONTROL` behavior follows the documented current contract
- unsupported parity or extended-stream claims are not surfaced as supported
  behavior
- no dedicated derivative or shadow status stream family is surfaced as current
  authority

## Excluded lanes

The following are not section `26` certification requirements in the current
tree:
- forensic replay wire negotiation
- parserless cluster-fabric transport
- distributed-read and telemetry transport expansion
