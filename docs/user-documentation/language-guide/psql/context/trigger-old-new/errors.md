# PSQL TRIGGER OLD NEW CONTEXT: Error Contracts
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
OLD.<column_name>, NEW.<column_name>;
~~~

## Notes
- Details: Trigger runtime exposes OLD and NEW row context helpers.
- Runtime note: Row-level triggers map old/new column values in executor trigger context.
- Error/contract note: Access outside trigger context is invalid.
- Usage rationale: Row-delta logic and audit rules in trigger bodies.
