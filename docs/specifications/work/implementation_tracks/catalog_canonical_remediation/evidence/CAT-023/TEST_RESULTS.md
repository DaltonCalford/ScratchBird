# Test Results

Status: `PASS (CAT-023 complete)`

## Commands
1. `cmake --build build --target scratchbird_tests -j14`
2. `cd build && ctest --output-on-failure -R 'CatalogRoutingAdmissionExtensionContractTest.RoutingAndAdmissionCatalogContracts|CatalogRoutingAdmissionExtensionContractTest.SloAutoscaleAdmissionTuningCatalogContracts|CatalogDatabaseBootstrapTest.CreatesRoutingAdmissionCatalogFamilyPages|CatalogShardingExtensionContractTest.ShardingCatalogContracts|CatalogDatabaseBootstrapTest.CreatesShardingCatalogFamilyPages|CatalogClusterClockExtensionContractTest.ClockCatalogContracts|CatalogClusterClockExtensionContractTest.NodeCatalogContracts|CatalogDatabaseBootstrapTest.CreatesClusterNodeAndClockCatalogFamilyPages'`

## Results
- Build: `PASS`
- `CatalogRoutingAdmissionExtensionContractTest.RoutingAndAdmissionCatalogContracts`: `PASS`
- `CatalogRoutingAdmissionExtensionContractTest.SloAutoscaleAdmissionTuningCatalogContracts`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesRoutingAdmissionCatalogFamilyPages`: `PASS`
- `CatalogShardingExtensionContractTest.ShardingCatalogContracts`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesShardingCatalogFamilyPages`: `PASS`
- `CatalogClusterClockExtensionContractTest.ClockCatalogContracts`: `PASS`
- `CatalogClusterClockExtensionContractTest.NodeCatalogContracts`: `PASS`
- `CatalogDatabaseBootstrapTest.CreatesClusterNodeAndClockCatalogFamilyPages`: `PASS`
- Aggregate: `8 passed, 0 failed`
