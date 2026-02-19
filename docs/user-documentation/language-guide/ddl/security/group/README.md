# DDL Object: GROUP
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Security README](../README.md)

## Summary
- Family: Security
- Lifecycle status: Partial command lifecycle
- Lifecycle note: ALTER supports generic rename/move; SHOW and DESCRIBE remain missing.
- Runtime note: Group lifecycle is partially closed in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE GROUP <group_name>; |
| ALTER | Supported | ALTER GROUP [IF EXISTS] <group_name> RENAME TO <new_name>; ALTER GROUP [IF EXISTS] <group_name> SET SCHEMA <schema_path>; |
| SHOW | Not available | -- No explicit SHOW GROUP command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for GROUP. |
| DROP | Supported | DROP GROUP <group_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
