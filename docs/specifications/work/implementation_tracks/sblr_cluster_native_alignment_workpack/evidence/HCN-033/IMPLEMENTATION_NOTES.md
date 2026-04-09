# Implementation Notes - HCN-033

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - added `GcSafeHorizonEvaluation` and `GcSafeHorizonCalculator`.
- `src/core/cluster_write_safety.cpp`
  - implemented evaluation and reclaimability checks.
  - integrated with `SnapshotRegistry` (OST source) and `FollowerApplyPipeline` (RWM source).
- `tests/unit/test_gc_safe_horizon.cpp`
  - validates formula behavior and reclaimability gating.

Contract notes:
- conservative default: if OST or RWM is absent, safe horizon remains 0.
- reclaim check uses strict inequality (creator_local_txn_id < gc_safe_horizon).
