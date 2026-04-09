# IMP-03 Implementation Checklist

## Ticket
- ID: IMP-03
- Section: 03_Disk_Allocator_and_Free_Space
- Gate Contract: docs/specifications/03_Disk_Allocator_and_Free_Space/TEST_CONTRACT.md

## Inputs
- docs/specifications/03_Disk_Allocator_and_Free_Space/SPEC_OUTLINE.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/ALLOCATION_ALGORITHMS.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/EXTENT_AND_FSM_LAYOUT.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/BUFFER_POOL_AND_FLUSH.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/TEST_CONTRACT.md

## Ordered Tasks
1. Implement extent-size policy as a deterministic function of page size.
2. Implement FSM page/root formats and validation rules.
3. Implement allocation and deallocation algorithms with lock protocol.
4. Implement autoextend and growth-chunk behavior.
5. Implement FSM rebuild workflow and triggers.
6. Implement buffer-pool dirty tracking and flush/commit durability sequence.
7. Implement deterministic failure handling and error codes.
8. Implement required stress/consistency test contracts and evidence.

## Exit Criteria
- Required section tests pass.
- Gate result is pass.
- Traceability maps requirements to artifacts and test IDs.
