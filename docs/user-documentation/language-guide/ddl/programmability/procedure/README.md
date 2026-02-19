# DDL Object: PROCEDURE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Programmability README](../README.md)

## Summary
- Family: Programmability
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Procedure lifecycle is command-complete through SHOW and DROP.
- Runtime note: Routine body semantics support IF/ELSIF/ELSE, loops, WHEN, EXCEPTION; TRY blocks are not implemented.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE PROCEDURE <procedure_name>(...) AS DECLARE ... BEGIN ... END; |
| ALTER | Supported | ALTER PROCEDURE <procedure_name>(...) <alter_actions>; |
| SHOW | Supported | SHOW PROCEDURE <procedure_name>; SHOW PROCEDURES; |
| DESCRIBE | Not available | -- No DESCRIBE variant for PROCEDURE. |
| DROP | Supported | DROP PROCEDURE <procedure_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
