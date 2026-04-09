# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesRuntimeContextCatalogFamilyPages:CatalogRuntimeContextExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesRuntimeContextCatalogFamilyPages`: `PASS`
- `CatalogRuntimeContextExtensionContractTest.ConnectionContracts`: `PASS`
- `CatalogRuntimeContextExtensionContractTest.TransactionContracts`: `PASS`
- Aggregate command result: `3 tests passed, 0 failed`
