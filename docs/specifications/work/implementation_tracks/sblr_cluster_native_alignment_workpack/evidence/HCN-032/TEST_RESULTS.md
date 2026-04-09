# Test Results - HCN-032

Command:
```bash
cd build/tests
./scratchbird_tests --gtest_filter='SnapshotRegistryTest.*:CommittedWatermarkPublisherTest.*'
./scratchbird_tests --gtest_filter='FollowerApplyPipelineTest.*:ShardCommitLogPipelineTest.*'
```

Result:
- 6 tests run
- 6 passed
- 0 failed

Log references:
- `/tmp/hcn032_gtest.log`
- `/tmp/hcn030_031_recheck.log`
