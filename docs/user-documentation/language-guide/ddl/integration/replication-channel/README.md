# DDL Object: REPLICATION CHANNEL
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Replication commands parse and emit; runtime still includes bridge-partial areas.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE REPLICATION CHANNEL <channel_name> DIRECTION <ONE_WAY|BIDIRECTIONAL> SOURCE <source_name> TARGET <target_name>; |
| ALTER | Supported | ALTER REPLICATION CHANNEL <channel_name> <alter_actions>; RESYNC REPLICATION CHANNEL <channel_name>; |
| SHOW | Not available | -- No explicit SHOW REPLICATION CHANNEL command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for REPLICATION CHANNEL. |
| DROP | Supported | DROP REPLICATION CHANNEL <channel_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
