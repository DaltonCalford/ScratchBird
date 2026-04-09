# Principal Account Source Scope and Shared Rights Identity Model

## Purpose

Define how presented principals are resolved to concrete security accounts and how that identity model supports shared-rights behavior.

## Principal Account Tuple

A principal account is resolved from a tuple that includes:

- presented principal name
- principal kind
- source scope kind
- optional source scope value
- optional authentication database context
- optional tenant context

## Source Scope Kinds

Current code-backed source scope kinds include:

- `ANY`
- `HOST_EXACT`
- `HOST_WILDCARD`
- `CIDR`
- `PEER_UID`
- `PEER_GID`

Additional peer and socket context may participate where the transport provides it.

## Deterministic Resolution Ranking

Resolution shall select the most specific matching account tuple.

Current proof shows the ranking pattern:

1. exact host and matching scoped tuple
2. wildcard host tuple
3. `ANY`

Peer-credential exact scopes outrank weaker generic scopes when present.

## Ambiguity and Validation

The engine shall fail closed when:

- no matching principal tuple exists
- multiple tuples match with equivalent specificity
- a source scope value is malformed, such as invalid CIDR or invalid peer numeric value
- the tuple duplicates an existing canonical identity tuple

## Shared Rights Relationship

Resolved account identity is the root for:

- direct user grants
- effective role membership
- effective group membership
- auth policy binding
- MFA policy binding

This is the basis for shared-user and shared-rights behavior. Rights flow from the resolved security account and its membership closure, not from the raw transport username alone.

## Current Proof and Rebuild Boundary

Current code proves:

- scoped principal account catalog storage
- deterministic exact, wildcard, and any resolution
- peer UID and GID scope resolution
- duplicate tuple rejection
- malformed source-scope rejection

This specification reconstructs the product rule that connection identity is a scoped resolution problem, not a simple username lookup.
