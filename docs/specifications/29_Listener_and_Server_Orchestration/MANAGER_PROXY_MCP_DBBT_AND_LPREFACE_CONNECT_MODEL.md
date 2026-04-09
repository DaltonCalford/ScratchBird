# Manager Proxy MCP DBBT and LPREFACE Connect Model

## Purpose

Define the optional manager proxy connect path, including scoped database admission, DBBT issuance, LPREFACE validation, and proxy transport behavior.

## Scope

This model applies only to the optional ScratchBird manager front-door path. It does not redefine native direct listener attach.

## Manager Proxy Preconditions

Before manager-scoped database connect:

1. the client must complete manager authentication
2. the requested database must be within manager proxy scope
3. the requested client intent must be supported
4. client nonce bounds must validate when provided

## DBBT Model

The manager issues a database binding token with:

- database UUID
- listener identity
- issuance and expiry times
- manager session identity
- client nonce
- server nonce
- flags
- signature

The token is encoded and assigned a stable token identity.

## LPREFACE Model

The manager packages:

- listener identity
- encoded DBBT
- database selector
- requested profile
- flags

into a listener preface payload.

## Validation Sequence

When a listener management channel exists, the manager shall:

1. issue the DBBT
2. encode LPREFACE
3. submit `LPREFACE_VALIDATE` on the listener management channel
4. require explicit listener acknowledgement
5. refuse connect if validation fails or is rejected

If no listener management channel is configured, the manager may route directly to proxy mode without DBBT or LPREFACE validation.

## Proxy Transport

After successful managed connect, the manager establishes the upstream native connection and proxies bytes bidirectionally until both directions are drained or one side closes and buffers empty.

The proxy session drains in a bounded way; it does not wait indefinitely for additional client activity once the upstream has closed and all buffered data is flushed.

## Audit Boundary

DBBT issuance and LPREFACE decision points are auditable management events. Success and refusal are both recorded.

## Current Proof and Rebuild Boundary

Current code proves:

- manager authentication before connect
- database-owner scope restriction
- DBBT issuance
- LPREFACE validation
- managed proxy transport handoff

This specification reconstructs the canonical managed-connect path and keeps it distinct from direct listener attach.
