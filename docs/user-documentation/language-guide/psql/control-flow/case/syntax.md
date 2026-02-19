# PSQL CASE: Syntax
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Topic README](README.md)
- Next: [Semantics](semantics.md)

## Coverage
- Status: Supported

## Form
~~~sql
CASE WHEN <predicate> THEN <expr> ELSE <expr> END;
~~~

## Notes
- Details: CASE expressions and CASE statement forms are accepted in native v3 procedural surfaces.
- Runtime note: Runtime expression evaluation supports CASE branching.
- Error/contract note: Type incompatibility across branches can trigger runtime cast/validation failures.
- Usage rationale: Declarative branching in expressions and control blocks.
