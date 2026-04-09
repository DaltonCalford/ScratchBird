# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesTypeCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesDomainExtensionCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesCharsetCollationExtensionCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesResourceTimezoneExtensionCatalogFamilyPages:CatalogDatabaseBootstrapTest.CreatesParserCapabilityExtensionCatalogFamilyPages:CatalogTypeSchemaContractTest.*:CatalogDomainExtensionContractTest.*:CatalogCharsetCollationExtensionContractTest.*:CatalogResourceTimezoneExtensionContractTest.*:CatalogParserCapabilityContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesParserCapabilityExtensionCatalogFamilyPages`: `PASS`
- `CatalogParserCapabilityContractTest.*`: `PASS`
- Aggregate command result: `21 tests passed, 0 failed`
