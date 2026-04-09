# Implementation Notes - HCN-020

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - introduced `WriteAdmissionReason`, `FencingToken`, `ShardLeaderState`, `WriteAdmissionRequest/Result`.
  - introduced `ClusterWriteSafetyController`.
- `src/core/cluster_write_safety.cpp`
  - implemented leader state upsert and write admission validation.
  - reasoned failures: `SHARD_NOT_REGISTERED`, `SHARD_WRITES_DISABLED`, `NOT_CURRENT_LEADER`, `FENCING_SHARD_MISMATCH`, `STALE_FENCING_TOKEN`, `ROUTING_EPOCH_MISMATCH`.

Safety properties:
- Deterministic rejection ordering prevents ambiguous behavior.
- No fallback path bypasses leader-term fencing checks.
