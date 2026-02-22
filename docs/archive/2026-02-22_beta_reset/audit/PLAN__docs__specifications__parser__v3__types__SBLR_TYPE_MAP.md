# Implementation Plan: SBLR_TYPE_MAP.md

**Spec Path:** `docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md`

**Category:** types

## Scope Summary
- Implement authoritative transaction or datatype specifications.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`

## Implementation Steps (Detailed)
- Reconcile type list with authoritative datatype registry
- Define mapping for type modifiers into TYPE_SPEC
- Define reserved opcode ranges and extension policy
- Add conformance tests for mapping stability

## Manual Gap Analysis (Missing/Unclear Details)
- No reserved opcode range policy
- No modifier mapping to TYPE_SPEC
- No validation tests

## Verification
- Conformance and regression tests.
