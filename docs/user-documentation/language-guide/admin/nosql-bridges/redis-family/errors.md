# Admin REDIS BRIDGE COMMANDS: Error Contracts
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Runtime](runtime.md)
- Next: [Topic README](README.md)

## Coverage
- Status: Partial

## Form
~~~sql
REDIS STRING ...; REDIS HASH ...; REDIS LIST ...; REDIS SET ...; REDIS ZSET ...; REDIS STREAM ...; REDIS PUBSUB ...;
~~~

## Notes
- Details: Redis family bridge forms are explicit parser/emitter surfaces.
- Runtime note: Runtime is bridge-partial in 0.1.0.
- Error/contract note: Unsupported semantic variants return deterministic bridge errors.
- Usage rationale: Low-latency key-value and stream operations from SQL entrypoint.
