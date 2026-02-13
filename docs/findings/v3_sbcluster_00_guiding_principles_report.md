# SBCLUSTER-00 Guiding Principles - V3 Findings

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/Cluster Specification Work/SBCLUSTER-00-GUIDING-PRINCIPLES.md`

Status: **Non-authoritative reference** (not listed in `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`). No code verification performed.

## Notes
- The file header states “Non-Authoritative Reference” while the footer claims “Document Status: Authoritative (V3)”. The authoritative inventory does not list this file, so per process it is treated as non-authoritative.
- Principles are high-level and map to other SBCLUSTER specs; implementation verification should be done against those authoritative specs if/when listed.

## Principles (Not Verified)
[ ] Engine authority.
[ ] Shard-local MVCC.
[ ] No cross-shard transactions.
[ ] Push compute to data.
[ ] Identical security configuration across nodes.
[ ] One-way upgrades (monotonic CCE).
[ ] Trust boundary enforcement (no key restore).
[ ] Immutable audit chain.
[ ] Consensus over configuration (Raft).
[ ] Observability by design.

