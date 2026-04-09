# Canonical Gap Register

## DQO-11-G01 Cross-machine query execution

- Status: open
- Current bounded proof:
  - `docs/specifications/25_Runtime_Modes/CLUSTER_ROUTING_AND_ADMISSION.md`
    + `full distributed route selection algorithms`
  - `docs/specifications/36_Query_Rewrite_and_Planner/PLANNER_UNIFICATION_PHYSICAL_PROPERTY_AND_ACCESS_TRUST_BETA2_MODEL.md`
    + `exchange and gather posture`
- Gap:
  - no complete Beta 2 canon for query decomposition, remote fragment
    execution, data motion, remote cost, or result stitching across cluster
    members
- Closing tickets:
  - `DQO-11-002`
  - `DQO-11-003`

## DQO-11-G02 High-performance OLTP support

- Status: partial_substrate
- Current bounded proof:
  - `docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md`
    + `commit-group batch apply`
  - `docs/specifications/25_Runtime_Modes/CLUSTER_OLTP_NODE_LIFECYCLE.md`
    + `Current source evidence does not prove a full cluster OLTP node lifecycle`
- Gap:
  - no unified Beta 2 commercial-grade OLTP canon covering service classes,
    hot-key and hot-partition contention, prepared execution posture, node
    specialization, and benchmark gates
- Closing tickets:
  - `DQO-11-004`
  - `DQO-11-005`

## DQO-11-G03 Shard support

- Status: partial_substrate
- Current bounded proof:
  - `docs/specifications/25_Runtime_Modes/SHARD_ROUTING_MULTI_SHARD_GUARD_AND_COMMIT_LOG_MODEL.md`
    + `deterministic shard routing`
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/SHARDING_CATALOG_SCHEMA.md`
    + `support sharding, cluster membership, and shard placement`
- Gap:
  - no complete Beta 2 lifecycle for shard placement, rebalancing, split/merge,
    read routing, ownership recovery, and cluster-visible operator controls
- Closing tickets:
  - `DQO-11-006`
  - `DQO-11-007`

## DQO-11-G04 OLAP support and cubes

- Status: partial_substrate
- Current bounded proof:
  - `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/OLAP_CUBE_CATALOG_SCHEMA.md`
    + `OLAP processing nodes and cube support`
  - `docs/specifications/18_Index_Framework/COLUMNSTORE_SPEC.md`
    + `scan acceleration, aggregation support, and summary pruning`
- Gap:
  - no integrated Beta 2 canon for analytical segment lifecycle, vectorized
    scan posture, cube matching, materialization and refresh, and analytical
    benchmark gates
- Closing tickets:
  - `DQO-11-008`
  - `DQO-11-009`

## Bounded scope rule

Only the four topic families above are in direct closure scope for this
package.

If research uncovers a prerequisite not already listed here:

- record it in `RISK_DECISION_LOG.md`
- tie it to the affected ticket
- do not silently expand the package into unrelated feature families
