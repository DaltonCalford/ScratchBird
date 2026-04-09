# Section 03 Specification Outline

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
This section is now organized around four code-backed authorities: allocation and FSM publication in `PageManager`, buffer residency and flush policy in `BufferPool`, cleanup coordination in `GarbageCollector`, and canonical buffer-policy configuration loading in `Database`.

## Code-backed subsection map
- Allocator and FSM authority:
  - `ALLOCATION_ALGORITHMS.md`
  - `EXTENT_AND_FSM_LAYOUT.md`
- Buffer residency, replacement, prefetch, and flush policy:
  - `BUFFER_POOL_AND_FLUSH.md`
  - `ADMISSION_REPLACEMENT_AND_GHOST_HISTORY.md`
  - `MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md`
  - `PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md`
  - `WORKLOAD_CLASSIFICATION_AND_POLICY_HINTS.md`
  - `BUFFER_WRITE_SHAPING_AND_FRAGMENTATION_CONTROL.md`
- GC integration and MGA-aware page-hinting:
  - `BUFFER_GC_COORDINATION_AND_PAGE_MAINTENANCE.md`
  - `MGA_AWARE_BUFFER_POOL_POLICY.md`
  - `MGA_BUFFER_LOCALITY_AND_VISIBILITY.md`
  - `VERSION_PLACEMENT_LOCALITY_AND_FRAGMENTATION_POLICY.md`
- Config and failure policy:
  - `BUFFER_CONFIGURATION_AND_DEPLOYMENT_PROFILES.md`
  - `BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md`
  - `WRITEBACK_FAILURE_AND_DISK_FULL_POLICY.md`
  - `NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md`
- Section governance:
  - `README.md`
  - `DECISION_RECORD.md`
  - `DEPENDENCIES.md`
  - `TEST_CONTRACT.md`

## Current implementation depth
- The allocator is real, but page-granular and FSM-driven rather than extent-driven.
- The buffer pool is real and materially richer than the old prose suggested, especially in dirty-state staging, queue-state observability, profile loading, and background writer behavior.
- GC or buffer coordination is real for heap-page cleanup and MGA hint publication, but not a general page-maintenance orchestrator.
- Many advanced policy labels exist as enums, config keys, or snapshot fields before they exist as fully closed runtime control systems.

## Implementation closure boundary
- `hotcold`, `tablespace`, and physical `NUMA`-specific layouts remain in the
  section only as explicitly non-implemented or roadmap-only vocabulary. They
  are not current Beta 1 implementation requirements.
- Section `03` standardizes on the current page-level FSM allocator model for
  this work-plan. A true extent allocator is deferred until the canon is
  explicitly promoted.
- Ghost-history, workload classification, and prefetch fairness are
  configuration, hint, or observability surfaces unless a specific runtime
  controller contract is made authoritative in the canon.

## Deferred beyond current canonical scope
- A section-owned authority matrix separating implemented behavior, advisory
  hints, rejected layouts, and roadmap-only ideas
- A section-owned operator status surface for writeback incidents, dirty debt,
  and allocation pressure
- A machine-readable test and gate matrix for section `03`

## Suggestions
- Keep future edits fail-closed: if a policy surface is only config or observability, the spec must say so directly.
- Prefer one implementation workpack per drift cluster: allocator/FSM, buffer replacement, prefetch/thrash, and writeback-failure handling.
