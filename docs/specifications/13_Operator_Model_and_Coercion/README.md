# Section 13: Operator Model and Coercion

Status: current_authority

## Section scope

This section defines the current shipped ScratchBird behavior for:
- explicit `CAST(expr AS type)` execution
- implicit coercion that is actually performed today by runtime branches
- session and configuration controls that tighten or relax coercion behavior
- write-path coercion into catalog-defined column types
- fail-closed boundaries for unsupported custom cast and operator objects

## Section boundaries

This section does not own:
- scalar and complex type layout details, which belong to sections `14` and `15`
- transaction lifecycle, which belongs to section `08`
- durability or recovery interpretation, which belongs to section `35`
- parser front-end ownership, which belongs to section `28`
- SBLR lowering and execution container ownership, which belongs to sections `22` and `23`
- catalog-backed custom cast or operator objects, which are not currently authoritative runtime features

## Current implementation state

The current implementation is not one centralized universal operator registry. The audited authority is distributed across a small number of current code paths:
- `TypedValue::convertTo(...)` in `src/core/typed_value.cpp` is the canonical explicit cast authority
- `coerceValueForColumn(...)` in `src/sblr/executor.cpp` is the canonical write-path coercion authority for non-array columns
- `coerceArrayValueForColumn(...)` in `src/sblr/executor.cpp` is the canonical write-path coercion authority for array columns
- branch-local numeric and temporal helpers in `src/sblr/executor.cpp` and `src/sblr/expression_evaluator.cpp` provide additional implicit coercion behavior for specific operator and function families
- the session control surface for stricter operator coercion is `operator.strict_mode`
- the process-wide default coercion context is `types.coercion_context`

## Governing invariants

- ScratchBird is always in a transaction.
- `COMMIT` and `ROLLBACK` immediately start the next transaction.
- `SET operator.strict_mode` changes session state, not transaction existence.
- `SET LOCAL operator.strict_mode` is not supported.
- Explicit and implicit coercion behavior is defined in MGA terms only. No WAL or LSN semantics are part of this section.
- No current authoritative runtime support exists for user-defined `CREATE CAST`, `DROP CAST`, `CREATE OPERATOR`, or `DROP OPERATOR` objects.

## Direct audit lookup anchors

- `src/core/typed_value.cpp` search key `TypedValue::convertTo(`
- `src/sblr/executor.cpp` search key `coerceValueForColumn(`
- `src/sblr/executor.cpp` search key `operator.strict_mode`

## File Index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [BETA2_CAST_STRING_AND_SUBFIELD_ACCESS_MODEL.md](BETA2_CAST_STRING_AND_SUBFIELD_ACCESS_MODEL.md)
- [CAST_MATRIX.md](CAST_MATRIX.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [IMPLICIT_COERCION_RULES.md](IMPLICIT_COERCION_RULES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Maintenance
- Update file list with `../skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`.
