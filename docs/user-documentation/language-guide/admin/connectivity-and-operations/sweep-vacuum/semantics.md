# Admin SWEEP AND VACUUM: Semantics
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Syntax](syntax.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Partial

## Form
~~~sql
SWEEP DATABASE;
~~~

## Notes
- `SWEEP DATABASE` drives MGA-oriented cleanup and transaction-horizon advancement.
- PostgreSQL-style `VACUUM` semantics are not part of native v3.
