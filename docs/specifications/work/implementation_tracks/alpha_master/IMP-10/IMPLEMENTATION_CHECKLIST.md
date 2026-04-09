# IMP-10 Implementation Checklist

## Ticket
- ID: IMP-10
- Section: 10_GC_and_Sweep
- Gate Contract: docs/specifications/10_GC_and_Sweep/TEST_CONTRACT.md

## Inputs
- docs/specifications/10_GC_and_Sweep/SPEC_OUTLINE.md
- docs/specifications/10_GC_and_Sweep/GC_SWEEP_ALGORITHM.md
- docs/specifications/10_GC_and_Sweep/TEST_CONTRACT.md

## Ordered Tasks
1. Implement GC horizon eligibility rules using OIT/OAT/OST and TIP states.
2. Implement sweep trigger thresholds and scheduler checks.
3. Implement cooperative/background GC and sweep workflows.
4. Implement index cleanup and LOB/TOAST orphan cleanup coupling.
5. Implement failure handling and retry-safe sweep semantics.
6. Implement required test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
