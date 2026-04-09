# IMP-16 Implementation Checklist

## Ticket
- ID: IMP-16
- Section: 16_Context_Variables
- Gate Contract: docs/specifications/16_Context_Variables/TEST_CONTRACT.md

## Inputs
- docs/specifications/16_Context_Variables/SPEC_OUTLINE.md
- docs/specifications/16_Context_Variables/CONTEXT_VARIABLES_NORMATIVE_IMPLEMENTATION.md
- docs/specifications/16_Context_Variables/TEST_CONTRACT.md

## Ordered Tasks
1. Implement canonical context namespace and variable registry contracts.
2. Implement variable resolution algorithm and scope-mask validation.
3. Implement trigger row-context frame semantics (`NEW`/`OLD`, operation flags).
4. Implement mutable variable assignment rules and validation checks.
5. Implement per-parser alias/hide mappings and unknown-variable behavior.
6. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
