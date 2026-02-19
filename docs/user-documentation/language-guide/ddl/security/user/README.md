# DDL Object: USER
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Security README](../README.md)

## Summary
- Family: Security
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Core user DDL is present; visibility is currently indirect.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE USER <user_name> PASSWORD '<secret>'; |
| ALTER | Supported | ALTER USER <user_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW USER command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for USER. |
| DROP | Supported | DROP USER <user_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
