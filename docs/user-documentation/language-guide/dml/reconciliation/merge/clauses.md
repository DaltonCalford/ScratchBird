# DML MERGE: Clauses
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
MERGE INTO <target> USING <source> ON <match_predicate> WHEN MATCHED THEN ... WHEN NOT MATCHED THEN ...;
~~~

## Notes
- Clause summary: Supports deterministic match branches for update/insert/delete operations.
- Runtime note: Core parser surface exists; runtime behavior depends on branch shape and target semantics.
- Error/contract note: Branch contract validation enforces match and action semantics.
- Usage rationale: Two-stream reconciliation and change-apply workflows.
