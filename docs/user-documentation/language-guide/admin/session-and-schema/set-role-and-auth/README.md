# Admin Topic: SET ROLE AND SESSION AUTHORIZATION
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [Admin README](../../README.md)
- [Family README](../README.md)

## Summary
- Topic family: Session And Schema
- Status in 0.1.0: Supported

## Implementation Matrix (0.1.0)
- `SET ROLE` / `RESET ROLE`: Supported end-to-end (parser, emitter, executor runtime).
- `SET SESSION AUTHORIZATION` / `RESET SESSION AUTHORIZATION`: Supported end-to-end (parser, emitter, executor runtime).

## Key Runtime Constraints
- `SET ROLE` requires role membership.
- `SET SESSION AUTHORIZATION` is superuser-only.
- MFA step-up policy can block `SET ROLE`/`RESET ROLE` until step-up succeeds.

## Documentation Series
- [Syntax](syntax.md)
- [Semantics](semantics.md)
- [Examples](examples.md)
- [Runtime](runtime.md)
- [Error Contracts](errors.md)
