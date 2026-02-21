# Admin SET ROLE AND SESSION AUTHORIZATION: Semantics
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
- Status: Supported

## SET ROLE / RESET ROLE

`SET ROLE`:

1. Resolves role by name.
2. Verifies role is granted to current user.
3. Sets active role in connection context.
4. Sets current schema/search path to role default schema when available.

`RESET ROLE`:

1. Clears active role.
2. Restores schema context to current user home schema.

Role switch transaction policy (from `security.role_switch_default_action`):

- `commit`: auto-commit before switch
- `rollback`: auto-rollback before switch
- `error`: deny switch until caller commits/rolls back
- `defer`: keep pending state and defer switch boundary behavior

## SET/RESET SESSION AUTHORIZATION

- Changes effective user identity for session context.
- `RESET SESSION AUTHORIZATION` restores original session user.
- Requires superuser privileges.

## MFA Interaction

If policy requires MFA, privileged role/security operations can require active step-up window. Without valid step-up, role operation fails with authorization error.

## Implementation Notes

- `RESET ROLE` and `SET ROLE NONE/DEFAULT` all map to reset semantics.
- `RESET SESSION AUTHORIZATION` and `SET SESSION AUTHORIZATION DEFAULT` both restore the original session user.
