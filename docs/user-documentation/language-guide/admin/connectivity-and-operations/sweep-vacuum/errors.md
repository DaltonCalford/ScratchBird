# Admin SWEEP AND VACUUM: Error Contracts
Last modified: 2026-02-21

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
~~~

## Parser/Compilation Errors
- `PRS_0505`: `VACUUM` alias forms are rejected in native v3.

## Runtime Errors
- `Sweep manager not available`: runtime cannot route command to GC manager.
- `Sweep failed: ...`: sweep manager returned an execution error.
