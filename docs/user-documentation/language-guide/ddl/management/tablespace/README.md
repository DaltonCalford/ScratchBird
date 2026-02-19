# DDL Object: TABLESPACE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Management README](../README.md)

## Summary
- Family: Management
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Missing explicit SHOW/DESCRIBE command keeps lifecycle partial.
- Runtime note: DDL mutation surface exists; observability is indirect in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE TABLESPACE <tablespace_name> LOCATION '<path>'; |
| ALTER | Supported | ALTER TABLESPACE <tablespace_name> RESIZE ...; |
| SHOW | Not available | -- No explicit SHOW TABLESPACE command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for TABLESPACE. |
| DROP | Supported | DROP TABLESPACE <tablespace_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
