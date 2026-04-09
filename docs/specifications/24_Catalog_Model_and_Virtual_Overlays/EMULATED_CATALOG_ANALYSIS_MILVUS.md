# Emulated Catalog Analysis: Milvus 2.x

## Purpose
Identify Milvus catalog and metadata surfaces and how they map to ScratchBird canonical catalog data and runtime metrics.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data.
- `runtime`: derived from runtime state.
- `gated`: exposed only if feature is enabled.

## Mapping Table
| Milvus surface | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| listCollections / describeCollection | Collection metadata | `table` | canonical | Collections map to tables. |
| listPartitions / describePartition | Partition metadata | `partition` | canonical | Partition registry. |
| listAliases | Collection aliases | `object_name` | canonical | Alias mapping. |
| showCollections | Collection list | `table` | virtual | Derived view. |
| showIndexes / describeIndex | Index metadata | `index`, `index_option`, `index_stats` | canonical | Vector index metadata. |
| getCollectionStatistics | Collection stats | `table_stats` | runtime | Derived stats. |
| getPartitionStatistics | Partition stats | `partition_stats` | runtime | Derived stats. |
| getIndexBuildProgress | Index build state | `index_maintenance` | runtime | Maintenance state. |
| getIndexState | Index state | `index` | runtime | Index lifecycle state. |

## Notes
- Milvus metadata surfaces map directly to ScratchBird catalog tables and index metadata.
- Vector index types are modeled in canonical index metadata and exposed through Milvus APIs.

## Resolved Decisions
- Milvus RBAC metadata is required in Alpha and maps to canonical security tables:
  - users: `user`
  - roles: `role`
  - user-role bindings: `role_membership`
  - collection/partition/index grants: `permission` + `object_permission`
  - policy filters: `policy` when row/column constraints are needed for emulation parity.

## Open Questions
- None.
