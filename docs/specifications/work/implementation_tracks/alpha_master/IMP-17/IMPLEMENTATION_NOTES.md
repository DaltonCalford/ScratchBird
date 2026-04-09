# IMP-17 Implementation Notes

- Ticket: IMP-17

## Execution Mode
This workspace is specification-only (`docs/` tree, no implementation `src/` tree). IMP-17 is executed as a deterministic implementation contract gate for low-capability implementers.

## What Was Produced
- Function/procedure core lifecycle matrices.
- BLOB filter lifecycle/runtime/sandbox matrices.
- Remote connector UDR ABI/state, metadata, passthrough, and degraded recovery matrices.
- Cluster fabric ABI/link, multiplex, parserless passthrough, and task lifecycle matrices.
- Negative, performance, and compatibility matrices.

## Determinism Controls
- UDR ABI symbol lists and state transitions are explicit.
- Parserless fabric rule (SBLR payload only) is explicit and enforced.
- Remote metadata snapshots are immutable after completion.

## Runtime Constraint
Actual source-code implementation and executable test run are pending because no code/test harness exists in `local_work`.
