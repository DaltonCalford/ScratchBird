# Implementation Notes - HCN-032

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - added `SnapshotRegistryEntry`, `SnapshotRegistry`.
  - added `CommittedWatermarkPublisher`.
- `src/core/cluster_write_safety.cpp`
  - implemented snapshot registry CRUD + oldest boundary computation.
  - implemented monotonic CWM publish and snapshot vector extraction.
- `tests/unit/test_snapshot_registry_cwm.cpp`
  - validates snapshot boundary and CWM behavior.

Contract notes:
- snapshot registry uses `(shard_id, session_id)` keying.
- CWM publication rejects backward movement with transaction-state error.
