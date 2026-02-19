# DDL Object: CDC TABLE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: CDC surfaces exist for ETL/replication lineage, including required TRACK contract keys.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE CDC TABLE <table_name> TRACK (LAST_MODIFIED_TXN_ID, ROW_UUID); |
| ALTER | Supported | ALTER CDC TABLE <table_name> TRACK (LAST_MODIFIED_TXN_ID, ROW_UUID); |
| SHOW | Not available | -- No explicit SHOW CDC TABLE command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for CDC TABLE. |
| DROP | Supported | DROP CDC TABLE <table_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
