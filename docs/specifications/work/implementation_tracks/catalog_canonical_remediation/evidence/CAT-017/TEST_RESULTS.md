# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesIndexTelemetryExtensionCatalogFamilyPages:CatalogIndexMetadataExtensionContractTest.*:CatalogIndexMetricsExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesIndexMetadataExtensionCatalogFamilyPages`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesIndexTelemetryExtensionCatalogFamilyPages`: `PASS`
- `CatalogIndexMetadataExtensionContractTest.AccessMethodOpclassAndColumnContracts`: `PASS`
- `CatalogIndexMetadataExtensionContractTest.OptionAndMaintenanceDeltaContracts`: `PASS`
- `CatalogIndexMetricsExtensionContractTest.StatsUsageAndContentionContracts`: `PASS`
- `CatalogIndexMetricsExtensionContractTest.StorageAndHealthContracts`: `PASS`
- Aggregate command result: `6 tests passed, 0 failed`
