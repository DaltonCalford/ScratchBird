# Dependency Graph and Shared Ownership

This file owns the architectural dependency graph and shared-ownership rules for subsystem interaction.

## Top-level dependency graph

1. client/tooling surfaces depend on bounded protocol, listener, and runtime entry surfaces depending on mode
2. parser/front-door surfaces depend on dialect and parser sections, then hand off into shared internal execution forms
3. listener/server orchestration depends on protocol/handshake/session-entry surfaces and the engine runtime boundary
4. gate/certification surfaces depend on lower subsystem truth but do not replace it

## Shared-ownership seams

### Engine <-> listener seam

- section 29 owns listener/session/process local truth
- lower engine sections own engine-local truth
- section 32 owns the seam description and explicit boundary rule

### Parser <-> runtime seam

- sections 21 and 28 own front-door and parser truth
- sections 22 and 23 own shared internal execution model truth
- section 32 owns the architectural fact that these are distinct but connected layers

### Protocol/handshake <-> session/runtime seam

- sections 26 and 27 own transport and handshake contract truth
- section 29 owns listener/session orchestration truth
- section 32 owns the top-level graph showing how those surfaces relate

### Client/tooling <-> engine contract seam

- section 30 owns external client/tool surface truth
- lower runtime sections own engine behavior
- section 32 owns the rule that the client layer must not be mistaken for engine-local ownership

## Dependency admission rules

1. A subsystem may depend inward on a more local execution layer only through an explicitly owned seam.
2. Parser layers may lower into shared execution forms, but may not claim direct ownership of runtime semantics.
3. Client/tooling layers may invoke public or bounded entry surfaces, but may not bypass owned session or protocol boundaries in specification language.
4. Gate and certification sections may constrain release claims, but they may not become the detailed owner of subsystem semantics.
5. Any new cross-subsystem dependency must declare whether it is hard-boundary, shared-boundary, or adjacent-boundary before it is considered architecture-safe.

## Architectural review triggers

A section 32 update is required when any of the following occurs:
- a new entry surface is added
- a subsystem begins depending on a previously non-adjacent subsystem
- an internal message path is promoted to a public or bounded contract
- an existing hard boundary becomes shared
- a new extensibility surface or plugin seam is introduced

## Explicit non-guarantees

- this graph is not a distributed systems topology map
- this graph does not imply stable internal APIs between every adjacent subsystem
- this graph does not override lower-level section authority
- this graph does not certify performance, only architecture and ownership shape
