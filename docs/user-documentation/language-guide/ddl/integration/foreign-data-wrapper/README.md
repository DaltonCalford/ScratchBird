# DDL Object: FOREIGN DATA WRAPPER
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Create-only parser surface in 0.1.0.
- Runtime note: Lifecycle remains create-only and runtime closure is incomplete.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE FOREIGN DATA WRAPPER <fdw_name> [HANDLER <handler_name>]; |
| ALTER | Not available | -- No explicit ALTER FOREIGN DATA WRAPPER command in 0.1.0. |
| SHOW | Not available | -- No explicit SHOW FOREIGN DATA WRAPPER command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for FOREIGN DATA WRAPPER. |
| DROP | Not available | -- No explicit DROP FOREIGN DATA WRAPPER command in 0.1.0. |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
