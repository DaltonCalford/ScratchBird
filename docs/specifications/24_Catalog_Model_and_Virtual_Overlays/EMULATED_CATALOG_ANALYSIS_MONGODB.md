# Emulated Catalog Analysis: MongoDB 8.x

## Purpose
Identify MongoDB catalog and metadata surfaces and how they map to ScratchBird canonical catalog data and runtime metrics.

## Classification
- `canonical`: requires persisted ScratchBird catalog data.
- `virtual`: derived from canonical data.
- `runtime`: derived from runtime state.
- `gated`: exposed only if feature is enabled.

## Mapping Table
| MongoDB surface | Purpose | SB source | Storage class | Notes |
| --- | --- | --- | --- | --- |
| listDatabases | Database list | `database` | virtual | Derived view. |
| listCollections | Collection metadata | `table` + `object_name` | virtual | Collections map to tables. |
| listIndexes | Index metadata | `index`, `index_column`, `index_stats` | virtual | Index overlay. |
| system.views | View definitions | `view` | canonical | View registry. |
| system.js | Stored JS functions | `function` | canonical | Only if stored JS is enabled. |
| system.profile | Profiling data | `audit_log` / metrics | runtime | Runtime profiling view. |
| admin/system users | User/auth data | `user`, `role`, `permission` | canonical | Auth/roles mapping. |
| config.* (sharding) | Cluster metadata | cluster catalog | gated | Only if sharding/cluster emulation enabled. |

## Notes
- MongoDB exposes catalogs primarily via commands; ScratchBird provides these as **virtual overlays** over canonical catalogs.
- `system.views` and `system.js` represent actual stored objects and must map to canonical metadata if enabled.

## Resolved Decisions
- `system.js` support is enabled in Alpha and maps to canonical routine metadata in `function` (language=`javascript`).
- MongoDB sharding metadata requirements in Alpha map to:
  - `cluster`, `node`
  - `shard`, `shard_range`, `shard_replica`, `shard_migration`
  - runtime `sys.shard_status` for routing visibility.

## Open Questions
- None.
