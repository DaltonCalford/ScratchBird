# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesTypeCatalogFamilyPages:CatalogTypeSchemaContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesTypeCatalogFamilyPages`: `PASS`
- `CatalogTypeSchemaContractTest.*`: `PASS`
- Aggregate: `7 tests passed, 0 failed`
