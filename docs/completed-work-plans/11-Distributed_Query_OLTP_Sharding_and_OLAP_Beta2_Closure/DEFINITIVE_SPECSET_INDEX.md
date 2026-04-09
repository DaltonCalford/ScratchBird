# Definitive Specset Index

This file freezes the authoritative current boundaries and planned closure
targets for the four Beta 2 topic families in scope.

## Gap 1: Cross-machine query execution

- Current boundaries:
  - `docs/specifications/25_Runtime_Modes/CLUSTER_ROUTING_AND_ADMISSION.md`
    + `full distributed route selection algorithms`
  - `docs/specifications/36_Query_Rewrite_and_Planner/PLANNER_UNIFICATION_PHYSICAL_PROPERTY_AND_ACCESS_TRUST_BETA2_MODEL.md`
    + `exchange and gather posture`
- Planned canonical targets:
  - `docs/specifications/36_Query_Rewrite_and_Planner/BETA2_CROSS_MACHINE_QUERY_DECOMPOSITION_DATA_MOTION_AND_RESULT_STITCHING_MODEL.md`
  - `docs/specifications/25_Runtime_Modes/BETA2_CLUSTER_REMOTE_FRAGMENT_EXECUTION_EXCHANGE_AND_ADMISSION_MODEL.md`
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/BETA2_DISTRIBUTED_QUERY_FRAGMENT_LOCATION_COST_AND_EXCHANGE_CATALOG_MODEL.md`

## Gap 2: High-performance OLTP support

- Current boundaries:
  - `docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md`
    + `commit-group batch apply`
  - `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md`
    + `Current source evidence does not prove a full cluster OLTP node lifecycle`
- Planned canonical targets:
  - `docs/specifications/25_Runtime_Modes/BETA2_HIGH_PERFORMANCE_OLTP_SERVICE_CLASS_AND_NODE_SPECIALIZATION_MODEL.md`
  - `docs/specifications/36_Query_Rewrite_and_Planner/BETA2_HIGH_PERFORMANCE_OLTP_PLAN_SHAPES_CONTENTION_AVOIDANCE_AND_PREPARED_EXECUTION_MODEL.md`
  - `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/BETA2_HIGH_PERFORMANCE_OLTP_BENCHMARK_AND_CONTENTION_GATE_MODEL.md`

## Gap 3: Shard support

- Current boundaries:
  - `docs/specifications/25_Runtime_Modes/SHARD_ROUTING_MULTI_SHARD_GUARD_AND_COMMIT_LOG_MODEL.md`
    + `deterministic shard routing`
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SHARDING_CATALOG_SCHEMA.md`
    + `Define canonical catalog tables required to support sharding, cluster membership, and shard placement`
- Planned canonical targets:
  - `docs/specifications/25_Runtime_Modes/BETA2_SHARD_PLACEMENT_REBALANCE_SPLIT_MERGE_AND_READ_ROUTING_MODEL.md`
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/BETA2_SHARD_POLICY_RANGE_PLACEMENT_AND_MIGRATION_CATALOG_MODEL.md`
  - `docs/specifications/42_Failure_Model_and_Fault_Tolerance/BETA2_SHARD_FAILURE_REBALANCE_AND_OWNERSHIP_RECOVERY_CLASSIFICATION_MODEL.md`

## Gap 4: OLAP support and cubes

- Current boundaries:
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/OLAP_CUBE_CATALOG_SCHEMA.md`
    + `Define canonical catalog tables required for OLAP processing nodes and cube support`
  - `docs/specifications/18_Index_Framework/COLUMNSTORE_SPEC.md`
    + `scan acceleration, aggregation support, and summary pruning`
- Planned canonical targets:
  - `docs/specifications/18_Index_Framework/BETA2_OLAP_STORAGE_SEGMENT_PRUNING_VECTOR_SCAN_AND_CUBE_ACCELERATION_MODEL.md`
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/BETA2_OLAP_CUBE_MATERIALIZATION_REFRESH_AND_JOB_CATALOG_MODEL.md`
  - `docs/specifications/36_Query_Rewrite_and_Planner/BETA2_OLAP_QUERY_REWRITE_ROLLUP_CUBE_MATCHING_AND_HTAP_SEPARATION_MODEL.md`
  - `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/BETA2_OLAP_CUBE_REFRESH_AND_ANALYTICAL_BENCHMARK_GATE_MODEL.md`

## Primary reference roots

- `docs/reference/workspace_library/technical_specs/`
- `docs/reference/workspace_library/whitepapers/`
- `docs/reference/workspace_library/third_party_implementations/`
- `docs/reference/reference_library/`

## Implementation-grade research rule

Every closure lane must produce:

- process flows
- state models
- refusal rules
- algorithms or selection logic
- data-structure and metadata requirements
- distributed-execution, benchmark, or refresh behaviors where applicable
- sample pseudocode or implementation skeletons where complexity warrants it
