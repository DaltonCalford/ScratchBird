# Admin SWEEP AND VACUUM: Syntax
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Topic README](README.md)
- Next: [Semantics](semantics.md)

## Coverage
- Status: Partial

## Native Form
~~~sql
SWEEP DATABASE;
~~~

## Rejected Alias Forms
~~~sql
VACUUM;
VACUUM DATABASE;
VACUUM FULL;
VACUUM (ANALYZE);
VACUUM my_table;
~~~

## Notes
- `SWEEP DATABASE` is the canonical MGA garbage-collection command in native v3.
- `VACUUM` and all variants are rejected in native v3 (`PRS_0505`).
