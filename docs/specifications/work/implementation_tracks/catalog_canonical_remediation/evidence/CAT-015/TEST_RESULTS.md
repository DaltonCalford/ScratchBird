# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesRelationExtensionCatalogFamilyPages:CatalogRelationExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesRelationExtensionCatalogFamilyPages`: `PASS`
- `CatalogRelationExtensionContractTest.PartitionAndInheritanceContracts`: `PASS`
- `CatalogRelationExtensionContractTest.LanguageEventAndPackageMemberContracts`: `PASS`
- Aggregate command result: `3 tests passed, 0 failed`
