# Test Results

## Commands
1. `cmake --build build -j8 --target scratchbird_tests`
2. `./build/tests/scratchbird_tests --gtest_filter='*Alpha101*:*PageManagement*:*MoreCases*:*ErrorPaths*:*OnDiskFormat*:*CRC32C_Comprehensive*:*BTreeRightmost*' --gtest_color=no`

## Result
- Build: `PASS`
- Test slice: `PASS` (`49` tests run, `49` passed, `0` failed)
