# Section 25 Decision Record

## Current decisions

1. Runtime mode does not alter MGA transaction or durability semantics.
2. Current section-25 authority is local-runtime-first: workers, governance, maintenance, bounded parallelism, and bounded cluster-write-safety primitives.
3. Listener, parser, protocol, and handshake semantics remain primarily owned by sections `26` through `29`.
4. Cluster consensus, log replication, membership, healing, and full distributed scheduler behavior are not current implementation authority here.
5. `WAL_AFTER_*` is derivative debug or export scope only and must never be read as recovery or consensus truth.

## Rejected interpretations

- Treating section `25` as proof of a complete cluster runtime stack.
- Treating bounded governance metadata as proof of end-to-end distributed enforcement.
- Treating listener skew checks as proof of a full cluster clock discipline subsystem.
