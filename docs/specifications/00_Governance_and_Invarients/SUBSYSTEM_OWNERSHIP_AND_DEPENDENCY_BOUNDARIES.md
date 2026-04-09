# Subsystem Ownership and Dependency Boundaries

Status: current_authority

## Current ownership boundaries

| Boundary ID | Owner | Responsibility | Non-owner boundary |
| --- | --- | --- | --- |
| GOV-OWN-001 | scratchbird_core | runtime engine execution, storage, transaction, catalog, and core semantics | parser front doors must not bypass this runtime authority |
| GOV-OWN-002 | scratchbird_sblr | canonical executable IR contract | SQL parser surfaces are producers, not execution authority |
| GOV-OWN-003 | shared core type layer | durable UUIDv7-backed ID alias | subsystem-local ad hoc ID types must not replace canonical identity |
| GOV-OWN-004 | transaction manager | MGA and TIP visibility and horizon authority | parser and front-door layers do not own recovery semantics |
| GOV-OWN-005 | dialect compiler layer | dialect parsing and lowering to SBLR | engine execution semantics must not fragment into per-dialect runtime paths |
| GOV-OWN-006 | catalog manager metadata layer | persisted source dialect, SBLR bytecode, and native artifact provenance | parser code alone is not the durable source of artifact truth |

## Primary boundaries

- runtime execution belongs to engine core, not parser code
- SBLR is the canonical executable IR contract
- durable identity is shared and UUID-backed
- MGA horizon and publication logic belong to transaction management
- persisted lowering provenance belongs to catalog metadata, not transient parser state

## Non-guarantees

- no claim is made that every cross-repo owner boundary is fully mapped here
- no claim is made that driver and tooling boundaries are fully closed here
- no claim is made that the build graph alone is a sufficient long-term ownership registry
