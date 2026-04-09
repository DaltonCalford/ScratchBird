# Test Results

## Commands
1. `cmake --build build --target scratchbird_tests -j6`
2. `ctest --test-dir build -R "IndexCorruptionErrorContractTest|IndexPageBaseLayoutContractTest|IndexPageTypeAndSiblingContractTest|ToastGCContractTest|HeapToastLobPageWalkerContractTest|LobPageLayoutContractTest|HeapRecordContractTest|ToastOperationsTest|HeapToastIntegrationTest|ToastTIPVisibilityTest|HeapPageToastAPITest|MGABackVersioningTest" --output-on-failure`

## Result
- Build: `PASS`
- Contract subset: `PASS` (`66/66`)
