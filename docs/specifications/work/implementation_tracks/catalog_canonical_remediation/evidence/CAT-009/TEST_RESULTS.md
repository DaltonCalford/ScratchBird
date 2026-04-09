# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `cmake --build build --target scratchbird_tests -j1`
3. `build/tests/scratchbird_tests --gtest_filter='HomeSchemaResolutionTest.*:CatalogParentageAndNameUniquenessTest.*:CatalogDatabaseBootstrapTest.*'`

## Results
- Build pass after rerun (`-j1`) following transient `text file is busy` during gtest discovery.
- `CatalogDatabaseBootstrapTest.*`: `PASS`
- `CatalogParentageAndNameUniquenessTest.*`: `PASS`
- `HomeSchemaResolutionTest.*`: `PASS`
- Aggregate: `10 tests passed, 0 failed`
