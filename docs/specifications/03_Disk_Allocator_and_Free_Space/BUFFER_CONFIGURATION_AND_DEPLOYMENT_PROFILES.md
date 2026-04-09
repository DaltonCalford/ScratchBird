# Buffer Configuration and Deployment Profiles

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The current implementation has a real canonical buffer configuration loader, several built-in buffer profiles, logical domain budgets, writeback thresholds, replacement knobs, and prefetch or thrash-control knobs. It does not prove a broader catalog-backed or cluster-propagated deployment-profile system.

## Current implementation
- Canonical keys load from `storage.buffer.*`.
- Legacy `memory.*` keys remain as compatibility aliases.
- Buffer profiles `Dev`, `Oltp`, `Mixed`, `Analytics`, and `MaintenanceRecovery` are real.
- Only `single` and logical `segmented` layouts are accepted.
- `hotcold` and `tablespace` layouts are rejected as not implemented.
- Domain-budget overrides are validated for the current logical domains.
- Replacement, ghost-history, admission, prefetch, and thrash knobs are loaded and validated.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `BufferProfile`, `PoolLayout`, `PolicyDomain` | `67`, `76`, `84` | Profile, layout, and domain vocabularies |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `DomainBudgetConfig`, `Config` | `237`, `248` | Canonical buffer-policy configuration structure |
| ScratchBird | `include/scratchbird/core/buffer_pool.h` | `recomputeDomainFrames`, `applyProfileDefaults` | `304`, `314` | Domain-budget frame computation and profile defaults |
| ScratchBird | `src/core/database.cpp` | `loadBufferPoolConfig` | `1405` | Canonical loader entry point |
| ScratchBird | `src/core/database.cpp` | Config-loader audit contract | `1420` | Declares canonical `storage.buffer.*` authority and segmented-layout meaning |
| ScratchBird | `src/core/database.cpp` | Profile parsing and application | `1427`, `1441` | Loads and applies profile defaults |
| ScratchBird | `src/core/database.cpp` | Layout selection and parse | `1517`, `1523` | Canonical layout loading |
| ScratchBird | `src/core/database.cpp` | Reject unsupported layouts | `1531` | `hotcold` and `tablespace` layouts are fail-closed as not implemented |
| ScratchBird | `src/core/database.cpp` | Writeback config loading | `1541`, `1556`, `1576`, `1600`, `1624` | Enablement, batch size, and dirty thresholds |
| ScratchBird | `src/core/database.cpp` | Domain override loader | `1650` | Validates canonical domain budget overrides |
| ScratchBird | `src/core/database.cpp` | Replacement and admission knobs | `1731`, `1745`, `1759`, `1775` | Protected, ghost-history, second-touch, and direct-protect config |
| ScratchBird | `src/core/database.cpp` | Prefetch and thrash controls | `1783`, `1796`, `1884`, `1905`, `1920`, `1928` | Prefetch enablement, windows, usefulness, pressure, and worker validation |

## Drift and contradictions
- The old prose overstated deployment profile maturity and under-described the canonical config loader now present in `Database`.
- The code proves many knobs, but not all of them are backed by fully mature runtime controllers.

## Non-blocking expansion candidates
- A catalog-backed or cluster-propagated configuration authority if that remains intended
- Pending-restart or compatibility visibility for rejected layouts and partially implemented knobs
- A machine-readable config-to-runtime-consumer matrix for section `03`

## Suggestions
- Keep `storage.buffer.*` as canonical and demote compatibility aliases in prose.
- Explicitly mark which knobs are fully enforced versus advisory or partially consumed.
