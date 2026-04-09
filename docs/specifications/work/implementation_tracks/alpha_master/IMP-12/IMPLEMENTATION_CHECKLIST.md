# IMP-12 Implementation Checklist

## Ticket
- ID: IMP-12
- Section: 12_Temporary_Tables
- Gate Contract: docs/specifications/12_Temporary_Tables/TEST_CONTRACT.md

## Inputs
- docs/specifications/12_Temporary_Tables/SPEC_OUTLINE.md
- docs/specifications/12_Temporary_Tables/TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md
- docs/specifications/12_Temporary_Tables/TEST_CONTRACT.md

## Ordered Tasks
1. Implement `TEMP_SESSION` and `TEMP_GLOBAL` lifecycle contracts.
2. Implement session namespace isolation and name-shadowing behavior.
3. Implement `ON COMMIT DELETE ROWS|PRESERVE ROWS|DROP` semantics.
4. Implement restart/crash temp storage discard and root reinitialization behavior.
5. Implement temp locking and scope violation behavior.
6. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
