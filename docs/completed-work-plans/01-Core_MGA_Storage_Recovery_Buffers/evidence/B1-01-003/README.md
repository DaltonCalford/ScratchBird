# B1-01-003 Evidence Note

## Closure summary

Lane A implementation closure for this package is complete.

This closure pass:
- corrected the live `BufferPool::allocatePageGlobal` API note so it matches the
  implemented GPID-based custom-tablespace path
- added direct runtime proof that `StorageEngine::insertTuple` publishes heap
  tuples into the owning custom tablespace
- added direct runtime proof that `BufferPool::allocatePageGlobal` allocates and
  republishes valid heap pages inside a custom tablespace
- advanced the lane-A implementation rows in
  `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv` to `implemented`

## Code and proof anchors

- `include/scratchbird/core/buffer_pool.h`
- `tests/unit/test_storage_engine.cpp`
- `src/core/page_manager.cpp` search `PageManager::allocatePageInTablespace`
- `src/core/buffer_pool.cpp` search `BufferPool::publishDirtyGeneration`
- `src/core/catalog_manager.cpp` search `CatalogManager::resolveTablespaceBindings`
- `src/core/tid_resolver.cpp` search `TIDResolver::resolveTablespace`
- `src/core/database.cpp` search `Database::validate_bootstrap_page_map`
- `include/scratchbird/core/ondisk.h` search `PageHeader must publish the canonical section-05 field set`

## Verification

Focused rebuild:
- `cmake --build /home/dcalford/CliWork/ScratchBird/build --target scratchbird_tests -j4`

Focused test proof:
- `./build/tests/scratchbird_tests --gtest_filter='StorageEngineTest.InsertTuplePublishesHeapPageInCustomTablespace:StorageEngineTest.BufferPoolAllocatePageGlobalSupportsCustomTablespace'`

Result:
- both targeted tests passed on March 30, 2026

## Residual non-blockers

- section `06` still needs the dedicated bootstrap corruption-matrix gate called
  out earlier in this package; that remains B1-01-005 work, not a blocker on
  lane-A implementation closure
- this ticket did not run the broader gate or benchmark suite
