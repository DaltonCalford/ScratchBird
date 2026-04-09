# IMP-13 Implementation Checklist

## Ticket
- ID: IMP-13
- Section: 13_Operator_Model_and_Coercion
- Gate Contract: docs/specifications/13_Operator_Model_and_Coercion/TEST_CONTRACT.md

## Inputs
- docs/specifications/13_Operator_Model_and_Coercion/SPEC_OUTLINE.md
- docs/specifications/13_Operator_Model_and_Coercion/IMPLICIT_COERCION_RULES.md
- docs/specifications/13_Operator_Model_and_Coercion/CAST_MATRIX.md
- docs/specifications/13_Operator_Model_and_Coercion/TEST_CONTRACT.md
- docs/specifications/14_Base_Scalar_Types/TYPE_IO_AND_ERROR_SEMANTICS.md

## Ordered Tasks
1. Implement canonical operator catalog and deterministic operator lookup.
2. Implement precedence and associativity parsing/evaluation rules.
3. Implement implicit and explicit cast correctness using canonical cast matrix.
4. Implement coercion edge-case handling (null, overflow, lossy cast, text-to-type failures).
5. Implement fixed invalid-cast error behavior per cast and scalar I/O semantics.
6. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
