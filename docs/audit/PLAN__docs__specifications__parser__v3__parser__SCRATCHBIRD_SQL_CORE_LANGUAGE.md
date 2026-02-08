# Implementation Plan: SCRATCHBIRD_SQL_CORE_LANGUAGE.md

**Spec Path:** `docs/specifications/parser/v3/parser/SCRATCHBIRD_SQL_CORE_LANGUAGE.md`

**Category:** parser

## Scope Summary
- Implement grammar and language specification alignment.

## Dependencies
- `docs/specifications/parser/v3/PARSER_AMBIGUITY_RESOLUTION.md`
- `docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`

## Implementation Steps (Detailed)
- Define authoritative core SQL grammar and semantics
- Map core language constructs to AST and SBLR
- Define error/SQLSTATE mapping for syntax violations
- Align with dialect separation rules

## Manual Gap Analysis (Missing/Unclear Details)
- No SBLR emission rules
- No error/SQLSTATE mapping
- No explicit dialect separation

## Verification
- Grammar coverage and parsing tests.
