# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages:CatalogIndexMetadataExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`: `PASS`
- `CatalogIndexMetadataExtensionContractTest.AccessMethodOpclassAndColumnContracts`: `PASS`
- `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`: `PASS`
- Aggregate command result: `3 tests passed, 0 failed`
