# DDL Object: CUBE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Cluster And Service README](../README.md)

## Summary
- Family: Cluster And Service
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Cube object lifecycle is parser+emitter complete through create/alter/show/drop forms.
- Runtime note: Runtime semantic bridge is still partial in 0.1.0 (see ../../TODO_BETA_0_2_0.md).

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE CUBE <cube_name> AS <cube_select>; |
| ALTER | Supported | ALTER CUBE <cube_name> <alter_actions>; REFRESH CUBE <cube_name> [FULL|INCREMENTAL]; |
| SHOW | Supported | SHOW CUBE STATS <cube_name>; CUBE SHOW STATS <cube_name>; |
| DESCRIBE | Not available | -- No DESCRIBE variant for CUBE. |
| DROP | Supported | DROP CUBE <cube_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
