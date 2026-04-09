# IMP-08 Implementation Checklist

## Ticket
- ID: IMP-08
- Section: 08_Transaction_Core
- Gate Contract: docs/specifications/08_Transaction_Core/TEST_CONTRACT.md

## Inputs
- docs/specifications/08_Transaction_Core/SPEC_OUTLINE.md
- docs/specifications/08_Transaction_Core/TRANSACTION_LIFECYCLE.md
- docs/specifications/08_Transaction_Core/RECORD_VISIBILITY_RULES.md
- docs/specifications/08_Transaction_Core/TRANSACTION_MAP_LAYOUT.md
- docs/specifications/08_Transaction_Core/STARTUP_RECOVERY.md
- docs/specifications/08_Transaction_Core/TRANSACTION_CONTEXT_MAPPING.md
- docs/specifications/08_Transaction_Core/TEST_CONTRACT.md

## Ordered Tasks
1. Implement TIP state model and transaction lifecycle transitions.
2. Implement snapshot and record-version visibility evaluation.
3. Implement isolation/read-committed mode behavior and restart semantics.
4. Implement commit/rollback durability ordering for no-WAL MGA.
5. Implement limbo/2PC recovery behavior at startup.
6. Implement transaction/session/connection attribution mapping contracts.
7. Implement required test suites and capture gate evidence.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
