# DDL Object: CONNECTION RULE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Security README](../README.md)

## Summary
- Family: Security
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Connection rule family is command-partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE CONNECTION RULE <rule_name> ...; |
| ALTER | Supported | ALTER CONNECTION RULE <rule_name> ...; |
| SHOW | Not available | -- No explicit SHOW CONNECTION RULE command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for CONNECTION RULE. |
| DROP | Supported | DROP CONNECTION RULE <rule_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
