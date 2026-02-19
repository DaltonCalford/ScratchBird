# PSQL WHILE LOOP: Syntax
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
WHILE (<predicate>) DO ... END WHILE;
~~~

## Notes
- Details: WHILE loops are part of procedural control flow support.
- Runtime note: Loop execution follows routine runtime semantics.
- Error/contract note: Non-terminating conditions remain caller responsibility.
- Usage rationale: Repeated execution while a predicate remains true.
