# DDL Object: ROLE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Security README](../README.md)

## Summary
- Family: Security
- Lifecycle status: Partial command lifecycle
- Lifecycle note: ALTER supports generic rename/move; DESCRIBE remains missing.
- Runtime note: Role lifecycle is partially closed in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE ROLE <role_name>; |
| ALTER | Supported | ALTER ROLE [IF EXISTS] <role_name> RENAME TO <new_name>; ALTER ROLE [IF EXISTS] <role_name> SET SCHEMA <schema_path>; |
| SHOW | Supported | SHOW ROLE <role_name>; SHOW ROLES; |
| DESCRIBE | Not available | -- No DESCRIBE variant for ROLE. |
| DROP | Supported | DROP ROLE <role_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
