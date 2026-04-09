# Workload Classification and Policy Hints

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed code proves explicit workload classes and a pin API that accepts workload intent. It does not prove a global automatic workload-classification subsystem.

## Current implementation
- `WorkloadClass` is real and includes point lookup, range or sequential scan, nested-loop reread, sweep GC, bulk write, checkpoint cleaner, recovery replay, speculative prefetch, and temp work.
- `pinPageGlobal` accepts workload class as policy intent.
- GC publishes `SweepGc` workload intent when it updates frame hints.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `WorkloadClass` | `95` | Explicit workload-hint vocabulary |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `pinPageGlobal` audit contract | `470` | Workload intent is declared as advisory policy input |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `pinPageGlobal` parameters | `482`, `483`, `484` | Runtime page pin ingress accepts workload class |
| ScratchBird | `src/core/garbage_collector.cpp` | `page_hints.workload_class = BufferPool::WorkloadClass::SweepGc` | `767` | GC publishes explicit workload intent back into buffer policy |

## Drift and contradictions
- The old prose described stronger workload inference and routing than the code proves.
- The current code proves workload hint ingress, not a universal classifier.

## Non-blocking expansion candidates
- A global classifier or a clear decision that the engine will remain explicit-hint driven
- Metrics proving workload hints are consumed consistently across residency, prefetch, and writeback behavior

## Suggestions
- Standardize on `advisory workload intent` language until automatic classification is implemented and audited.
