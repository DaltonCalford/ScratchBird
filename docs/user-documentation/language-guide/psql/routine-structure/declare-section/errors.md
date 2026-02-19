# PSQL DECLARE SECTION: Error Contracts
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Runtime](runtime.md)
- Next: [Topic README](README.md)

## Coverage
- Status: Supported

## Form
~~~sql
DECLARE <var_name> <type_name> [= <expr>];
~~~

## Notes
- Details: Procedure/function blocks support variable declaration prior to executable statements.
- Runtime note: Declaration parsing and symbol binding are available for core forms.
- Error/contract note: Invalid declarations fail in parser or semantic validation stage.
- Usage rationale: Defines local working state for procedural blocks.
