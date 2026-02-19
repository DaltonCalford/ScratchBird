# Admin CONNECT AND DISCONNECT: Semantics
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Syntax](syntax.md)
- Next: [Examples](examples.md)

## Coverage
- Status: Supported

## Form
~~~sql
CONNECT <connection_target> [AS <user>] [PASSWORD <secret>]; DISCONNECT;
~~~

## Notes
- Details: CONNECT and DISCONNECT are explicit top-level statements.
- Runtime note: Connection/session lifecycle controls are available in parser and runtime.
- Error/contract note: Authentication failures and connection contract errors are deterministic.
- Usage rationale: Session attachment and teardown control.
