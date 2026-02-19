# DDL Object: STATISTICS
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Create-only parser surface in 0.1.0.
- Runtime note: Lifecycle remains create-only and requires closure for beta-hardening.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE STATISTICS <name> (<kind>) ON <columns> FROM <table_name>; |
| ALTER | Not available | -- No explicit ALTER STATISTICS command in 0.1.0. |
| SHOW | Not available | -- No explicit SHOW STATISTICS command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for STATISTICS. |
| DROP | Not available | -- No explicit DROP STATISTICS command in 0.1.0. |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
