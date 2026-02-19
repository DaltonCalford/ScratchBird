# Admin SHOW SESSION CONTROLS: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Form
~~~sql
SHOW SQL DIALECT; SHOW TIME ZONE; SHOW ALL; SHOW <var_path>;
~~~

## Notes
- Details: SHOW command family includes session/system display variants.
- Runtime note: Runtime returns current session/system control values through SHOW path.
- Error/contract note: Unknown variable paths are rejected by show contract validation.
- Usage rationale: Auditing and debugging current session state.
