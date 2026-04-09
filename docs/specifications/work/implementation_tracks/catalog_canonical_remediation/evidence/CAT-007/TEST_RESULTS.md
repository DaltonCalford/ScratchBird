# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `build/tests/scratchbird_tests --gtest_filter=CatalogDatabaseBootstrapTest.*`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.PersistsDatabaseIdentityRow`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesCanonicalFixedSchemaTree`: `PASS`
