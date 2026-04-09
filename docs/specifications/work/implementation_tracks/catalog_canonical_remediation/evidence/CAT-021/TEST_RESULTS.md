# Test Results

Status: `PASS (CAT-021 complete)`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `cd build && ctest --output-on-failure -R 'CatalogDatabaseBootstrapTest.CreatesSecurityExtensionCatalogFamilyPages|CatalogDatabaseBootstrapTest.CreatesClusterNodeAndClockCatalogFamilyPages|CatalogSecurityExtensionContractTest.AuthMappingAndSecurityClassContracts|CatalogSecurityExtensionContractTest.PkiAndCryptoContracts|CatalogClusterClockExtensionContractTest.ClockCatalogContracts|CatalogClusterClockExtensionContractTest.NodeCatalogContracts'`

## Results
- Build: `PASS`
- `CatalogClusterClockExtensionContractTest.ClockCatalogContracts`: `PASS`
- `CatalogClusterClockExtensionContractTest.NodeCatalogContracts`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesSecurityExtensionCatalogFamilyPages`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesClusterNodeAndClockCatalogFamilyPages`: `PASS`
- `CatalogSecurityExtensionContractTest.AuthMappingAndSecurityClassContracts`: `PASS`
- `CatalogSecurityExtensionContractTest.PkiAndCryptoContracts`: `PASS`
- Aggregate: `6 passed, 0 failed`
