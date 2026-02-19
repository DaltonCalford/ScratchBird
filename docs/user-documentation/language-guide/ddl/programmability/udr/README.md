# DDL Object: UDR
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Programmability README](../README.md)

## Summary
- Family: Programmability
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Create/drop only in parser command surface.
- Runtime note: Lifecycle remains partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE UDR <udr_name> ...; |
| ALTER | Not available | -- No explicit ALTER UDR command in 0.1.0. |
| SHOW | Not available | -- No explicit SHOW UDR command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for UDR. |
| DROP | Supported | DROP UDR <udr_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
