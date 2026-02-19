# DDL Object: PACKAGE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Programmability README](../README.md)

## Summary
- Family: Programmability
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Package lifecycle is command-complete through SHOW and DROP.
- Runtime note: Package/runtime closure follows procedure/function body constraints.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE PACKAGE <package_name> AS BEGIN ... END; |
| ALTER | Supported | ALTER PACKAGE <package_name> <alter_actions>; |
| SHOW | Supported | SHOW PACKAGE <package_name>; SHOW PACKAGES; |
| DESCRIBE | Not available | -- No DESCRIBE variant for PACKAGE. |
| DROP | Supported | DROP PACKAGE <package_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
