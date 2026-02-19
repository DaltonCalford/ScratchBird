# DML DELETE: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DML README](../../README.md)
- [Family README](../README.md)
- [Statement README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Syntax
~~~sql
DELETE FROM <table_name> WHERE <predicate> [RETURNING ...];
~~~

## Notes
- Clause summary: Supports searched delete and RETURNING where applicable.
- Runtime note: Core delete mutation path is available in 0.1.0.
- Error/contract note: Constraint and trigger side effects apply based on table metadata.
- Usage rationale: Row lifecycle cleanup and policy enforcement.
