# Operator Model and Coercion Spec Outline

## Owned topics

1. Explicit cast execution authority
2. Process-wide coercion context configuration
3. Session-scoped `operator.strict_mode`
4. Write-path coercion into catalog column types
5. Array write coercion and JSON-array text handling
6. Branch-local implicit coercion boundaries in evaluator and executor paths
7. Unsupported custom cast and operator object boundaries

## Canonical implementation authorities

- `src/core/typed_value.cpp`
- `src/sblr/executor.cpp`
- `src/sblr/expression_evaluator.cpp`
- `src/parser/parser_v3.cpp`
- `include/scratchbird/core/expression.h`

## Required guarantees

- Explicit cast requests must resolve through `TypedValue::convertTo(...)` with a concrete `TypeInfo` target.
- Write-path coercion must use catalog column metadata and return column-aware errors on mismatch.
- Session coercion controls must be explicit, inspectable, and fail closed on invalid values.
- Unsupported custom cast and operator DDL must not be described as shipped runtime authority.

## Explicit non-guarantees

- No single global operator or cast matrix is claimed beyond the proved current branches.
- No catalog-backed user-defined cast registry is claimed.
- No catalog-backed user-defined operator registry is claimed.
- No transaction-local `SET LOCAL operator.strict_mode` semantics are claimed.
