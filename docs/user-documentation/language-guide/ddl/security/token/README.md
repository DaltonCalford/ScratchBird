# DDL Object: TOKEN
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Security README](../README.md)

## Summary
- Family: Security
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Security token family is command-partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE TOKEN <token_name> FOR USER <user_name> ...; |
| ALTER | Supported | ALTER TOKEN <token_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW TOKEN command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for TOKEN. |
| DROP | Supported | DROP TOKEN <token_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
