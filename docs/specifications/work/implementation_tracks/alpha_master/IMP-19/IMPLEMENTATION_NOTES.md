# IMP-19 Implementation Notes

- Ticket: IMP-19

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-19 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- Authentication and authorization matrices with deterministic outcomes.
- Effective-permission and definer/invoker semantics matrices.
- Row/column/domain masking pipeline matrix.
- Encryption/key-metadata and PKI lifecycle matrices.
- Default policy, negative, performance, and compatibility matrices.

## Determinism Controls
- Access control pipeline order is explicit and testable.
- PKI lifecycle transitions and forbidden states are enumerated.
- SQL surfaces never expose private key material.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
