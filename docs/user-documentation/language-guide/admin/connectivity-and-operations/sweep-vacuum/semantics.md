# Admin SWEEP AND VACUUM: Semantics
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Syntax](syntax.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Partial

## Form
~~~sql
SWEEP DATABASE;
VACUUM [DATABASE];
~~~

## Notes
- `SWEEP DATABASE` is ScratchBird's native MGA garbage-collection surface.
- `VACUUM` is a PostgreSQL-compatibility alias and maps to the same sweep/GC execution path.
- Alias mapping is database-wide and does not introduce PostgreSQL vacuum modes (for example `FULL` or table-level vacuum variants).
- Semantics are intentionally MGA-oriented: reclaimable version cleanup and transaction-horizon advancement are handled via sweep manager behavior.
