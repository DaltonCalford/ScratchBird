# DDL Object: DATABASE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Management README](../README.md)

## Summary
- Family: Management
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Native database lifecycle exists; emulated create path is additionally exposed.
- Runtime note: Core DATABASE lifecycle is available; EMULATED path is parser-rich but runtime contract is still being normalized.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE DATABASE <database_name>; CREATE DATABASE EMULATED <engine> ON SERVER <server_name> '<remote_path>' WITH OPTIONS (...) ALIAS <alias_name>; |
| ALTER | Supported | ALTER DATABASE <database_name> SET OWNER TO <role_name>; |
| SHOW | Supported | SHOW DATABASE; |
| DESCRIBE | Not available | -- No DESCRIBE variant for DATABASE. |
| DROP | Supported | DROP DATABASE <database_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
