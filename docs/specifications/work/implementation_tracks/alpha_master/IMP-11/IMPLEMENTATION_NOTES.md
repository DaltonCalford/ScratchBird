# IMP-11 Implementation Notes

- Ticket: IMP-11

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-11 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- LOB read/write and streaming contract matrix.
- LOB MGA visibility and GC cleanup matrix.
- Online/offline relocation matrices, including watermark catch-up.
- TOAST pointer integrity and pointer-swap rollback matrices.
- Negative, performance, and compatibility matrices with deterministic outcomes.

## Determinism Controls
- Pointer envelope fields and relocation state transitions are explicit.
- Failure handling and rollback outcomes are fixed with deterministic error codes.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
