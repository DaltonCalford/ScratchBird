# Admin SWEEP AND VACUUM: Runtime
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Partial

## Form
~~~sql
SWEEP DATABASE;
~~~

## Notes
- Parser routing:
  - `SWEEP DATABASE` emits native `SBLR3_SWEEP`.
- Executor routing:
  - `SBLR3_SWEEP` dispatches to sweep manager execution.
- Remaining partial area in 0.1.0:
  - extended maintenance-option surfaces are not implemented for this command family.
