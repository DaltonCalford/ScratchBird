# Decision Record - 10_GC_and_Sweep

## Scope
- Cooperative GC, background GC, and sweep.
- OIT advancement rules.
- GC horizon computations.

## Invariants
- GC horizon uses transaction lock data, not xmin or WAL.
- Sweep must not advance past limbo.

## Assumptions
- Record versions may link same-page or cross-page through stable TID, CTID,
  and back-version surfaces. Sweep and reclaim decisions must remain valid
  across both cases.

## Decisions
- Use Firebird sweep trigger logic.
- Sweep scheduling and throttling are policy-driven and fail closed. The canon
  requires explicit budget, lane, and latency controls, but it does not require
  one fixed scheduler tick or one fixed runtime worker topology.
- Active sweep uses explicit logical worker roles:
  - coordinator/core sweep roles for eligibility and prune
  - downstream evidence/export roles for retained audit, `wal_after_log`, page
    findings, and shadow-capture delivery
- A single runtime subsystem may host multiple logical roles as long as role,
  stage, and failure boundaries remain explicit.
- Mandatory local evidence persistence precedes prune for non-`NORMAL` policy
  lanes.
- Reclaim legality authority is canonical section `10` behavior and must remain
  consistent across sweep and storage-engine cleanup paths.

## Alternatives Considered
- Vacuum/WAL (rejected).

## Closure state
- No unresolved canonical placement questions remain in this file. Remaining
  work is implementation closure against the decisions above.

## References
- docs/specifications_old/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md

## Code-backed audit addendum (2026-03-27)

### Status
`current_authority_with_reconstructed_expansion`

### Main finding
Some decisions remain correct, but several are now too strong or too narrow relative to the reviewed code. The strongest confirmed decision is still that mandatory local evidence precedes prune for non-`NORMAL` lanes. The weakest confirmed decisions are the fixed worker-role split and the implied breadth of archive behavior.

### Implementation code map
- Sweep lane and strict-audit control surfaces: `ScratchBird/include/scratchbird/core/sweep_manager.h:97`, `ScratchBird/include/scratchbird/core/sweep_manager.h:258`, `ScratchBird/include/scratchbird/core/sweep_manager.h:259`, `ScratchBird/include/scratchbird/core/sweep_manager.h:260`
- Policy lane encode or decode and control packing: `ScratchBird/src/core/sweep_manager.cpp:174`, `ScratchBird/src/core/sweep_manager.cpp:194`, `ScratchBird/src/core/sweep_manager.cpp:211`, `ScratchBird/src/core/sweep_manager.cpp:227`
- Mandatory local evidence before prune handoff: `ScratchBird/src/core/sweep_manager.cpp:2765`
- Evidence-blocked failure paths: `ScratchBird/src/core/sweep_manager.cpp:2799`, `ScratchBird/src/core/sweep_manager.cpp:2852`, `ScratchBird/src/core/sweep_manager.cpp:2903`
- Shadow-capture and derivative `wal_after` handoff gates: `ScratchBird/src/core/sweep_manager.cpp:2873`, `ScratchBird/src/core/sweep_manager.cpp:2925`
- Sweep completion publication: `ScratchBird/src/core/sweep_manager.cpp:3020`
- Page-audit downgrade and evidence failure statistics: `ScratchBird/src/core/sweep_manager.cpp:4372`, `ScratchBird/src/core/sweep_manager.cpp:4376`, `ScratchBird/src/core/sweep_manager.cpp:4393`
- Retention metadata stronger than archive-transfer behavior: `ScratchBird/src/core/catalog_manager.cpp:65810`, `ScratchBird/src/core/catalog_manager.cpp:65968`, `ScratchBird/src/core/catalog_manager.cpp:66015`, `ScratchBird/src/core/catalog_manager.cpp:66055`, `ScratchBird/src/core/catalog_manager.cpp:66075`

### Current status
- MGA or OIT-based sweep truth remains the live decision baseline.
- Mandatory local evidence before prune for non-`NORMAL` lanes is confirmed.
- Logical lane boundaries are real.
- Persisted lane-mask, strict-audit, and staged handoff state are real.
- Retention metadata exists, but full archive-transfer behavior is not proven.

### Deferred beyond current canonical scope
- This pass did not prove one fixed scheduler-tick or one fixed per-worker IOPS
  runtime contract.
- This pass did not prove separate runtime worker implementations for
  coordinator, core, page-audit, handoff, and export roles; it proved one
  subsystem with logical lane and stage boundaries.
- This pass did not prove archive-tier behavior matching the strongest
  archive-transfer prose elsewhere in section `10`.

### Suggestions
- Recast worker-role decisions as logical boundaries unless separate worker implementations are introduced.
- Remove or downgrade any scheduler constants not directly tied to reviewed code.
- Split “retention metadata exists” from “archive subsystem exists” as separate decisions.
- Keep the version-link assumption consistent with canonical TID-chain and
  cross-page movement behavior already used by reclaim and migration paths.

### Known contradictions and drift
- The decision record states fixed split worker roles, while the reviewed runtime proves one `SweepManager`-centered subsystem with staged lanes.
- The decision record claims fixed scheduling and throttling specifics that were not proven in this pass.
- The decision record is silent on the current gap between retention metadata and a full archive-transfer subsystem.
- The earlier in-place link assumption was narrower than the actual audited
  engine behavior and has been widened here to match the canon.
