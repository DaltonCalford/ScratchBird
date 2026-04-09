# IMP-04 Implementation Checklist

## Ticket
- ID: IMP-04
- Section: 04_Page_Size_Policy
- Gate Contract: docs/specifications/04_Page_Size_Policy/TEST_CONTRACT.md

## Inputs
- docs/specifications/04_Page_Size_Policy/SPEC_OUTLINE.md
- docs/specifications/04_Page_Size_Policy/DECISION_RECORD.md
- docs/specifications/04_Page_Size_Policy/TEST_CONTRACT.md

## Ordered Tasks
1. Implement supported page-size set and deterministic selection rules.
2. Implement create-time validation and startup mismatch rejection.
3. Implement derived-constant calculations from page size.
4. Implement large-page structural constraints and offset/length rules.
5. Implement compatibility gates for legacy-supported page sizes.
6. Implement deterministic error mapping for invalid values and mismatch.
7. Implement section-required tests and evidence capture.

## Exit Criteria
- Required section tests pass.
- Gate result is pass.
- Traceability maps requirements to artifacts and test IDs.
