# DDL Object: FOREIGN SERVER
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Runtime closure for server/object semantics is not fully canonicalized in v3 dispatch.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE SERVER <server_name> FOREIGN DATA WRAPPER <fdw_name> OPTIONS (...); |
| ALTER | Supported | ALTER SERVER <server_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW SERVER command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for FOREIGN SERVER. |
| DROP | Supported | DROP SERVER <server_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
