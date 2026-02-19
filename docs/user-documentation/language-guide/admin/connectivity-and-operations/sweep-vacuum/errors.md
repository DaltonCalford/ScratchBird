# Admin SWEEP AND VACUUM: Error Contracts
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Runtime](runtime.md)
- Next: [Topic README](README.md)

## Coverage
- Status: Partial

## Form
~~~sql
SWEEP DATABASE;
VACUUM [DATABASE];
~~~

## Parser/Compilation Errors
- `PRS_0505`: `VACUUM` supplied with unsupported options (for example `VACUUM FULL`).

## Runtime Errors
- `Sweep manager not available`: runtime cannot route command to GC manager.
- `Sweep failed: ...`: sweep manager returned an execution error.

## Notes
- `SBLR3_ADMIN_VACUUM_ALIAS` is no longer rejected by vNext bridge fallback (`BRG_0406`); it is mapped to sweep/GC execution.
