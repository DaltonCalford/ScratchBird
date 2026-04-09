# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesTypeCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesDomainExtensionCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesCharsetCollationExtensionCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesResourceTimezoneExtensionCatalogFamilyPages:CatalogTypeSchemaContractTest.*:CatalogDomainExtensionContractTest.*:CatalogCharsetCollationExtensionContractTest.*:CatalogResourceTimezoneExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesCharsetCollationExtensionCatalogFamilyPages`: `PASS`
- `CatalogCharsetCollationExtensionContractTest.*`: `PASS`
- Aggregate command result: `18 tests passed, 0 failed`
