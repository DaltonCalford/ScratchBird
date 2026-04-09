# Governance Decision Record

Status: current_authority

## Current decisions

1. canonical execution authority lives in scratchbird_core plus scratchbird_sblr
2. SQL parser surfaces are front-door compilers and must lower to SBLR
3. durable internal identity is UUIDv7-backed through shared ID aliases
4. transaction visibility and recovery governance are MGA-derived and not WAL-derived
5. dialect lowering must fail closed rather than silently bypassing SBLR
6. catalog metadata persists lowering provenance and native artifact information
7. historical governance prose is evidence only unless it matches current canonical and code-backed authority

## Rejected alternatives

- direct SQL execution by parser front doors
- replacing MGA and TIP and OIT and OAT and OST semantics with WAL-redo semantics
- introducing subsystem-local durable ID types as competing primary identity
- treating historical promotion notes as current authority by default

## Change surface

- build graph and runtime library boundaries
- UUID identity types and aliases
- transaction-manager horizon and publication logic
- dialect compiler lowering surfaces
- catalog artifact metadata fields

## Non-guarantees

- no single generated governance registry is claimed here
- no claim is made that every historical governance artifact is current proof
- no claim is made that every gate and test mapping is already exhaustive
