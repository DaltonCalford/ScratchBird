# Implementation Notes - HCN-013

Primary implementation paths:
- `src/core/catalog_manager.cpp`
  - Cluster clock catalog CRUD and constraints.
  - Cluster fabric catalog CRUD and constraints.
- `include/scratchbird/core/catalog_manager.h`
  - Control-plane catalog info structures and APIs.
- `tests/unit/test_catalog_cluster_clock_extension_contract.cpp`
- `tests/unit/test_catalog_cluster_fabric_extension_contract.cpp`

Operational behavior:
- Versioned/update-guarded entries (e.g., fabric mode version checks).
- Event/error/task/session/txn records provide deterministic replay/audit lineage in catalog state.
