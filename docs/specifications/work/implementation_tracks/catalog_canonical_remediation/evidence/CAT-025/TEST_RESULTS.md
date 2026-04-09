# Test Results

Status: `PASS (CAT-025 complete)`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `cd build && ctest --output-on-failure -R 'CatalogSchedulerExtensionContractTest.SchedulerExtensionCatalogContracts|CatalogDatabaseBootstrapTest.CreatesSchedulerExtensionCatalogFamilyPages'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesSchedulerExtensionCatalogFamilyPages`: `PASS`
- `CatalogSchedulerExtensionContractTest.SchedulerExtensionCatalogContracts`: `PASS`
- Aggregate: `2 passed, 0 failed`
