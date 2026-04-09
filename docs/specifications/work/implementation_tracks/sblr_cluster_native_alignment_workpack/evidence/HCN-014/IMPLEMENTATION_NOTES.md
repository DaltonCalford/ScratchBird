# Implementation Notes - HCN-014

Primary implementation paths:
- `src/core/catalog_manager.cpp`
  - Node catalog + role/service surfaces.
  - Sharding metadata families (cluster/shard policy/key/scope/range/replica/migration/zone).
  - Routing/admission and replication runtime metadata families.
- `include/scratchbird/core/catalog_manager.h`
  - API contracts and typed catalog info payloads.
- Tests:
  - `tests/unit/test_catalog_sharding_extension_contract.cpp`
  - `tests/unit/test_catalog_routing_admission_extension_contract.cpp`
  - `tests/unit/test_catalog_replication_runtime_conflict_extension_contract.cpp`
