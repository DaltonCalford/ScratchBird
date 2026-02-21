# Admin SET ROLE AND SESSION AUTHORIZATION: Runtime
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Examples](examples.md)
- Next: [Error Contracts](errors.md)

## Coverage
- Status: Supported

## Execution Paths

### SET ROLE

- Parser: `parseSet()` recognizes role forms.
- Emitter: emits `SBLR3_SET_ROLE`.
- Executor: v3 session-control runtime handles role switch, role membership check, and schema context update.

### SET SESSION AUTHORIZATION

- Parser: `parseSet()` recognizes session authorization forms.
- Emitter: emits `SBLR3_SET_SESSION_AUTH`.
- Executor: v3 session-control runtime handles effective-user switch/reset.

## Authorization Hooks

- Role/auth changes require active authenticated connection context.
- `SET SESSION AUTHORIZATION` checks `isSuperuser()`.
- Privileged role/security SQL is subject to MFA step-up validation window.

## Server Behavior

- Server session executes SBLR bytecode; plain SQL text is not executed directly by server runtime.
- On MFA step-up violation for privileged command, server returns SQLSTATE `28000`.
