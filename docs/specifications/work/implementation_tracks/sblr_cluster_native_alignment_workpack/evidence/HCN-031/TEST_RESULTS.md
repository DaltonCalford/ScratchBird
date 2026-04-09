# Test Results - HCN-031

Command:
```bash
cd build/tests
./scratchbird_tests --gtest_filter='FollowerApplyPipelineTest.*'
./scratchbird_tests --gtest_filter='ShardCommitLogPipelineTest.*'
```

Result:
- 4 tests run
- 4 passed
- 0 failed

Log references:
- `/tmp/hcn031_gtest.log`
- `/tmp/hcn030_recheck.log`
