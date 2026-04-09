# Prefetch Fairness and Thrash Control

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The code proves prefetch APIs, workload labels, prefetch and thrash-control configuration, and explicit thrash-state vocabulary. This pass did not prove a fully closed fairness scheduler matching the older prose.

## Current implementation
- `prefetchPagesGlobal` exists.
- Prefetch enablement, worker count, scan/index/chain windows, max debt, usefulness floor, and thrash pressure thresholds are all real config surfaces.
- `ThrashDetectorState` is real.
- Workload classes include speculative prefetch and scan-oriented behaviors.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `WorkloadClass` | `95` | Workload labels include speculative prefetch and scan classes |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `ThrashDetectorState` | `179` | Thrash-state vocabulary |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Prefetch and thrash config fields | `259`, `260`, `261`, `262`, `263`, `264`, `265`, `266`, `267`, `268` | Prefetch workers/windows, debt, usefulness floor, and thrash budgets |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `prefetchPagesGlobal` | `558` | Prefetch API surface |
| ScratchBird | `src/core/buffer_pool.cpp` | `BufferPool::prefetchPagesGlobal` | `3517` | Runtime prefetch entry point |
| ScratchBird | `src/core/database.cpp` | Prefetch enablement and key loading | `1783`, `1796` | Canonical prefetch config loading |
| ScratchBird | `src/core/database.cpp` | Prefetch usefulness floor and thrash pressure loading | `1884`, `1905` | Core prefetch control thresholds |
| ScratchBird | `src/core/database.cpp` | Prefetch worker and max-debt validation | `1920`, `1928` | Validation for worker count and speculative debt budget |

## Drift and contradictions
- The old prose implied stronger fairness and anti-thrash orchestration than this audit proved.
- The current code proves substantial config and vocabulary surface, but not a fully closed fairness governor.

## Non-blocking expansion candidates
- A proven per-session or per-object fairness controller
- Operator-visible debt, usefulness, and thrash-state reporting
- Direct tests or gates proving prefetch suppression and fairness behavior under pressure

## Suggestions
- Keep this file explicit about which parts are proven control surfaces versus not-yet-proven runtime enforcement.
