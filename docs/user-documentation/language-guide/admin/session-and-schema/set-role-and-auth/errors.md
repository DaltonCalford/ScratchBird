# Admin SET ROLE AND SESSION AUTHORIZATION: Error Contracts
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)
- [Topic README](README.md)

Series navigation:
- Previous: [Runtime](runtime.md)
- Next: [Topic README](README.md)

## Coverage
- Status: Supported

## SET ROLE Errors

- `Role '<name>' not found`
- `Permission denied: Role '<name>' not granted to current user`
- `SET ROLE requires COMMIT or ROLLBACK before switching roles` (policy=`error`, transaction boundary conflict)
- `MFA step-up required for privileged command` (SQLSTATE `28000`)

## SET SESSION AUTHORIZATION Errors

- `Permission denied: SET SESSION AUTHORIZATION (superuser only)`
- `User '<name>' does not exist`
- `SET SESSION AUTHORIZATION requires connection context`

## Related Policy/Auth Errors

When command execution is blocked at auth layer (before query execution), deterministic auth-policy errors can include:

- `AUTH_POLICY_NEGOTIATION_REQUIRED`
- `AUTH_POLICY_METHOD_DENIED`
- `AUTH_POLICY_REQUIRED_METHOD`
- `AUTH_POLICY_TRANSPORT_DENIED`
