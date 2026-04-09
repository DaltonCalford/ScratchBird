# IMP-17 Implementation Checklist

## Ticket
- ID: IMP-17
- Section: 17_Functions_and_Procedures
- Gate Contract: docs/specifications/17_Functions_and_Procedures/TEST_CONTRACT.md

## Inputs
- docs/specifications/17_Functions_and_Procedures/SPEC_OUTLINE.md
- docs/specifications/17_Functions_and_Procedures/BLOB_FILTER_RUNTIME_AND_UDR_CONTRACT.md
- docs/specifications/17_Functions_and_Procedures/NORMATIVE_UDR_REMOTE_ENGINE_CONNECTOR_CHECKLIST.md
- docs/specifications/17_Functions_and_Procedures/NORMATIVE_UDR_SB_CLUSTER_FABRIC_CHECKLIST.md
- docs/specifications/17_Functions_and_Procedures/TEST_CONTRACT.md

## Ordered Tasks
1. Implement function evaluation and procedure lifecycle contracts.
2. Implement native full-surface exposure and emulated parser gating for unsupported functions.
3. Implement BLOB filter UDR lifecycle, chunk runtime ABI, and sandbox denial-path behavior.
4. Implement remote connector UDR ABI/state machine, metadata immutability, passthrough policy/txn/audit, and degraded recovery.
5. Implement cluster fabric UDR ABI/link states, multiplex session isolation, parserless SBLR passthrough, and task lifecycle persistence.
6. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
