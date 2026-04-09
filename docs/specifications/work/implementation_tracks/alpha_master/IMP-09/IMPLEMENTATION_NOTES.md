# IMP-09 Implementation Notes

- Ticket: IMP-09

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-09 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- Full lock compatibility matrix for NL/IS/IX/S/SIX/U/X.
- Fairness-aware acquisition/queueing matrix.
- Deterministic deadlock victim selection matrix.
- Conversion and escalation matrix.
- Error semantics matrix with statement/transaction outcomes.

## Determinism Controls
- No lock-mode inference; matrix is explicit and exhaustive.
- Deadlock victim tie-break is fixed and testable.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
