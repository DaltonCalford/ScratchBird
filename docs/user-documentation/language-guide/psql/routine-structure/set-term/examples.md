# PSQL SET TERM: Examples
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
- Status: Partial

## Form
~~~sql
SET TERM <new_terminator> [<old_terminator>];
~~~

## Notes
- Details: SET TERM is parser+emitter supported; client/script splitting remains caller-side workflow.
- Runtime note: Native parseStatements still uses semicolon splitting unless caller applies statement segmentation.
- Error/contract note: Incorrect delimiter management causes script-level parse failures.
- Usage rationale: Required when authoring multi-statement routine definitions in script files.
