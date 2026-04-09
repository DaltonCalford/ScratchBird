# Test Results

Status: `PASS`

Executed:
```bash
./build/tests/scratchbird_tests --gtest_filter='CatalogTextSearchExtensionContractTest.*:CatalogDatabaseBootstrapTest.CreatesTextSearchCatalogFamilyPages:CatalogOlapCubeExtensionContractTest.OlapCubeCatalogContracts:CatalogDatabaseBootstrapTest.CreatesOlapCubeCatalogFamilyPages'
```

Observed:
- `CatalogTextSearchExtensionContractTest.TextSearchCatalogContracts` passed.
- `CatalogDatabaseBootstrapTest.CreatesTextSearchCatalogFamilyPages` passed.
- Regression anchors:
  - `CatalogOlapCubeExtensionContractTest.OlapCubeCatalogContracts` passed.
  - `CatalogDatabaseBootstrapTest.CreatesOlapCubeCatalogFamilyPages` passed.
