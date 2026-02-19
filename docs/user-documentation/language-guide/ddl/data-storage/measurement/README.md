# DDL Object: MEASUREMENT
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Partial command lifecycle
- Lifecycle note: No DROP/SHOW commands keep lifecycle partial.
- Runtime note: Runtime closure for full measurement lifecycle is not complete in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE MEASUREMENT <measurement_name> ON <table_or_stream>(<columns>); |
| ALTER | Supported | ALTER MEASUREMENT <measurement_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW MEASUREMENT command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for MEASUREMENT. |
| DROP | Not available | -- No explicit DROP MEASUREMENT command in 0.1.0. |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
