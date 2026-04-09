# Admission, Replacement, and Ghost History

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed code proves replacement and admission knobs, residency tiers, and
policy-domain accounting surfaces. For current canonical authority, ghost
history and second-touch behavior are configuration and observability surfaces,
not a promise of a fully closed domain-aware victim-selection controller.

## Current implementation
- Protected and ghost-history percentages are real config fields.
- Second-touch admission and direct-protect-roots knobs are real config fields.
- Residency tiers `RingOnly`, `Probationary`, `Protected`, and `PinBiased` are real observable state labels.
- Workload intent can enter buffer policy through the pin API.
- The audit contract in `BufferPool` explicitly says several later-ticket behaviors are not yet complete.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `ResidencyTier` | `141` | Observable residency-tier vocabulary |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `MgaFrameSnapshot` | `201` | Frame snapshot carries residency tier and policy-domain state |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | Replacement and admission config fields | `255`, `256`, `257`, `258` | Protected percentage, ghost-history percentage, second-touch generations, and direct-protect-roots |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `pinPageGlobal` audit contract | `470` | Workload intent enters policy here |
| ScratchBird | `src/core/database.cpp` | Load `protected_pct`, `ghost_history_pct`, `admission_second_touch_generations`, `admission_direct_protect_roots` | `1731`, `1745`, `1759`, `1775` | Canonical runtime config loading for replacement and admission knobs |

## Drift and contradictions
- The old prose read like ghost history and admission were fully mature replacement controllers.
- The current code proves meaningful config and observability surface, but not full end-to-end policy closure for every named behavior.

## Deferred beyond current canonical scope
- A code-backed victim-selection authority that proves how ghost history,
  protection, and second-touch interact at eviction time
- An operator-visible replacement-state and ghost-history evidence surface
- Direct tests or gates for the named replacement behaviors

## Suggestions
- Keep this file explicit about the distinction between observable policy state and fully proven eviction behavior.
- Do not infer stronger eviction guarantees from ghost-history or second-touch
  names unless the canon is explicitly promoted.
