# NUMA Locality and Frame Ownership

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed code proves partition ownership observability and partitioned page-table locking. It does not prove true NUMA-local frame pools, NUMA placement, or NUMA-aware scheduler behavior.

## Current implementation
- Frame snapshots carry `owner_partition` and `home_partition` fields.
- Buffer-pool APIs use partitioned page-table structures for at least some page-table operations.
- The current evidence is about ownership and lock partitioning, not NUMA memory placement.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaFrameSnapshot` | `201` | Snapshot type carrying ownership fields |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `owner_partition`, `home_partition` | `205`, `206` | Observable ownership metadata exists |
| ScratchBird | `src/core/buffer_pool.cpp` | `getPartitionIndex(gpid)` and partition-table lock | `4905`, `4908` | Runtime page-table ownership is partitioned |
| ScratchBird | `src/core/buffer_pool.cpp` | Partition table lookup | `4911` | Page residency lookup is partition-table scoped |

## Drift and contradictions
- The old prose described NUMA-local frame ownership more strongly than the code proves.
- The code currently proves partitioned ownership observability, not NUMA.

## Non-blocking expansion candidates
- NUMA-local allocation or residency enforcement
- NUMA-aware scheduler or worker affinity
- An operator-visible NUMA-locality metric surface

## Competitive parity closure requirements

For the competitive-performance parity package, NUMA and partition-local memory
behavior are release-significant rather than optional on hosts where locality
materially affects benchmark speed.

Before parity closure on such hosts, ScratchBird shall provide:

- worker affinity or explicit local-memory preference for admitted multi-worker
  execution
- locality-aware memory charging so worker scratch is not treated as topology
  free
- an operator-visible locality metric surface showing when cross-partition
  memory traffic or ownership stealing occurred

The parity package may not claim closure on a multi-node NUMA host while a
benchmark-governed multi-worker path remains slower because scheduler or
allocator behavior ignores locality.

## Suggestions
- Keep `NUMA` language explicitly marked as unproven or roadmap-only until code-backed placement exists.
