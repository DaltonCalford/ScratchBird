# PSQL FOR LOOP: Examples
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Semantics](semantics.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Supported

## Form
~~~sql
FOR <var> IN <range_or_query> DO ... END FOR;
~~~

## Notes
- Details: FOR loops are parsed in procedural control flow set.
- Runtime note: Loop variable binding and iteration semantics are available for core forms.
- Error/contract note: Range/query mismatch conditions are rejected by semantic checks.
- Usage rationale: Deterministic iteration over ranges or query-derived rows.
