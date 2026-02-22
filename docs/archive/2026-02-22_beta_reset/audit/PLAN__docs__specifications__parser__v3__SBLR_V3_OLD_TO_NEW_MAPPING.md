# Implementation Plan: SBLR_V3_OLD_TO_NEW_MAPPING.md

**Spec Path:** `docs/specifications/parser/v3/SBLR_V3_OLD_TO_NEW_MAPPING.md`

**Category:** sblr

## Scope Summary
- Implement this SBLR registry/spec and align with parsers/executors.

## Dependencies
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SEMANTICS.md`

## Implementation Steps (Detailed)
- Validate mapping completeness against V2 opcode registry and V3 opcode spec
- Resolve duplicates and collisions (same new code listed multiple times)
- Define mapping policy for deprecated or removed opcodes
- Define ordering/grouping rules and namespace normalization
- Generate automated mapping validation tool/test

## Manual Gap Analysis (Missing/Unclear Details)
- Duplicate entries exist (same new code repeated with Opcode/ExtendedOpcode)
- No stated policy for removed/unsupported opcodes
- No validation of completeness or collision detection

## Verification
- Opcode registry conformance tests.
- Serializer/deserializer round‑trip tests.
