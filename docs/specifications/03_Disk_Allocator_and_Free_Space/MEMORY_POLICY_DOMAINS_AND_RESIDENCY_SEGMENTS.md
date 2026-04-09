# Memory Policy Domains and Residency Segments

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
Logical policy domains, domain budgets, and residency tiers are real. The current implementation does not prove physically separate residency segments or physically segmented frame arrays.

## Current implementation
- `PolicyDomain` is real with current domains for critical system, hot OLTP, read-mostly, scan/bulk ring, version/undo, and temporary work.
- `DomainBudgetConfig` and per-domain budget overrides are real.
- `MgaFrameSnapshot` records domain, residency tier, lifecycle state, and dirty state.
- `PoolLayout::Segmented` is explicitly defined as logical policy domains over a shared frame array.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Segmented-layout audit contract | `44` | Defines segmented layout as logical domains over shared frames |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `PolicyDomain` | `84` | Logical domain vocabulary |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `ResidencyTier` | `141` | Observable residency state labels |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaFrameSnapshot` | `201` | Snapshot exposes domain, residency tier, lifecycle state, and dirty state |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `DomainBudgetConfig`, `Config`, `domainBudget`, `recomputeDomainFrames` | `237`, `248`, `294`, `304` | Domain-budget definition and frame computation |
| ScratchBird | `src/core/database.cpp` | Config-loader segmented-layout audit contract | `1420` | Canonical semantics for segmented layout are stated here |
| ScratchBird | `src/core/database.cpp` | `apply_domain_override` | `1650` | Runtime validation for canonical domain overrides |

## Drift and contradictions
- Older prose overstated physical segmentation.
- The current code proves logical accounting and observability, not distinct memory pools.

## Non-blocking expansion candidates
- A proven physical residency implementation if segmented memory remains part of the roadmap
- An operator-visible budget usage and enforcement report for each domain

## Suggestions
- Use `logical policy domains over shared frames` as the canonical phrase unless the runtime architecture changes.
