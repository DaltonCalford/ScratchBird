# DML UPDATE: Error Contracts
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DML README](../../README.md)
- [Family README](../README.md)
- [Statement README](README.md)

Series navigation:
- Previous: [Runtime](runtime.md)
- Next: [Statement README](README.md)

## Coverage
- Status: Supported

## Syntax
~~~sql
UPDATE <table_name> SET <assignments> WHERE <predicate> [RETURNING ...];
~~~

## Notes
- Clause summary: Supports searched update and RETURNING projection.
- Runtime note: Core update mutation path is available in 0.1.0.
- Error/contract note: Type conversion and constraint validation are enforced per target column.
- Usage rationale: In-place state transition for existing rows.
