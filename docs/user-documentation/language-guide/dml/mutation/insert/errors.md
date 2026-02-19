# DML INSERT: Error Contracts
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
INSERT INTO <table_name>(<columns>) VALUES (...) [RETURNING ...] [ON CONFLICT ...];
~~~

## Notes
- Clause summary: Supports VALUES, SELECT-source insert, RETURNING, and conflict-handling variants.
- Runtime note: Core insert mutation path is available in 0.1.0.
- Error/contract note: Contract validation applies to column counts, data type conversion, and constraint checks.
- Usage rationale: Canonical append/upsert ingestion statement.
