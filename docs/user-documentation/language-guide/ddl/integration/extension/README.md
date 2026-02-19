# DDL Object: EXTENSION
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Extension lifecycle is command-partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE EXTENSION <extension_name> [WITH VERSION '<version>']; |
| ALTER | Supported | ALTER EXTENSION <extension_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW EXTENSION command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for EXTENSION. |
| DROP | Supported | DROP EXTENSION <extension_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
