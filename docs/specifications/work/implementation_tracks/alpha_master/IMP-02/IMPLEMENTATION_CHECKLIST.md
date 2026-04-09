# IMP-02 Implementation Checklist

## Ticket
- ID: IMP-02
- Section: 02_Filespace_Lifecycle
- Gate Contract: docs/specifications/02_Filespace_Lifecycle/TEST_CONTRACT.md

## Inputs
- docs/specifications/02_Filespace_Lifecycle/SPEC_OUTLINE.md
- docs/specifications/02_Filespace_Lifecycle/FILESPACE_FILE_LAYOUT.md
- docs/specifications/02_Filespace_Lifecycle/FILESPACE_OPERATIONS.md
- docs/specifications/02_Filespace_Lifecycle/PARTITION_BOUNDARY_SPLIT_AND_OBJECT_RELOCATION.md
- docs/specifications/02_Filespace_Lifecycle/TEST_CONTRACT.md

## Ordered Tasks
1. Implement filespace header layout and page-0 validation rules.
2. Implement filespace create, attach, detach, autoextend, and state transitions.
3. Implement shadow-file mirroring and read-only failover behavior.
4. Implement online table relocation with copy, catch-up, and atomic swap.
5. Implement offline table relocation flow.
6. Implement online and offline range-partition boundary split flow.
7. Implement migration-history persistence and resumable state machine.
8. Implement lock protocol for filespace operations and relocation/split cutovers.
9. Implement deterministic errors for all rejection/failure paths.
10. Implement required tests for section gate coverage.

## Exit Criteria
- All required section tests pass.
- Gate result is pass.
- Traceability rows map each required behavior to implementation/test artifacts.
