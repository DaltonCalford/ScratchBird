# Type I/O and Error Semantics

## Purpose

This file defines the current runtime conversion, parse/format, and named failure semantics for scalar and scalar-adjacent values.

## Primary Runtime Contract

`TypedValue` is the authoritative runtime carrier for this section.

Runtime conversion authority is centered on `TypedValue::convertTo(...)`.

String formatting authority is centered on `TypedValue::toString()` and its type-local helpers.

## Current Named Failure Vocabulary

`NUMERIC_VALUE_OUT_OF_RANGE`

`INVALID_TEXT_REPRESENTATION`

`DATETIME_VALUE_OUT_OF_RANGE`

These names are the canonical runtime failure vocabulary surfaced directly by the currently audited conversion code paths.

## Current Conversion Rules

`TypeSystem` currently authorizes scalar text parsing between string types and numeric, boolean, temporal, and UUID targets.

`TypeSystem` currently authorizes string formatting between numeric, boolean, or temporal sources and string targets.

Money conversion behavior is current runtime truth and is defined by `TypedValue`, including its presently permissive behavior relative to stricter historical prose.

NaN-handling code paths are part of the current runtime.

## Current Boundary

This file authorizes the currently shipped runtime conversion behavior.

This file does not widen section `14` into a universal dialect-alias matrix for every historical input spelling described elsewhere.

This file does not define a universal SQLSTATE contract for every conversion failure.
