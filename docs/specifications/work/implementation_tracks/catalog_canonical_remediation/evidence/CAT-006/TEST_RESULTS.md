# Test Results

Status: `Pass`
Ticket: `CAT-006`

## Validation Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `build/tests/scratchbird_tests --gtest_filter=CatalogDatabaseBootstrapTest.PersistsDatabaseIdentityRow`

## Results
1. Build completed successfully for `scratchbird_tests`.
2. Bootstrap catalog contract test passed with `object_name` assertions enabled.

## Contract Assertions Covered
1. `object_name` catalog page is allocated and surfaced by catalog manager.
2. Canonical default-language database name row exists after bootstrap.
3. Reopen does not duplicate canonical default-language database name row.
