# DDL Object: INDEX
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Index lifecycle is command-complete via `CREATE`, `ALTER`, `SHOW`, and `DROP` surfaces.
- Runtime note: `CREATE INDEX` includes detailed method/option coverage for core, vector/ANN, and emulation-surface methods.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | `CREATE [CONCURRENTLY] [IF NOT EXISTS] [name] ON table [USING method] (...) [INCLUDE (...)] [WHERE ...] [TABLESPACE ...] [WITH (...)]` |
| ALTER | Supported | `ALTER INDEX <index_name> <alter_actions>; ALTER INDEX DEFAULTS FOR <index_type> SET (...);` |
| SHOW | Supported | `SHOW INDEX <index_name>; SHOW INDEXES FROM <table_name>;` |
| DESCRIBE | Not available | No `DESCRIBE INDEX` variant in native v3. |
| DROP | Supported | `DROP INDEX <index_name>;` |

## Method and Option Reference
- Method inventory, per-method purpose, and `WITH (...)` option semantics are documented in [CREATE](create.md).

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
