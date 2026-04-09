# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `ctest --output-on-failure -R 'CatalogDatabaseBootstrapTest.CreatesSecurityExtensionCatalogFamilyPages|CatalogSecurityExtensionContractTest.AuthMappingAndSecurityClassContracts|CatalogSecurityExtensionContractTest.PkiAndCryptoContracts'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesSecurityExtensionCatalogFamilyPages`: `PASS`
- `CatalogSecurityExtensionContractTest.AuthMappingAndSecurityClassContracts`: `PASS`
- `CatalogSecurityExtensionContractTest.PkiAndCryptoContracts`: `PASS`
- Aggregate command result: `3 tests passed, 0 failed`
