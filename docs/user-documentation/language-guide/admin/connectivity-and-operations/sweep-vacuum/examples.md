# Admin SWEEP AND VACUUM: Examples
Last modified: 2026-02-19

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

## Canonical Forms
~~~sql
-- Native MGA sweep/GC
SWEEP DATABASE;

-- Compatibility alias to the same sweep/GC runtime path
VACUUM;
VACUUM DATABASE;
~~~

## Rejected Form
~~~sql
VACUUM FULL;
-- PRS_0505 in V3: options are not supported for the VACUUM alias.
~~~
