# 03 Disk Allocator and Free Space

## Status
- Section status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27
- Primary repo audited: `ScratchBird`

## Current status
- The reviewed implementation has a real allocator, buffer-pool, dirty-writeback, and GC coordination surface.
- `PageManager` is the current allocation and FSM publication authority. It is not transaction visibility or recovery authority.
- `BufferPool` implements a shared frame array with logical policy domains, residency tiers, dirty-state staging, checkpoint-bound flushing, and a background writer.
- `GarbageCollector` implements cooperative and background heap-page cleanup, dirty-page tracking, sweep-blocked wake logic, and MGA frame-hint publication into the buffer pool.
- Canonical `storage.buffer.*` configuration loading is real in `Database`; legacy `memory.*` keys still exist as compatibility aliases.
- The code is materially ahead of the old prose on writeback control and buffer configuration, but materially behind the old prose on extent allocation, NUMA locality, physical residency segmentation, and global workload-classification orchestration.

## Major drift now recorded
- Older prose treated `segmented`, `hot_cold`, `tablespace`, NUMA-local pools, and policy domains as if they were all equally implemented buffer layouts. The reviewed code only proves `single` and logical `segmented`; `hotcold` and `tablespace` layouts are explicitly rejected.
- Older prose described a richer extent allocator than the reviewed code proves. The current allocator truth is page-level FSM bitmap management plus tablespace growth, preallocation, and reconstruction.
- Older prose described more complete ghost-history, prefetch-fairness, and workload-classification behavior than the reviewed code proves. The current code shows substantial config and observability surface, but not a fully closed family of runtime controllers for every advertised policy.
- Older prose implied a more complete disk-full and archive-style writeback incident plane than the reviewed code proves. The current code does prove fail-closed write admission fencing and persisted checkpoint or writeback incident state.

## Section file status
- `README.md`: code-backed section summary, current_authority_with_reconstructed_expansion
- `SPEC_OUTLINE.md`: normalized to current implementation depth, current_authority_with_reconstructed_expansion
- `DECISION_RECORD.md`: normalized to code-backed allocator/buffer decisions, current_authority_with_reconstructed_expansion
- `DEPENDENCIES.md`: normalized to current subsystem dependencies, current_authority_with_reconstructed_expansion
- `ADMISSION_REPLACEMENT_AND_GHOST_HISTORY.md`: narrowed to proven config and observable policy surface, current_authority_with_reconstructed_expansion
- `ALLOCATION_ALGORITHMS.md`: narrowed to page-level FSM allocation and autoextend truth, current_authority_with_reconstructed_expansion
- `BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md`: normalized to dirty-generation, queue-state, and adaptive bgwriter truth, current_authority_with_reconstructed_expansion
- `BUFFER_CONFIGURATION_AND_DEPLOYMENT_PROFILES.md`: normalized to live `storage.buffer.*` loader and profile surface, current_authority_with_reconstructed_expansion
- `BUFFER_GC_COORDINATION_AND_PAGE_MAINTENANCE.md`: normalized to real GC/buffer interaction, current_authority_with_reconstructed_expansion
- `BUFFER_POOL_AND_FLUSH.md`: normalized to shared-frame buffer pool and checkpoint-bound flush truth, current_authority_with_reconstructed_expansion
- `CACHE_AND_BUFFER_COMMERCIAL_GRADE_MGA_ALIGNMENT_MODEL.md`: reconciles older cache research against ScratchBird MGA, forced-write, and shared-frame design rules, current_authority_with_reconstructed_expansion
- `BUFFER_WRITE_SHAPING_AND_FRAGMENTATION_CONTROL.md`: narrowed to proven write shaping and conservative fragmentation controls, current_authority_with_reconstructed_expansion
- `EXTENT_AND_FSM_LAYOUT.md`: narrowed to current FSM and tablespace metric truth, current_authority_with_reconstructed_expansion
- `MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md`: narrowed to logical domains over a shared frame array, current_authority_with_reconstructed_expansion
- `MGA_AWARE_BUFFER_POOL_POLICY.md`: normalized to MGA-aware hints and dirty-state policy, current_authority_with_reconstructed_expansion
- `MGA_BUFFER_LOCALITY_AND_VISIBILITY.md`: normalized to advisory locality and non-authority visibility boundaries, current_authority_with_reconstructed_expansion
- `NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md`: narrowed to partitioned ownership observability, current_authority_with_reconstructed_expansion
- `PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md`: narrowed to real prefetch and thrash config plus control surfaces, current_authority_with_reconstructed_expansion
- `TEST_CONTRACT.md`: normalized to next-wave code-backed gate expectations, current_authority_with_reconstructed_expansion
- `VERSION_PLACEMENT_LOCALITY_AND_FRAGMENTATION_POLICY.md`: narrowed to version-chain hints, not a full placement subsystem, current_authority_with_reconstructed_expansion
- `WORKLOAD_CLASSIFICATION_AND_POLICY_HINTS.md`: normalized to advisory workload-hint ingress, current_authority_with_reconstructed_expansion
- `WRITEBACK_FAILURE_AND_DISK_FULL_POLICY.md`: normalized to fail-closed write-admission fencing and incident persistence, current_authority_with_reconstructed_expansion

