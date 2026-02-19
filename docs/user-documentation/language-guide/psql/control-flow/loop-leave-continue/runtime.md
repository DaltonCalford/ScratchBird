# PSQL LOOP LEAVE CONTINUE: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Form
~~~sql
LOOP ... LEAVE; CONTINUE; END LOOP;
~~~

## Notes
- Details: Unconditional LOOP with LEAVE/CONTINUE control is parser-supported.
- Runtime note: Control transfer works within active loop scope.
- Error/contract note: Out-of-scope LEAVE/CONTINUE usage is rejected.
- Usage rationale: Fine-grained loop control in procedural flows.
