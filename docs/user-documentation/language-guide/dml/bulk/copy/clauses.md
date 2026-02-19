# DML COPY: Clauses
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DML README](../../README.md)
- [Family README](../README.md)
- [Statement README](README.md)

Series navigation:
- Previous: [Statement](statement.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Supported

## Syntax
~~~sql
COPY <table_name> [(<columns>)] FROM '<path>' [WITH (...)] OR COPY (<query>) TO '<path>' [WITH (...)] ;
~~~

## Notes
- Clause summary: Supports import/export shape with options for format and delimiters.
- Runtime note: Bulk path exists in parser command family; backend I/O behavior depends on runtime environment.
- Error/contract note: File path, format contract, and permission checks gate execution.
- Usage rationale: High-volume ingest/export with explicit format controls.
