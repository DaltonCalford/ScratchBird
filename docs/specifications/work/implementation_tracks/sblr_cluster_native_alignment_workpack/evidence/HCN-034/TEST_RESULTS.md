# Test Results - HCN-034

Commands:
```bash
cmake --build build --target scratchbird_tests -j8
build/tests/scratchbird_tests --gtest_filter='ShardCommitLogPipelineTest.*:FollowerApplyPipelineTest.*:SnapshotRegistryTest.*:CommittedWatermarkPublisherTest.*:GcSafeHorizonCalculatorTest.*:DomainControlPlaneReplicaCatalogTest.*'
build/tests/scratchbird_tests --gtest_filter='ClusterWriteFencingTest.*:DeterministicShardRouterTest.*:SessionEpochPinsTest.*:CatalogSessionEpochPinningTest.*:MultiShardWriteGuardTest.*:GtxidOrderingTest.*'
```

Results:
- PH3 cluster safety subset: 11 passed, 0 failed.
- PH2 regression subset recheck: 11 passed, 0 failed.

Log references:
- `/tmp/hcn030_034_gtest.log`
- `/tmp/hcn020_024_hcn034_recheck.log`
