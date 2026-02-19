# DDL Object: INDEX
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Classic index lifecycle is command-complete via SHOW INDEX surfaces.
- Runtime note: Core index paths are available; some advanced method semantics depend on backend operator classes.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE INDEX <index_name> ON <table_name>(<columns>) [USING <method>]; |
| ALTER | Supported | ALTER INDEX <index_name> <alter_actions>; ALTER INDEX DEFAULTS FOR <index_type> SET (...); |
| SHOW | Supported | SHOW INDEX <index_name>; SHOW INDEXES FROM <table_name>; |
| DESCRIBE | Not available | -- No DESCRIBE variant for INDEX. |
| DROP | Supported | DROP INDEX <index_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
