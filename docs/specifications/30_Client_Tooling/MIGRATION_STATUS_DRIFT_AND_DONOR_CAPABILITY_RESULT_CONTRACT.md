# Migration Status, Drift, and Donor Capability Result Contract

## Scope

This file defines the operator-facing result contract for migration and
passthrough inspection surfaces.

## Required result families

The operator-facing migration inspection surface must remain able to return, at
minimum:

- donor identity
- donor capability class
- extraction mode class
- current migration phase
- unresolved drift class
- unresolved drift count
- cutover readiness class
- last assessment time
- last successful extraction time, when available
- refusal or warning code, when not ready

## Donor capability rule

Donor capability must be returned as normalized canonical classes, not only as
free-form connector text.

## Drift rule

Unresolved drift must remain visible as structured result data. It must not be
collapsed into a boolean ready or not-ready output.

## Cutover readiness rule

Cutover readiness must distinguish, at minimum:

- ready
- ready with bounded warnings
- blocked by unresolved drift
- blocked by donor weakness
- blocked by missing assessment

## Native SQL and tooling boundary

Native SQL or admin-tooling inspection surfaces may choose different display
formats, but they must preserve the same canonical fields and meanings.

## Fail-closed rules

The operator surface shall not:

1. report ready without a donor capability class
2. report ready while hiding unresolved drift
3. report cutover-ready on a weak donor without exposing the weak-donor class
4. silently degrade to plain text when structured result fields are available
