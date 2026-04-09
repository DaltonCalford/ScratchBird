# Section 03 Decision Record

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Decisions now grounded in code
- `PageManager` is the current durable allocation and FSM publication authority.
- Page-level FSM allocation is the canonical section `03` allocator model for
  this Beta 1 lane; a richer extent-class allocator is deferred until the canon
  is explicitly promoted.
- `BufferPool` policy is not recovery truth. MGA visibility, checkpoint truth, and commit durability remain outside buffer replacement policy.
- `PoolLayout::Segmented` currently means logical policy domains and budget accounting over a shared frame array.
- `PoolLayout::HotCold` and `PoolLayout::Tablespace` are not currently implemented runtime layouts.
- Physical NUMA-local frame ownership is not a current Beta 1 requirement for
  this section; existing NUMA language remains roadmap-only unless separately
  promoted.
- Canonical buffer config keys load from `storage.buffer.*`; `memory.*` keys remain compatibility aliases.
- Writeback failure must fence write admission fail-closed rather than pretending durability succeeded.
- GC cleanup is heap-page focused and reclaim-horizon gated; it is not a generic all-page maintenance service.
- Ghost-history, workload classification, and prefetch fairness are canonical
  only as documented config, hint, and observability surfaces until a stronger
  runtime controller contract is separately promoted.

## Implementation code map
- `ScratchBird/include/scratchbird/core/page_manager.h:32,42,101,113,290,297,390`
- `ScratchBird/src/core/page_manager.cpp:957,1321,1598,1916,2188,2308`
- `ScratchBird/include/scratchbird/core/buffer_pool.h:42,44,125,160,248,270`
- `ScratchBird/src/core/buffer_pool.cpp:2624,2660,4894,4968,5039,5285`
- `ScratchBird/src/core/database.cpp:1405,1420,1517,1531,1541,1731,1745,1759,1775,1783,1884,1905,1920,1928,2780,5680`
- `ScratchBird/include/scratchbird/core/database.h:522,524,525`
- `ScratchBird/include/scratchbird/core/garbage_collector.h:32,117,140,160,165`
- `ScratchBird/src/core/garbage_collector.cpp:362,447,461,486,569,757,767,860,973,983`

## Drift and contradictions
- The old decision record implied a more complete extent and segmentation story than the code proves.
- The code now proves more detailed writeback staging and failure fencing than the old decision record captured.
- Several policy names exist in code primarily as config, enums, and observability surfaces before they exist as fully closed runtime controllers.

## Deferred beyond current canonical scope
- A single decision register that classifies every advertised layout or policy
  as implemented, rejected, compatibility-only, or roadmap-only
- A section-wide proof table linking each major allocator or buffer decision to
  specific tests or gates

## Suggestions
- Keep this file as the first place where roadmap-only features are explicitly downgraded when code proof is missing.
- Require every new section `03` feature to name its owner subsystem and operator-visible failure mode before it is promoted into canonical prose.
