# Test Results

## Commands
1. `cmake --build build --target scratchbird_tests -j6`
2. `ctest --test-dir build -R 'HeapRecordContractTest' --output-on-failure`
3. `ctest --test-dir build -R 'HeapRecordContractTest|HeapPageToastAPITest|MGABackVersioningTest' --output-on-failure`

## Result
- Build: `PASS`
- `HeapRecordContractTest`: `PASS` (`3/3`)
- Heap/MGA regression subset: `PASS` (`15/15`)
