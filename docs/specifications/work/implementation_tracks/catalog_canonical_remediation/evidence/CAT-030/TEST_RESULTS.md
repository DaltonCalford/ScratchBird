# Test Results

Status: `PASS`

Executed:
```bash
./build/tests/scratchbird_tests --gtest_filter='CatalogTextSearchExtensionContractTest.*:CatalogDatabaseBootstrapTest.CreatesTextSearchCatalogFamilyPages:CatalogOlapCubeExtensionContractTest.OlapCubeCatalogContracts:CatalogDatabaseBootstrapTest.CreatesOlapCubeCatalogFamilyPages'
```

Observed:
- `CatalogOlapCubeExtensionContractTest.OlapCubeCatalogContracts` passed.
- `CatalogDatabaseBootstrapTest.CreatesOlapCubeCatalogFamilyPages` passed.
- Regression anchors:
  - `CatalogTextSearchExtensionContractTest.TextSearchCatalogContracts` passed.
  - `CatalogDatabaseBootstrapTest.CreatesTextSearchCatalogFamilyPages` passed.
