# Admin SWEEP AND VACUUM: Syntax
Last modified: 2026-02-19

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

## Compatibility Alias Form
~~~sql
VACUUM;
VACUUM DATABASE;
~~~

## Not Supported In V3
~~~sql
VACUUM FULL;
VACUUM (ANALYZE);
VACUUM my_table;
~~~

## Notes
- `SWEEP DATABASE` is the native MGA garbage-collection command.
- `VACUUM` is accepted only as a compatibility alias to the same sweep/GC behavior.
