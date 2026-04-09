# Test Results - HCN-060

Command:
```bash
build/tests/scratchbird_tests --gtest_filter='ClusterWriteFencingTest.*:DeterministicShardRouterTest.*:SessionEpochPinsTest.*:MultiShardWriteGuardTest.*:GtxidOrderingTest.*:ShardCommitLogPipelineTest.*:FollowerApplyPipelineTest.*:SnapshotRegistryTest.*:CommittedWatermarkPublisherTest.*:GcSafeHorizonCalculatorTest.*:DomainControlPlaneReplicaCatalogTest.*:StructuredEventStreamTest.*:HealthReadinessContractTest.*'
```

Results:
- 26 tests ran.
- 26 passed, 0 failed.

Log reference:
- `/tmp/hcn060_tests.log`
