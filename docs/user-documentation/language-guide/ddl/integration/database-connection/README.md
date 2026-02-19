# DDL Object: DATABASE CONNECTION
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: External connection object has create/alter/drop surfaces but no explicit show/describe command.
- Runtime note: Current runtime normalization is through admin/config key contracts rather than dedicated catalog object opcode family.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE DATABASE CONNECTION <name> HOST '<host>' MOUNT '<mount_path>' AUTH_MODE <SHARED|NAMED> ROLE '<role>' PASSWORD '<secret>' [GROUP '<group_name>']; |
| ALTER | Supported | ALTER DATABASE CONNECTION <name> AUTH_MODE <SHARED|NAMED> ...; |
| SHOW | Not available | -- No explicit SHOW DATABASE CONNECTION command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for DATABASE CONNECTION. |
| DROP | Supported | DROP DATABASE CONNECTION <name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
