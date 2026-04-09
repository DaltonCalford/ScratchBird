# Implementation Notes - HCN-023

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - introduced `MultiShardGuardPolicy`, `MultiShardGuardResult`, `MultiShardGuardReason`.
- `src/core/cluster_write_safety.cpp`
  - implemented write shard cardinality analysis and policy/override resolution.

Safety properties:
- multi-shard writes require intentional operator/app-layer action when configured.
- single-shard writes remain unaffected.
