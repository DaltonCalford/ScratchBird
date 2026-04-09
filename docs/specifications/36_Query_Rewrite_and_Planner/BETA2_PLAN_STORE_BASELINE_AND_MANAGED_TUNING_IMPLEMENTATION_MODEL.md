# Beta 2 Plan Store Baseline And Managed Tuning Implementation Model

## Purpose

Close the runtime and operator-workflow gap in the existing Beta 2 plan-store
and managed-tuning canon.

## Governing rules

1. Plan-store history, baseline policy, and tuning action must be durable rows.
2. Forced plans are always subject to legality and safety checks.
3. Automatic actions are reviewable and reversible.
4. Query-store style telemetry remains native ScratchBird evidence, not donor
   compatibility.

## Required runtime flows

- plan publication recording
- outcome ingestion
- regression classifier
- baseline recommendation worker
- forced-plan validator
- auto-correction review queue

## Required operator surfaces

- `show plan history`
- `show active baselines`
- `quarantine plan`
- `force baseline`
- `clear force`
- `review automatic action`

## Refusal rules

- `PLAN_BASELINE_ILLEGAL`
- `PLAN_FORCE_REFUSED`
- `PLAN_AUTO_ACTION_REQUIRES_REVIEW`
- `PLAN_HISTORY_RETENTION_EXPIRED`

## Metrics

- regressed plan count
- forced-plan refusal count
- automatic action acceptance rate
- plan history storage growth

## Cross-section requirements

- section `36` owns baseline and tuning closure
- section `31` owns regression gate integration
