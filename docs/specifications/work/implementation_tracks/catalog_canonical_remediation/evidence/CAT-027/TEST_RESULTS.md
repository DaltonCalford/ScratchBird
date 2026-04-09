# Test Results

Status: `PASS`

## Commands
1. `cmake --build build --target scratchbird_tests -j4`
2. `cd build && ctest --output-on-failure -R 'CatalogReplicationRuntimeConflictExtensionContractTest|CatalogDatabaseBootstrapTest.CreatesReplicationRuntimeConflictCatalogFamilyPages'`

## Results
- Build: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesReplicationRuntimeConflictCatalogFamilyPages`: `PASS`
- `CatalogReplicationRuntimeConflictExtensionContractTest.ReplicationRuntimeConflictCatalogContracts`: `PASS`
- Aggregate: `2 passed, 0 failed`
