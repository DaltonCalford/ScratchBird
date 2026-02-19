# DDL Object: DOMAIN
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Domain lifecycle is command-complete; advanced domain options are available in CREATE DOMAIN grammar.
- Runtime note: Runtime support exists for core domain DDL in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE DOMAIN <domain_name> AS <type_spec> [constraints...]; |
| ALTER | Supported | ALTER DOMAIN <domain_name> <alter_actions>; |
| SHOW | Supported | SHOW DOMAIN <domain_name>; SHOW DOMAINS; |
| DESCRIBE | Not available | -- No DESCRIBE variant for DOMAIN. |
| DROP | Supported | DROP DOMAIN <domain_name> [RESTRICT|CASCADE]; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
