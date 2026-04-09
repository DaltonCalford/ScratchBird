# Implementation Notes - HCN-031

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - added `FollowerApplyPipeline`, `FollowerApplyResult`, `FollowerApplyReason`.
- `src/core/cluster_write_safety.cpp`
  - implemented ordered follower apply against durable SCL records.
  - implemented idempotent replay handling for already applied entries.
  - implemented per-shard replication watermark progression.
- `tests/unit/test_follower_apply_pipeline.cpp`
  - validates ordered apply, replay idempotence, payload mismatch rejection, and missing-entry rejection.

Contract notes:
- RWM changes only when a new in-order entry is applied.
- out-of-order requests never advance RWM.
