# Test Results - HCN-012

Command:
```bash
cd build/tests
./scratchbird_tests --gtest_filter='StorageLockProviderTest.*:SecurityTest.ConcurrentAccess_TwoProcesses'
```

Result:
- 2 tests run
- 2 passed
- 0 failed

Log reference:
- `/tmp/hcn012_gtest.log`
