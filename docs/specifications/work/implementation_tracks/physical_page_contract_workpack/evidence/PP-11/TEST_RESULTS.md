# Test Results

## Commands
1. `cmake --build build --target scratchbird_tests -j6`
2. `ctest --test-dir build -R 'HeapRecordContractTest|ToastOperationsTest|HeapToastIntegrationTest|ToastTIPVisibilityTest|HeapPageToastAPITest|MGABackVersioningTest' --output-on-failure`

## Result
- Build: `PASS`
- Contract + TOAST/MGA regression subset: `PASS` (`41/41`)
