# ScratchBird Optimizer Plan Surface And Donor Render Analysis 2026-04-02

This package analyzes the current ScratchBird optimizer plan contract and the
client-visible plan or explain surfaces used by the donor engines targeted for
emulation.

## Files

- `OPTIMIZER_PLAN_SURFACE_AND_DONOR_RENDER_ANALYSIS.md`
- `SCRATCHBIRD_RUNTIME_PLAN_SURFACE_MATRIX.csv`
- `CURRENT_SCRATCHBIRD_PLAN_CONSUMER_STATE.csv`
- `DONOR_PLAN_SURFACE_CLASSIFICATION.csv`

## Scope

- local-source-only analysis
- ScratchBird optimizer, SBLR, executor, and current adapter code
- donor explain or plan-output source already present in the local reference tree
- no speculative claims for engines whose current local source sample does not
  prove a stable client-visible explain contract

## Main Result

ScratchBird already has a rich canonical plan contract in
`optimizer::RuntimePlan`. Future parser work should convert donor `EXPLAIN`,
`PROFILE`, `SHOW PLAN`, or equivalent requests into render-profile requests over
that structured plan, and should avoid parsing `plan_text` except where a donor
surface is explicitly text-only.
