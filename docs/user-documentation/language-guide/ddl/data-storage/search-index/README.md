# DDL Object: SEARCH INDEX
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Partial command lifecycle
- Lifecycle note: ALTER supports only REBUILD in 0.1.0; SHOW/DESCRIBE are missing.
- Runtime note: Lifecycle remains partial pending broader ALTER and observability commands.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE SEARCH INDEX <index_name> ON <table_name>(<search_columns>); |
| ALTER | Partial | ALTER SEARCH INDEX <index_name> REBUILD [ONLINE|OFFLINE]; |
| SHOW | Not available | -- No explicit SHOW SEARCH INDEX command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for SEARCH INDEX. |
| DROP | Supported | DROP SEARCH INDEX <index_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
