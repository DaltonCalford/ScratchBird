# Admin MILVUS BRIDGE COMMANDS: Examples
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

## Form
~~~sql
MILVUS CREATE COLLECTION ...; MILVUS CREATE INDEX ...; MILVUS INSERT ...; MILVUS SEARCH ...;
~~~

## Notes
- Details: Milvus/vector bridge forms are explicit parser/emitter surfaces.
- Runtime note: Runtime is bridge-partial in 0.1.0.
- Error/contract note: Unsupported semantic variants return deterministic bridge errors.
- Usage rationale: Vector ANN lifecycle and query operations from SQL entrypoint.
