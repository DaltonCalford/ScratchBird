# PSQL EXCEPTION BLOCK: Runtime
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [PSQL README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Form
~~~sql
BEGIN ... WHEN ANY DO EXCEPTION <name>; END;
~~~

## Notes
- Details: EXCEPTION handling blocks are parser-supported in routine bodies.
- Runtime note: Runtime propagates and maps exceptions through routine execution path.
- Error/contract note: Unhandled exceptions surface deterministic error contracts.
- Usage rationale: Explicit exception routing and failure handling in routines.
