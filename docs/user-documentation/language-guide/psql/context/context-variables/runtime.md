# PSQL CONTEXT VARIABLES: Runtime
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
CURRENT_USER, SESSION_USER, CURRENT_ROLE, CURRENT_CONNECTION, CURRENT_SESSION, CURRENT_TRANSACTION, NOW, CURRENT_DATE, CURRENT_TIME, CURRENT_TIMESTAMP;
~~~

## Notes
- Details: All listed context keywords are parsed as function/context expressions.
- Runtime note: CURRENT_TIMESTAMP is transaction-start anchored; NOW is wall-clock evaluation time.
- Error/contract note: Unavailable context values return null-like runtime values when no active context exists.
- Usage rationale: Identity/session/transaction introspection from SQL and PSQL.
