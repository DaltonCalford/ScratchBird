# Test Results

## Commands
1. `./build/tests/scratchbird_tests --gtest_filter='PageManagementEdgeTest.PageManager_FSMCorruption_*' --gtest_color=no`
2. `./build/tests/scratchbird_tests --gtest_filter='*Alpha101*:*PageManagement*:*MoreCases*:*ErrorPaths*:*OnDiskFormat*:*CRC32C_Comprehensive*:*BTreeRightmost*' --gtest_color=no`

## Result
- Corruption edge tests: `PASS` (`2/2`)
- Storage/page contract slice: `PASS` (`49/49`)
