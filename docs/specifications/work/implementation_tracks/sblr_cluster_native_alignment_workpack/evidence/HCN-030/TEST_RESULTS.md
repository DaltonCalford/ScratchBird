# Test Results - HCN-030

Command:
```bash
cd build/tests
./scratchbird_tests --gtest_filter='ShardCommitLogPipelineTest.*'
./scratchbird_tests --gtest_filter='ClusterWriteFencingTest.*:DeterministicShardRouterTest.*:SessionEpochPinsTest.*:CatalogSessionEpochPinningTest.*:MultiShardWriteGuardTest.*:GtxidOrderingTest.*'
```

Result:
- 13 tests run
- 13 passed
- 0 failed

Log references:
- `/tmp/hcn030_gtest.log`
- `/tmp/hcn020_024_recheck.log`
