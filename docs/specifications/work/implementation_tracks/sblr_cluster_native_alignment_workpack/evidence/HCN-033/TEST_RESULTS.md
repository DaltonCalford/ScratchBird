# Test Results - HCN-033

Command:
```bash
cd build/tests
./scratchbird_tests --gtest_filter='GcSafeHorizonCalculatorTest.*'
./scratchbird_tests --gtest_filter='SnapshotRegistryTest.*:CommittedWatermarkPublisherTest.*:FollowerApplyPipelineTest.*:ShardCommitLogPipelineTest.*'
```

Result:
- 8 tests run
- 8 passed
- 0 failed

Log references:
- `/tmp/hcn033_gtest.log`
- `/tmp/hcn030_032_recheck.log`
