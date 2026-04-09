# Code Area Ownership Map

## Row-store and write path

- `src/core/storage_engine.cpp`
  - `beginBulkInsert(`
  - `insertTupleWithHandle(`
  - `performPostInsertMaintenance(`
  - `captureUnchangedStableTidIndexEffects(`
- `include/scratchbird/core/storage_engine.h`
  - `BulkInsertHandle`
- `src/core/heap_page.cpp`
  - heap tuple insertion and update-locality substrate

## Exact families and B-tree behavior

- `src/core/btree.cpp`
  - `split_leaf_page(`
- `include/scratchbird/core/btree.h`
- `src/core/garbage_collector.cpp`
  - `publishIndexCleanupPublication(`

## Executor, bulk lanes, result cache, and runtime operators

- `src/sblr/executor.cpp`
  - `executeCanonicalV3(`
  - `QueryResultCacheManager::getInstance()`
  - `beginBulkInsert(` call sites
  - `bulk_load_plan` publication call sites
- `include/scratchbird/sblr/executor.h`

## Planner and cache identity

- `src/optimizer/query_planner.cpp`
  - `memory_grant_feedback`
- `src/optimizer/vnext_plan_cache.cpp`
  - `buildPlanCacheKey(`
  - `recordOptimizerEvent(`
- `src/sblr/query_compiler_v3_optimizer_support.cpp`
  - `buildPlanCacheKey(`

## Catalog and durable feedback

- `src/core/catalog_manager.cpp`
  - `bulk_load_plan`
  - `memory_grant_feedback`
- `include/scratchbird/core/catalog_manager.h`

## Benchmark and driver discipline

- `ScratchBird-Benchmarks/`
  - runner and artifact preservation logic
- `ScratchBird-driver/`
  - prepared execution and batch rewrite surfaces only when the benchmark audit
    proves driver behavior materially affects parity
