# Implicit Coercion Rules

## Current authority model

ScratchBird implicit coercion is real but distributed. The current authoritative decision order is:

```text
if expression contains explicit CAST:
    use TypedValue::convertTo via the explicit cast path
else if value is being written into a catalog-defined column:
    use coerceValueForColumn or coerceArrayValueForColumn
else if runtime is binding function parameters to declared parameter types:
    use TypedValue::convertTo against the parameter TypeInfo
else if executor or expression evaluator uses a branch-local numeric or temporal helper:
    use that branch-local helper only for that operator or function family
else:
    do not claim implicit coercion support without direct code proof
```

## Branch-local implicit coercion currently proved

Current audited code proves these branch-local coercion classes:
- numeric function and operator families in `src/sblr/expression_evaluator.cpp` that coerce inputs through local double-conversion helpers
- executor helpers for temporal comparison and text-to-target implicit conversion in `src/sblr/executor.cpp`
- function parameter binding in executor paths that coerce argument values to declared parameter types through `TypedValue::convertTo(...)`
- write-path coercion for column targets through `coerceValueForColumn(...)`

## `operator.strict_mode`

`operator.strict_mode` is a current runtime control surface with these rules:
- parser accepts shorthand assignment syntax such as `SET operator.strict_mode ON`
- executor rejects `SET LOCAL operator.strict_mode`
- executor requires a live connection context
- accepted values are `ON`, `OFF`, `TRUE`, `FALSE`, `1`, `0`
- reset clears the session variable instead of setting a third explicit mode
- the current value is visible in the session settings row set as `operator.strict_mode`

Normative interpretation:
- this setting controls session-level strictness for operator/coercion fallback surfaces that consult session state
- it is not transaction-local
- it does not create or destroy transaction context

## Relationship to `types.coercion_context`

`types.coercion_context` defines the process default used by `TypedValue::convertTo(...)`.

The current section treats the process default and the session setting as separate control surfaces:
- `types.coercion_context` is the explicit cast and conversion default authority
- `operator.strict_mode` is the session coercion strictness authority for runtime paths that consult session state

This section does not claim that every implicit coercion branch consults both controls equally. Only directly proved branch behavior is authoritative.

## Unsupported custom cast and operator DDL

The audited current parser and runtime sources prove `CAST` expression syntax. They do not prove current authoritative support for:
- `CREATE CAST`
- `DROP CAST`
- `CREATE OPERATOR`
- `DROP OPERATOR`

Accordingly:
- no durable custom cast catalog is part of the shipped authority
- no durable custom operator catalog is part of the shipped authority
- no implementation may infer those features from older prose
