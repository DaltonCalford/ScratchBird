# Result Summary - HCN-032

Status: complete.

Implemented:
- Added `SnapshotRegistry` in `cluster_write_safety.{h,cpp}`.
  - register/update snapshot boundaries per `(shard_id, session_id)`.
  - remove snapshot registrations.
  - compute `oldestSnapshotBoundary(shard_id)`.
- Added `CommittedWatermarkPublisher` in `cluster_write_safety.{h,cpp}`.
  - monotonic per-shard CWM publication.
  - cross-shard snapshot vector capture.

Behavior validated:
- snapshot registry tracks and updates oldest active boundary correctly.
- per-shard CWM cannot move backward.
- snapshot vector returns deterministic CWM values for requested shard set.
