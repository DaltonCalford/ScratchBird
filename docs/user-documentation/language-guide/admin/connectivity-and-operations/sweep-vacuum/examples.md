# Admin SWEEP AND VACUUM: Examples
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Semantics](semantics.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Partial

## Canonical Form
~~~sql
-- Native MGA sweep/GC
SWEEP DATABASE;
~~~

## Rejected Form
~~~sql
VACUUM FULL;
-- PRS_0505 in v3: VACUUM aliases are retired; use SWEEP DATABASE.
~~~
