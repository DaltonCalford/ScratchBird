# Implementation Plan: BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md

**Spec Path:** `docs/specifications/parser/v3/BETA_SQL2023_IMPLEMENTATION_SPECIFICATION.md`

**Category:** sql-standard

## Scope Summary
- Implement SQL:2023 required features for V3 and formalize rejection for deferred features.

## Dependencies
- `docs/specifications/parser/v3/parser/*` (dialect grammars)
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_*`
- `docs/specifications/parser/v3/types/*`
- `docs/specifications/parser/v3/catalog/*`

## Implementation Steps (Detailed)
- Normalize SQL:2023 feature list to V3 REQUIRED vs REJECTED and ensure rejection behavior is specified
- Map every required feature to V3 parser grammar, AST nodes, and SBLR emission rules
- Define executor semantics for JSON enhancements, NULL handling, and numeric literal rules
- Align catalog metadata with SBDB$ domains and system catalog layout (no RDB$ naming)
- Define storage formats using V3 type specs (JSON/JSONB, JSON path, comparison operators)
- Define error codes and SQLSTATEs for unsupported SQL:2023 features
- Add conformance tests for each required feature and rejection tests for deferred features

## Manual Gap Analysis (Missing/Unclear Details)
- References to `src/parser/v2` and v2 files are not aligned with V3 parser separation
- Uses Firebird-style RDB$ catalog table names instead of SBDB$ catalog
- Storage format sections appear incomplete and reference non‑V3 headers
- No explicit SBLR opcode mapping for SQL:2023 features
- Optional/deferred features are mentioned but do not specify rejection opcodes and error codes

## Verification
- SQL:2023 conformance tests for required features.
- Rejection tests for deferred features with `ERR_FEATURE_DISABLED`.
