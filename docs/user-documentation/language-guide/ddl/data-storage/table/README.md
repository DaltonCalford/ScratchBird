# DDL Object: TABLE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Table lifecycle is command-complete including DESCRIBE and TRUNCATE companion flow.
- Runtime note: Runtime table lifecycle is closed for primary forms in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE TABLE <schema.table_name> (...); |
| ALTER | Supported | ALTER TABLE <schema.table_name> <alter_actions>; |
| SHOW | Supported | SHOW TABLE <schema.table_name>; SHOW TABLES [FROM <schema_name>] [LIKE <pattern>]; |
| DESCRIBE | Supported | DESCRIBE <schema.table_name>; |
| DROP | Supported | DROP TABLE <schema.table_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
