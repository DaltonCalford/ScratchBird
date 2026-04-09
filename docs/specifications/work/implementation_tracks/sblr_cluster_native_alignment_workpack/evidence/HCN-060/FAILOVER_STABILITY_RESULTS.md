# Failover Stability Results - HCN-060

Validated tests:
- `DomainControlPlaneReplicaCatalogTest.*`
- `SnapshotRegistryTest.TracksOldestSnapshotBoundaryPerShard`
- `CommittedWatermarkPublisherTest.PublishesMonotonicCwmAndSnapshotVector`
- `GcSafeHorizonCalculatorTest.*`

Outcome:
- Domain control-plane replication/join manifests remain consistent with mismatch detection.
- Snapshot and CWM progression remains monotonic.
- GC-safe horizon remains bounded by `min(OST, RWM)` safety contract.
