# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `build/tests/scratchbird_tests --gtest_filter='CatalogDatabaseBootstrapTest.CreatesStorageExtensionCatalogFamilyPages:CatalogStorageExtensionContractTest.*'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesStorageExtensionCatalogFamilyPages`: `PASS`
- `CatalogStorageExtensionContractTest.FilespaceStatsAndBackupHistoryContracts`: `PASS`
- `CatalogStorageExtensionContractTest.LobAndLobPageContracts`: `PASS`
- Aggregate command result: `3 tests passed, 0 failed`
