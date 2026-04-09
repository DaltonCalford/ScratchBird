# Dependencies - 18_Index_Framework

## Upstream Dependencies
- 05_Page_Taxonomy_and_Binary_Layouts
- 08_Transaction_Core
- 09_Lock_Manager_Core
- 10_GC_and_Sweep
- 13_Operator_Model_and_Coercion
- 14_Base_Scalar_Types

## Downstream Dependents
- 21_V3_Dialect_Surface
- 24_Catalog_Model_and_Virtual_Overlays
- 20_Diagnostics_Audit_and_Observability
- 31_Conformance_Performance_and_Reliability_Gates

## External References
- Historical archive: `specifications_old/indexes/BTREE_SPEC.md`
- Historical archive: `specifications_old/indexes/INDEX_GC_PROTOCOL.md`
- Historical archive: `specifications_old/indexes/INDEX_ARCHITECTURE.md`

## Update 2026-03-28: current code-backed authority files

Primary section `18` code authority in this pass:
- `include/scratchbird/core/catalog_manager.h`
- `src/core/catalog_manager.cpp`
- `src/core/index_factory.cpp`
- `src/sblr/executor.cpp`
- `src/core/garbage_collector.cpp`
- `src/core/btree.cpp`
- `src/core/hash_index.cpp`
- `src/core/rtree.cpp`
- `src/core/columnstore.cpp`

Direct follow-on family proof targets, not closed in this pass:
- `gin_index.*`
- `gist_index.*`
- `spgist_index.*`
- `brin_index.*`
- `hnsw_index.*`
- `inverted_index.*`
- `lsm_tree_index.*`
