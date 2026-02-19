# Admin CQL BRIDGE COMMANDS: Examples
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
CQL KEYSPACE ...; CQL BATCH ...; CQL TTL ...; CQL WRITETIME ...;
~~~

## Notes
- Details: Cassandra-style command forms are explicit in parser dispatch.
- Runtime note: Runtime is bridge-partial in 0.1.0.
- Error/contract note: Unsupported semantic variants return deterministic bridge errors.
- Usage rationale: Preserves CQL intent from SQL entrypoint.
