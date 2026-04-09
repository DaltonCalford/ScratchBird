# Cross-Shard Snapshot Test Results

Validated by:
- `SnapshotRegistryTest.TracksOldestSnapshotBoundaryPerShard`
- `CommittedWatermarkPublisherTest.PublishesMonotonicCwmAndSnapshotVector`

Observed:
- registry oldest-boundary updates are deterministic.
- snapshot vector captures per-shard CWM values with zero default for unknown shards.
