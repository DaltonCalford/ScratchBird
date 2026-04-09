# Beta 2 Relational Temporal Versioning And History Binding Model

## Purpose

Define first-class relational temporal tables over current MGA lineage so table
owners can expose history binding and time-travel query surfaces without
inventing a second visibility model.

## Governing rules

1. MGA version truth remains authoritative.
2. Temporal table state is a relational exposure over MGA lineage plus retained
   history rows.
3. Every temporal table must declare one history-binding policy.
4. Time-travel query lowering must be deterministic.
5. Retention and legal-hold policy must be explicit.

## Canonical metadata

- `sb_temporal_table`
  - `table_uuid`
  - `history_table_uuid`
  - `period_start_column_uuid`
  - `period_end_column_uuid`
  - `version_policy`
  - `retention_policy`
  - `enabled`
- `sb_temporal_history_binding`
  - `binding_uuid`
  - `table_uuid`
  - `history_table_uuid`
  - `copy_mode`
  - `schema_lockstep_policy`
  - `status`

## Admitted query forms

- `AS OF <timestamp>`
- `BETWEEN <start> AND <end>`
- `FROM <start> TO <end>`
- `ALL HISTORY`

## Write flow

1. Base-row mutation commits normally.
2. Temporal binder materializes history row publication according to binding
   policy.
3. Period start and end values are bound to commit-visible time.
4. History publication occurs in the same logical commit envelope.

## Query lowering

1. Parser identifies temporal clause.
2. Planner resolves temporal binding for the table.
3. Query is lowered into current table, history table, or both depending on the
   clause kind.
4. Result stitching preserves deterministic period ordering.

## Refusal rules

- `TEMPORAL_BINDING_MISSING`
- `TEMPORAL_HISTORY_SCHEMA_DRIFT`
- `TEMPORAL_RETENTION_EXPIRED`
- `TEMPORAL_CLAUSE_INVALID`

## Example

```sql
create temporal table accounts with history accounts_history;
select * from accounts for system time as of timestamp '2026-03-31 23:59:59';
```

## Cross-section requirements

- section `24` owns bindings and history catalogs
- section `21` owns temporal clause grammar
- section `39` owns archive replay interaction
