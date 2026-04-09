# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| B1-04-001 | assigned section specs plus this package | all package control files | serial only |
| B1-04-002 | primary canonical targets for sections `18,33,34,36,38` plus package control files | all package control files and primary section README targets | serial only |
| B1-04-003 | include/scratchbird/core/catalog_manager.h, src/core/catalog_manager.cpp, include/scratchbird/core/index_factory.h, src/core/index_factory.cpp, src/core/storage_engine.cpp, include/scratchbird/core/buffer_pool.h, src/core/buffer_pool.cpp, src/core/hnsw_index.cpp, src/core/columnstore.cpp, src/core/lsm_tree_index.cpp | catalog, index-factory, storage, and buffer-residency seams | after ownership freeze |
| B1-04-004 | include/scratchbird/optimizer/statistics.h, src/optimizer/statistics_manager.cpp, src/optimizer/index_family_lowering.cpp, src/optimizer/query_planner.cpp, include/scratchbird/core/workload_governance.h, src/core/workload_governance.cpp, src/core/job_scheduler.cpp | planner, metrics, and workload-governance overlap with lane A storage surfaces | after lane A foundation |
| B1-04-005 | optimizer and memory gates and benchmarks | shared benchmark runners | after implementation tickets |

## Unsafe Parallel Boundaries

- any ticket that updates SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- any ticket that changes the same canonical spec file as another ticket
- any ticket that changes the same gate or benchmark artifact family
- any ticket that changes `src/core/catalog_manager.cpp`,
  `src/core/storage_engine.cpp`, `src/core/buffer_pool.cpp`,
  `src/optimizer/statistics_manager.cpp`, `src/optimizer/query_planner.cpp`,
  or `src/core/workload_governance.cpp`
