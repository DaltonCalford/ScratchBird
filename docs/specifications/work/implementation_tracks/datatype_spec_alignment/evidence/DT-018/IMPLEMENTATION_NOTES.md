# Implementation Notes

- Added `EmulatedStorageKind` and `EmulatedTypeMapping` public contracts in `include/scratchbird/core/types.h`.
- Added `TypeSystem::resolveEmulatedType(...)` with strict normalization for engine/type names:
  - engine aliases (`postgres` -> `POSTGRESQL`, `mongo` -> `MONGODB`)
  - type normalization (trim, uppercase, strip parameter/generic suffix)
  - type aliases (`character varying` -> `VARCHAR`, `character` -> `CHAR`)
- Added a deterministic static matrix in `src/core/type_system.cpp` spanning Firebird, PostgreSQL, MySQL, Cassandra, Milvus, MongoDB, Neo4j, Redis.
- Added unit coverage in `tests/unit/test_type_system.cpp` for core mappings, normalization behavior, and reject paths.
