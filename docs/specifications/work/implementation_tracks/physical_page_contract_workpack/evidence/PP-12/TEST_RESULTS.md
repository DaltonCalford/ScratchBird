# Test Results

## Commands
1. `cmake --build build --target scratchbird_tests -j6`
2. `ctest --test-dir build -R 'LobPageLayoutContractTest|HeapRecordContractTest|ToastOperationsTest|HeapToastIntegrationTest|ToastTIPVisibilityTest|HeapPageToastAPITest|MGABackVersioningTest' --output-on-failure`

## Result
- Build: `PASS`
- Page/heap/TOAST/LOB contract subset: `PASS` (`44/44`)
