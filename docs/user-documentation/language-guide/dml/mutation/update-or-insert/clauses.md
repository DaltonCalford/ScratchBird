# DML UPDATE OR INSERT: Clauses
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
UPDATE OR INSERT INTO <table_name>(<columns>) VALUES (...) MATCHING(<key_columns>);
~~~

## Notes
- Clause summary: Native parser has explicit UPDATE OR INSERT branch.
- Runtime note: Core upsert-like mutation flow is available in 0.1.0.
- Error/contract note: Matching key contract must resolve deterministically.
- Usage rationale: Firebird-style deterministic merge for key-based writes.
