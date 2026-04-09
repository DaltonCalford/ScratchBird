# B1-04-003 Evidence Note

## Closure summary

Index family parity, table-access integration, and lane-A memory-adjacent catalog
closure for package `04` is complete.

This ticket:
- promoted canonical per-index family identity into durable runtime-backed
  catalog truth without widening the fixed `IndexRecord` layout
- materialized canonical family metadata through the existing
  `index_params_oid` TOAST carrier and synchronized it into
  `CatalogManager::IndexInfo`
- added create/open validation against the admitted `IndexFactory` named-family
  registry
- enabled the remaining ANN-family create paths needed by this lane
- fixed V3 type payload emission so `VECTOR(n)` column dimensions survive into
  catalog column precision and ANN-family admission
- persisted V3 `CREATE INDEX ... WITH (...)` family options and preserved legacy
  bare index-param keys during updates

## Primary implementation anchors

- `syncIndexParamsBlob(`
- `CatalogManager::updateIndexParams(`
- `IndexFactory::populateCanonicalMetadata(`
- `IndexFactory::validateCanonicalMetadata(`
- `StorageEngine::createIndexScan(`
- `typeSpecToColumnType`
- `V3Emitter::buildTypeSpec`

## Files changed

- `include/scratchbird/core/catalog_manager.h`
- `include/scratchbird/core/index_factory.h`
- `include/scratchbird/core/index_params.h`
- `src/core/catalog_manager.cpp`
- `src/core/index_factory.cpp`
- `src/core/index_params.cpp`
- `src/parser/v3_emitter.cpp`
- `src/sblr/ast_sblr_lowerer.cpp`
- `src/sblr/executor.cpp`
- `tests/unit/test_index_executor_dispatch_contracts.cpp`
- package tracker and audit files under `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/`

## Verification

Focused contract proof was rerun on March 30, 2026:

- command:
  `./build/tests/scratchbird_tests --gtest_filter='IndexExecutorDispatchContractsTest.V3CreateIndexRoutesIvfSq8HybridThroughVectorFamilyPath:IndexExecutorDispatchContractsTest.V3CreateIndexAdmitsAnnoyFamilyAndPersistsCanonicalMetadata:IndexExecutorDispatchContractsTest.V3CreateIndexAdmitsScannFamily:IndexExecutorDispatchContractsTest.V3CreateIndexAdmitsDiskannFamily:IndexExecutorDispatchContractsTest.V3CreateIndexAdmitsGpuCagraFamily'`
- artifact:
  `evidence/B1-04-003/index_family_contracts.log`
- result:
  5 tests passed

## Result

- `B1-04-003` is closed
- `B1-04-004` is now the active execution point for package `04`
