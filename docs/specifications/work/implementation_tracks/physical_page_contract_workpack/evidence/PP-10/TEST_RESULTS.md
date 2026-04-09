# Test Results

## Commands
1. `cmake --build build --target scratchbird_tests -j6`
2. `ctest --test-dir build -R 'HeapRecordContractTest|HeapPageToastAPITest|MGABackVersioningTest' --output-on-failure`

## Result
- Build: `PASS`
- `HeapRecordContractTest`: `PASS` (`5/5`)
- Heap/TOAST/MGA regression subset: `PASS` (`17/17`)
