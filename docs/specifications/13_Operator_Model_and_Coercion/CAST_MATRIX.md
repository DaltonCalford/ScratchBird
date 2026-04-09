# Cast Matrix

## Canonical explicit cast authority

The canonical explicit cast path is:
1. parser lowers an explicit cast request
2. runtime constructs a concrete `TypeInfo` target
3. runtime calls `TypedValue::convertTo(const TypeInfo&, TypedValue&, CastFormat, ErrorContext*)`
4. result is either a typed value or a fail-closed status plus `ErrorContext`

The canonical current implementation lives in `src/core/typed_value.cpp`.

## Process-wide coercion context

`TypedValue::convertTo(...)` reads the configuration key:
- section: `types`
- key: `coercion_context`
- accepted values: `STRICT`, `PERMISSIVE`
- default: `STRICT`

Current normative meaning:
- `STRICT`: lossy conversions that are explicitly guarded must fail closed
- `PERMISSIVE`: the same guarded conversions may apply rounding or truncation rules when the conversion branch allows it

## Session override surface

The executor exposes a session-scoped setting:
- name: `operator.strict_mode`
- accepted values: `ON`, `OFF`, `TRUE`, `FALSE`, `1`, `0`
- `SET LOCAL operator.strict_mode` is rejected
- setting requires connection context
- clearing or resetting the setting removes the session override
- the setting is visible in the session settings row set as `operator.strict_mode`

## Proven strict refusals in current code

The current cast authority explicitly proves at least these fail-closed strict cases:
- `TIMESTAMP_NS -> TIMESTAMP` with non-zero sub-microsecond remainder: reject with lossy strict error
- `DECIMAL256 -> integer` with non-zero fractional remainder: reject with lossy strict error
- `INT256 -> UINT256`: reject by coercion matrix even outside the lossy-rounding cases

These refusals are authoritative because they are directly embedded in `TypedValue::convertTo(...)`.

## Write-path column coercion

The canonical non-array write-path algorithm is:
1. build `TypeInfo` from catalog column metadata
2. preserve catalog precision, scale, timezone flags, and timezone hint
3. call `value.convertTo(target, coerced_out, CastFormat::DEFAULT, ctx)`
4. if conversion fails, attach `column_name` and the violating value to the error context
5. fail the write path

This behavior is authoritative for `INSERT`, `UPDATE`, conflict-update paths, trigger assignments, and related executor write surfaces that route through `coerceValueForColumn(...)`.

## Array write coercion

If `column.is_array` is true, write-path coercion does not use the scalar branch. It routes through `coerceArrayValueForColumn(...)`.

Current proven behavior:
- array input may be accepted as array-valued input or JSON array text that successfully parses
- each element is coerced independently using `TypedValue::convertTo(...)`
- invalid JSON array text fails closed
- non-array JSON input fails closed

## Non-authoritative claims rejected by this section

This section does not claim:
- one exhaustive global cast table covering every type pair in prose
- one exhaustive global implicit coercion table covering every operator family
- durable user-defined cast objects
- durable user-defined operator objects
