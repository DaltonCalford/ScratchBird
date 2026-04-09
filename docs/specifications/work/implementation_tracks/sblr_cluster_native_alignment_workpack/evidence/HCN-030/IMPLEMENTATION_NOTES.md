# Implementation Notes - HCN-030

Code paths:
- `include/scratchbird/core/cluster_write_safety.h`
  - added `ShardCommitLog`, `ShardCommitLogEntry`, `ShardCommitLogAppendResult`.
  - added `ShardCommitLogAppendReason` enum.
- `src/core/cluster_write_safety.cpp`
  - implemented strict append sequencing per shard.
  - implemented durable append to `<root>/<shard_id>.scl` with flush+fsync semantics.
  - implemented on-disk read/decode path with corruption checks.
- `tests/unit/test_shard_commit_log_pipeline.cpp`
  - validates ordering enforcement and durable replay across instances.

Contract notes:
- ordering is deterministic and gap-free per shard.
- corrupted or malformed SCL lines fail read with data corruption status.
