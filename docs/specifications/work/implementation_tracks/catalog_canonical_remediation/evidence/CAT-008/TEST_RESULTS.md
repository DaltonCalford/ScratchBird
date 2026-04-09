# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j8`
2. `build/tests/scratchbird_tests --gtest_filter=CatalogParentageAndNameUniquenessTest.*`
3. `build/tests/scratchbird_tests --gtest_filter=CatalogRenameMoveTest.RenameTriggerUpdatesResolver`
4. `build/tests/scratchbird_tests --gtest_filter=CatalogDatabaseBootstrapTest.*`

## Results
- Build: `PASS`
- `CatalogParentageAndNameUniquenessTest.TriggerNameCollisionIsParentScoped`: `PASS`
- `CatalogParentageAndNameUniquenessTest.SameTriggerNameOnDifferentTablesIsAllowed`: `PASS`
- `CatalogParentageAndNameUniquenessTest.IndexNameCollisionIsParentScoped`: `PASS`
- `CatalogRenameMoveTest.RenameTriggerUpdatesResolver`: `PASS`
- `CatalogDatabaseBootstrapTest.PersistsDatabaseIdentityRow`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesCanonicalFixedSchemaTree`: `PASS`
