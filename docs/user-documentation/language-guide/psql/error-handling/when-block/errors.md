# PSQL WHEN BLOCK: Error Contracts
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
WHEN <condition> DO ...;
~~~

## Notes
- Details: WHEN blocks are supported in procedural error/condition handling paths.
- Runtime note: Condition dispatch integrates with routine execution flow.
- Error/contract note: Invalid condition names or structure are rejected by parser/semantic validation.
- Usage rationale: Condition-driven handling for procedural operations.
