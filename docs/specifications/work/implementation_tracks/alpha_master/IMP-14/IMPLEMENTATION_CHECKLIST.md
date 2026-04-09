# IMP-14 Implementation Checklist

## Ticket
- ID: IMP-14
- Section: 14_Base_Scalar_Types
- Gate Contract: docs/specifications/14_Base_Scalar_Types/TEST_CONTRACT.md

## Inputs
- docs/specifications/14_Base_Scalar_Types/SPEC_OUTLINE.md
- docs/specifications/14_Base_Scalar_Types/SCALAR_STORAGE_FORMAT.md
- docs/specifications/14_Base_Scalar_Types/EMULATED_SCALAR_TYPE_MATRIX.md
- docs/specifications/14_Base_Scalar_Types/TYPE_IO_AND_ERROR_SEMANTICS.md
- docs/specifications/14_Base_Scalar_Types/EMBEDDED_FIELD_ACCESSORS.md
- docs/specifications/14_Base_Scalar_Types/TEST_CONTRACT.md

## Ordered Tasks
1. Implement canonical scalar type encodings and inline/varlen/TOAST rules.
2. Implement round-trip persistence guarantees for every scalar family.
3. Implement lossless emulated-type mapping to canonical types or domains.
4. Implement wire-format conversion contracts with no resolution loss.
5. Implement edge-case boundary and fuzz validation semantics for scalar parsing/formatting.
6. Implement deterministic error-code outcomes for invalid scalar I/O.
7. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
