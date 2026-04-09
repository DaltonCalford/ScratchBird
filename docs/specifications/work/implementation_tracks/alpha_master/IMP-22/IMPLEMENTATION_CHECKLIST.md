# IMP-22 Implementation Checklist

## Ticket
- ID: IMP-22
- Section: 22_SBLR_Canonical_Model_and_Opcodes
- Gate Contract: docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/TEST_CONTRACT.md

## Inputs
- docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SPEC_OUTLINE.md
- docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_FEATURE_TO_OPCODE_MATRIX.md
- docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_STATEMENT_PAYLOAD_SCHEMAS.md
- docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_VERIFIER_AND_VALIDATION_RULES.md
- docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/TEST_CONTRACT.md

## Ordered Tasks
1. Implement feature-key to opcode completeness and uniqueness checks.
2. Implement container/serialization and section-offset conformance checks.
3. Implement payload schema and normalization-evidence validation checks.
4. Implement expression/coercion/domain payload validation checks.
5. Implement determinism, corruption, and cross-section conformance checks.
6. Implement unresolved-placeholder sweep assertion for canonical section files.

## Exit Criteria
- Required suites A-H pass.
- Gate result is pass.
- No unresolved placeholders in canonical section-22 files except allowed freeze note.
