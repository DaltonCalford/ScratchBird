# Decision Record

## DR-13-01: explicit cast authority is centralized, implicit coercion is not

ScratchBird does have one strong cast authority: `TypedValue::convertTo(...)`. It does not currently have one equally centralized implicit coercion registry. The section therefore treats explicit casts as centralized and implicit coercion as distributed current-state authority.

## DR-13-02: column write coercion is part of section 13 authority

`INSERT`, `UPDATE`, merge-like write paths, trigger assignments, and related executor write surfaces repeatedly route values through `coerceValueForColumn(...)`. That makes write-path column coercion part of this section, not just part of generic executor behavior.

## DR-13-03: `operator.strict_mode` is a session control, not a local-transaction control

The executor accepts `SET operator.strict_mode` and rejects `SET LOCAL operator.strict_mode`. The section therefore treats the setting as a session-scoped coercion control that persists until reset or session teardown.

## DR-13-04: `types.coercion_context` provides the process default

`TypedValue::convertTo(...)` reads `types.coercion_context` and currently recognizes `STRICT` and `PERMISSIVE`, defaulting to `STRICT`. That process-level default remains authoritative unless a more specific runtime path documents its own override behavior.

## DR-13-05: custom cast and operator objects are fail-closed

The audited parser and runtime sources do not prove current authoritative support for durable user-defined `CREATE CAST`, `DROP CAST`, `CREATE OPERATOR`, or `DROP OPERATOR` objects. This section therefore rejects those as shipped implementation authority.
