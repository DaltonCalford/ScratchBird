# IMP-28 Implementation Checklist

## Ticket
- ID: IMP-28
- Section: 28_Parser_Implementations
- Gate Contract: docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md

## Inputs
- docs/specifications/28_Parser_Implementations/SPEC_OUTLINE.md
- docs/specifications/28_Parser_Implementations/PARSER_IMPLEMENTATION_CANONICAL_SPEC.md
- docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_*.md|csv
- docs/specifications/28_Parser_Implementations/NORMATIVE_*.md
- docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md

## Ordered Tasks
1. Implement architecture boundary, dedicated parser coverage, and capability-gate matrices.
2. Implement translation determinism, error mapping, session/naming, and wire/IPC matrices.
3. Implement conformance/performance and infrastructure SQL surface matrices.
4. Implement normative checklist suites K, L, M, N, O, P, Q, R, S, T matrices.
5. Implement negative/fuzz and fixture requirements matrix.

## Exit Criteria
- Required suites A-T pass.
- Gate result is pass.
- No unresolved reject path lacks deterministic mapping.
