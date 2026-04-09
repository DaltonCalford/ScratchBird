# IMP-09 Implementation Checklist

## Ticket
- ID: IMP-09
- Section: 09_Lock_Manager_Core
- Gate Contract: docs/specifications/09_Lock_Manager_Core/TEST_CONTRACT.md

## Inputs
- docs/specifications/09_Lock_Manager_Core/SPEC_OUTLINE.md
- docs/specifications/09_Lock_Manager_Core/LOCK_MANAGER_NORMATIVE_IMPLEMENTATION.md
- docs/specifications/09_Lock_Manager_Core/TEST_CONTRACT.md

## Ordered Tasks
1. Implement canonical lock modes/resource types and compatibility matrix.
2. Implement acquisition, queueing, fairness, release, and conversion rules.
3. Implement deterministic deadlock detection and victim selection.
4. Implement timeout/cancellation semantics and escalation policy.
5. Implement DDL/DML intent mapping and error outcomes.
6. Implement required test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
