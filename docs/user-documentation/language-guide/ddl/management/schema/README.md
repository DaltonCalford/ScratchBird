# DDL Object: SCHEMA
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Management README](../README.md)

## Summary
- Family: Management
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Schema namespace lifecycle is closed in parser command families.
- Runtime note: Runtime metadata path is available in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE SCHEMA <schema_name>; |
| ALTER | Supported | ALTER SCHEMA <schema_name> RENAME TO <new_name>; |
| SHOW | Supported | SHOW SCHEMA <schema_name>; |
| DESCRIBE | Not available | -- No DESCRIBE variant for SCHEMA. |
| DROP | Supported | DROP SCHEMA <schema_name> [RESTRICT|CASCADE]; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
