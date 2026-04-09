# SBLR Expression, Operator, and Type Mapping

## Current authority
- `schemaForOpcode(...)` in `v3_payloads.cpp`
- `V3Emitter` and `AstSblrLowerer` expression emission methods
- executor expression evaluation for current `SBLR3_EXPR_*`, `SBLR3_LITERAL_*`, and column-reference opcodes

## Proven current expression surface
The current code and tests prove a real v3 expression transport around:
- `SBLR3_LITERAL_*`
- `SBLR3_COLUMN_REF`
- unary expression schemas
- binary expression schemas
- `SBLR3_EXPR_CAST`
- `SBLR3_CASE` or case-like expression schemas
- `SBLR3_IN` or `SBLR3_NOT_IN`
- `SBLR3_LIKE` families
- `SBLR3_EXISTS`
- `SBLR3_SUBQUERY`
- function-call and aggregate-call schemas

## Current operator reality
1. parser or lowerer selects concrete `SBLR3_EXPR_*` transport opcodes,
2. payload schema mapping for unary or binary families is deterministic,
3. executor implements arithmetic, comparison, logical, and bitwise evaluation for a substantial current subset.

## Fail-closed boundaries
- The current prose `TYPE_REF` formal model is broader than the audited transport and decoder proof in this section.
- Explicit `cast_rule_id`, `signature_id`, and multi-step coercion registries are not yet proven here as a universal encoded payload contract.
- Section `13` still owns cast and coercion legality; section `22` only documents the current transport or execution bridge that is actually present.
