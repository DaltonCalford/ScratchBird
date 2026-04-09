# Emulated Catalog Analysis: Cassandra 5.x

## Purpose
Identify Cassandra system keyspace tables and how they map to ScratchBird canonical catalog data and runtime metrics.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data.
- `runtime`: derived from runtime state.
- `gated`: exposed only if feature is enabled.

## Mapping Table (system_schema)
| Cassandra system_schema | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| system_schema.keyspaces | Keyspace definitions | `schema` | canonical | Keyspace == schema. |
| system_schema.tables | Table definitions | `table` | canonical | Table metadata. |
| system_schema.columns | Column definitions | `column` | canonical | Column metadata. |
| system_schema.types | UDT definitions | `type`, `domain` | canonical | UDT/struct types. |
| system_schema.functions | UDF registry | `function` | canonical | Function metadata. |
| system_schema.aggregates | Aggregate registry | `function` (aggregate flag) | canonical | Aggregate metadata. |
| system_schema.indexes | Index registry | `index` | canonical | Index metadata. |
| system_schema.views | Materialized views | `view` | canonical | Materialized view metadata. |
| system_schema.triggers | Triggers | `trigger` | canonical | Trigger metadata. |
| system_schema.dropped_columns | Dropped columns | `column_drop_history` | canonical | Column drop history. |

## Mapping Table (system keyspace)
| Cassandra system | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| system.local | Local node metadata | runtime metrics | runtime | Node identity/state. |
| system.peers / peers_v2 | Cluster peers | cluster metadata | runtime | Cluster node list. |
| system.size_estimates | Token range sizes | `table_stats` | runtime | Derived stats. |
| system.sstable_activity | SSTable activity | storage metrics | runtime | Derived from storage metrics. |
| system.prepared_statements | Prepared statements | `sys.prepared_statement` | runtime | Prepared statement cache. |
| system.available_ranges | Range availability | runtime metrics | runtime | Range availability state. |

## Notes
- Cassandra’s `system_schema` tables require canonical catalog data; they are exposed as **virtual overlays** over ScratchBird catalogs.
- Cassandra’s `system` tables are runtime/state views and can be virtualized from ScratchBird runtime metrics without dedicated storage.

## Resolved Decisions
- Required `system_schema` coverage for Alpha includes:
  - `keyspaces`, `tables`, `columns`, `types`, `functions`, `aggregates`, `indexes`, `views`, `triggers`, `dropped_columns`.
- Partitioner/token metadata in cluster mode maps to:
  - `shard_range` (token ranges)
  - `shard` (logical ownership)
  - `node` and `shard_replica` (replica placement)
  - runtime `sys.shard_status` for liveness and serving state.

## Open Questions
- None.
