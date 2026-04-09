# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesTypeCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesDomainExtensionCatalogFamilyPages:CatalogTypeSchemaContractTest.*:CatalogDomainExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesTypeCatalogFamilyPages`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesDomainExtensionCatalogFamilyPages`: `PASS`
- `CatalogTypeSchemaContractTest.*`: `PASS`
- `CatalogDomainExtensionContractTest.*`: `PASS`
- Aggregate: `11 tests passed, 0 failed`
