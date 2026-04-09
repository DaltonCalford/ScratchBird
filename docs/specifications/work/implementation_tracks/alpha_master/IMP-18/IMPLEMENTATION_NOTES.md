# IMP-18 Implementation Notes

- Ticket: IMP-18

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-18 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- Core index family correctness matrix.
- Text/spatial and analytics index matrices.
- Vector family and specialized index matrices.
- Engine-specific behavior matrix.
- MGA/security, metrics/costing, maintenance, and health-scan matrices.
- DDL features and fulltext ranking/ts_config matrices.
- Negative, performance, and compatibility matrices.

## Determinism Controls
- All index behaviors are tied to explicit case IDs and expected outcomes.
- Security and MGA ordering constraints are explicit.
- Online rebalance/relocate contracts include deterministic concurrency and atomic swap rules.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
