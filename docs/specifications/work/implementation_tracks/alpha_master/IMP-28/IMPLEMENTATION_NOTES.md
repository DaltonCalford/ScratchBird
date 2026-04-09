# IMP-28 Implementation Notes

- Ticket: IMP-28

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-28 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- Suite-group matrices covering A through T plus negative/fuzz and fixture evidence requirements.

## Determinism Controls
- Capability decisions, translation outputs, and egress mappings are explicit.
- Remote connector and cluster-fabric parser surfaces are explicit and policy-gated.
- Emulated parser bootstrap behavior is explicit and non-fallback.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
