# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `cd build && ctest --output-on-failure -R 'CatalogRemoteConnectorExtensionContractTest|CatalogDatabaseBootstrapTest.CreatesRemoteConnectorExtensionCatalogFamilyPages'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesRemoteConnectorExtensionCatalogFamilyPages`: `PASS`
- `CatalogRemoteConnectorExtensionContractTest.RemoteConnectorExtensionCatalogContracts`: `PASS`
- Aggregate: `2 passed, 0 failed`