## Primary audit lookup anchors
- `src/core/page_manager.cpp` search `PageManager::allocatePageInTablespace`
  for page-level FSM allocation and growth authority.
- `src/core/buffer_pool.cpp` search `BufferPool::publishDirtyGeneration` for
  dirty-generation publication and checkpoint-bound writeback lookup.
- `src/core/garbage_collector.cpp` search `GarbageCollector::cleanPage` for
  GC-driven page cleanup ownership.
- `include/scratchbird/core/database.h` search
  `AUDIT CONTRACT: when write_admission_fenced() is true` for the fail-closed
  write-admission boundary used by writeback and durability paths.

## Section file index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [ADMISSION_REPLACEMENT_AND_GHOST_HISTORY.md](ADMISSION_REPLACEMENT_AND_GHOST_HISTORY.md)
- [ALLOCATION_ALGORITHMS.md](ALLOCATION_ALGORITHMS.md)
- [ALLOCATOR_FREE_SPACE_AND_WRITEBACK_AUTHORITY_MODEL.md](ALLOCATOR_FREE_SPACE_AND_WRITEBACK_AUTHORITY_MODEL.md)
- [BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md](BACKGROUND_WRITER_DIRTY_PAGE_TRACKING_AND_WRITEBACK.md)
- [BUFFER_CONFIGURATION_AND_DEPLOYMENT_PROFILES.md](BUFFER_CONFIGURATION_AND_DEPLOYMENT_PROFILES.md)
- [BUFFER_GC_COORDINATION_AND_PAGE_MAINTENANCE.md](BUFFER_GC_COORDINATION_AND_PAGE_MAINTENANCE.md)
- [BUFFER_POOL_AND_FLUSH.md](BUFFER_POOL_AND_FLUSH.md)
- [BUFFER_WRITE_SHAPING_AND_FRAGMENTATION_CONTROL.md](BUFFER_WRITE_SHAPING_AND_FRAGMENTATION_CONTROL.md)
- [CACHE_AND_BUFFER_COMMERCIAL_GRADE_MGA_ALIGNMENT_MODEL.md](CACHE_AND_BUFFER_COMMERCIAL_GRADE_MGA_ALIGNMENT_MODEL.md)
- [CACHE_BUFFER_DEEP_RESEARCH_RECONCILIATION_AND_COMMERCIAL_GRADE_EVOLUTION_MODEL.md](CACHE_BUFFER_DEEP_RESEARCH_RECONCILIATION_AND_COMMERCIAL_GRADE_EVOLUTION_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [EXTENT_AND_FSM_LAYOUT.md](EXTENT_AND_FSM_LAYOUT.md)
- [MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md](MEMORY_POLICY_DOMAINS_AND_RESIDENCY_SEGMENTS.md)
- [MGA_AWARE_BUFFER_POOL_POLICY.md](MGA_AWARE_BUFFER_POOL_POLICY.md)
- [MGA_BUFFER_LOCALITY_AND_VISIBILITY.md](MGA_BUFFER_LOCALITY_AND_VISIBILITY.md)
- [NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md](NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md)
- [PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md](PREFETCH_FAIRNESS_AND_THRASH_CONTROL.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [VERSION_PLACEMENT_LOCALITY_AND_FRAGMENTATION_POLICY.md](VERSION_PLACEMENT_LOCALITY_AND_FRAGMENTATION_POLICY.md)
- [WORKLOAD_CLASSIFICATION_AND_POLICY_HINTS.md](WORKLOAD_CLASSIFICATION_AND_POLICY_HINTS.md)
- [WRITEBACK_FAILURE_AND_DISK_FULL_POLICY.md](WRITEBACK_FAILURE_AND_DISK_FULL_POLICY.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Deferred beyond current canonical scope
- A proven extent allocator or extent-class placement model is not required for
  this work-plan; page-level FSM allocation is the canonical allocator contract
  until the spec is explicitly promoted.
- Physical segmented, hot-cold, tablespace-local, or NUMA-local frame pools are
  roadmap-only unless a later canonical update makes them required.
- A fully closed replacement or ghost-history controller is not assumed here;
  the current requirement is the documented config, residency-tier, and
  observability surface.
- A fully closed prefetch fairness controller is not assumed here; the current
  requirement is the documented hint and control surface.

## Suggestions
- Keep section `03` grounded in the current implementation split: `PageManager` for allocation/FSM, `BufferPool` for cache and writeback policy, `GarbageCollector` for page cleanup, and `Database` for canonical config loading and write-admission fencing.
- Treat `extent`, `NUMA`, `hotcold`, and advanced fairness language as roadmap-only until the code proves them.
- Reconcile future cache and buffer findings into ScratchBird MGA and forced-write terms before promoting them into canon.
- Convert the remaining high-drift areas into implementation work items instead of preserving aspirational prose.
