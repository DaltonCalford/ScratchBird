# PSQL CREATE EXCEPTION: Examples
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Semantics](semantics.md)
- Next: [Runtime](runtime.md)

## Coverage
- Status: Supported

## Form
~~~sql
CREATE EXCEPTION <name> '<message>'; DROP EXCEPTION <name>;
~~~

## Notes
- Details: Exception objects are created/dropped via DDL and consumed in PSQL blocks.
- Runtime note: Create/drop command paths are available; ALTER/SHOW are not available in 0.1.0.
- Error/contract note: DDL-level validation enforces exception object naming and message contracts.
- Usage rationale: Defines reusable named exceptions for routine/trigger logic.
