# IMP-30 Implementation Notes

- Ticket: IMP-30

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-30 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- Matrices for ABI/lifecycle, connectivity, installer profiles, statement/result APIs, command surfaces, errors, migration, and replication control surfaces.

## Determinism Controls
- ABI version and struct-size checks are explicit.
- Profile-specific accept/reject behavior is explicit.
- Migration/replication CLI-to-SQL mapping contracts are explicit.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
