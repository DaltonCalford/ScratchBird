# Replication Replay Attack Results - HCN-060

Validated tests:
- `FollowerApplyPipelineTest.InOrderApplyUpdatesReplicationWatermark`
- `FollowerApplyPipelineTest.ReplayIsIdempotentAndOrderingIsEnforced`
- `ShardCommitLogPipelineTest.AppendIsStrictlyOrderedPerShard`
- `GtxidOrderingTest.FollowerApplyRejectsGapsAndDuplicates`

Outcome:
- Replay and duplicate apply attempts are rejected or rendered idempotent.
- Ordering gaps are detected and blocked.
- Replication watermark moves monotonically under in-order apply.
