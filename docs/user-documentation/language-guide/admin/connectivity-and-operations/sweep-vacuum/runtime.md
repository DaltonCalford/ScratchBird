# Admin SWEEP AND VACUUM: Runtime
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
- Status: Partial

## Form
~~~sql
SWEEP DATABASE;
VACUUM [DATABASE];
~~~

## Notes
- Parser routing:
  - `SWEEP DATABASE` emits native `SBLR3_SWEEP`.
  - `VACUUM` emits `SBLR3_ADMIN_VACUUM_ALIAS`.
- Executor routing:
  - `SBLR3_ADMIN_VACUUM_ALIAS` is dispatched to the sweep manager (`executeSweep(...)`), not to PostgreSQL vacuum semantics.
- Telemetry:
  - successful alias dispatch is recorded as `vnext_opcode_dispatch=ok` for `SBLR3_ADMIN_VACUUM_ALIAS`.
- Remaining partial area in 0.1.0:
  - extended maintenance-option surfaces are not implemented for this command family.
