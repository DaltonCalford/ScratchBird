# PSQL IF ELSIF ELSE: Semantics
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Syntax](syntax.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Supported

## Form
~~~sql
IF (<predicate>) THEN ... ELSIF (<predicate>) THEN ... ELSE ... END IF;
~~~

## Notes
- Details: Native v3 control flow includes IF with ELSIF and ELSE branches.
- Runtime note: Branch semantics are available inside routine/trigger bodies.
- Error/contract note: Invalid branch structure fails parser contract checks.
- Usage rationale: Core conditional branching for procedural logic.
