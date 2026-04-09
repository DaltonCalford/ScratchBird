# sys.cluster.metrics Results

Validated by `SqlObservabilityViewBuilderTest.BuildsClusterShardAndSnapshotRows`.

Observed:
- shard rows include leader term, lease, CWM/OST/RWM/GC-safe values, and lag metrics.
- snapshot rows include session/shard boundaries with deterministic ordering.
- output schema aligns to required `sys.cluster.metrics.shards` and `sys.cluster.metrics.snapshots` contracts.
